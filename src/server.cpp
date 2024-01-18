#include "irods/plugins/api/private/replica_truncate_common.hpp"
#include "irods/plugins/api/replica_truncate_common.h" // For API plugin number.

#include <irods/apiHandler.hpp>
#include <irods/at_scope_exit.hpp>
#include <irods/data_object_proxy.hpp>
#include <irods/filesystem.hpp>
#include <irods/irods_exception.hpp>
#include <irods/irods_logger.hpp>
#include <irods/rodsErrorTable.h>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cstring> // For strdup.
#include <string>
#include <string_view>

namespace
{
	using log_api = irods::experimental::log::api;
	namespace data_object = irods::experimental::data_object;
	namespace fs = irods::experimental::filesystem;

	struct truncate_input
	{
		std::string_view path;
		std::int32_t length;
		KeyValPair options;
	};

	auto call_replica_truncate(irods::api_entry* _api, RsComm* _comm, const char* _input, char** _output) -> int
	{
		return _api->call_handler<const char*, char**>(_comm, _input, _output);
	} // call_replica_truncate

	auto truncate_replica(RsComm& _comm, const truncate_input& _input, char** _output) -> int
	{
		const auto [obj, lm] = data_object::make_data_object_proxy(_comm, fs::path{_input.path.c_str()});

		if (obj.locked()) {
			*_output =
				strdup(nlohmann::json{{"message", "Failed to truncate [{}]: Object is locked.", obj.logical_path()}}
			               .dump()
			               .c_str());
			return LOCKED_DATA_OBJECT_ACCESS;
		}

		// TODO resolve resource hierarchy based on input

		// TODO find replica in obj based on resolved hierarchy

		// TODO skip bundleResc

		// TODO populate this struct???
		DataObjInp dataObjTruncateInp{};

		if (const int ec = l3Truncate(&_comm, dataObjTruncateInp, dataObjInfo); ec < 0) {
			if (const auto truncate_errno = getErrno(ec); ENOENT != truncate_errno && EACCES != truncate_errno) {
				return ec;
			}
		}

		if (dataObjInfo->specColl == NULL) {
			/* reigister the new size */

			memset(&regParam, 0, sizeof(regParam));
			memset(&modDataObjMetaInp, 0, sizeof(modDataObjMetaInp));

			snprintf(tmpStr, MAX_NAME_LEN, "%lld", dataObjTruncateInp->dataSize);
			addKeyVal(&regParam, DATA_SIZE_KW, tmpStr);
			addKeyVal(&regParam, CHKSUM_KW, "");

			modDataObjMetaInp.dataObjInfo = dataObjInfo;
			modDataObjMetaInp.regParam = &regParam;
			status = rsModDataObjMeta(rsComm, &modDataObjMetaInp);
			clearKeyVal(&regParam);
			if (status < 0) {
				rodsLog(LOG_NOTICE,
				        "dataObjTruncateS: rsModDataObjMeta error for %s. status = %d",
				        dataObjTruncateInp->objPath,
				        status);
			}
		}

		return 0;
	} // truncate_replica

	auto rs_replica_truncate(RsComm* _comm, const char* _input, char** _output) -> int
	{
		// Like, obviously.
		if (!_input || !_output) {
			*_output =
				strdup(nlohmann::json{{"message", "Received nullptr for input and/or output pointer."}}.dump().c_str());
			return USER__NULL_INPUT_ERR;
		}

		try {
			// Parse the input string into JSON. If the parsing is unsuccessful, a json::exception will occur. If the
			// parsed JSON is not a JSON object, return an error because the input needs to be a JSON object.
			const auto input = nlohmann::json::parse(_input);
			if (!input.is_object()) {
				*_output = strdup(nlohmann::json{
					{"message", fmt::format("Expected input to be a JSON object. Received input: [{}]", _input)}}
				                      .dump()
				                      .c_str());
				return JSON_VALIDATION_ERROR;
			}

			// path and length are required inputs. Therefore, we access these elements directly with at() because if
			// either is not present in the input structure, a json::exception will occur. Do this first because there
			// is not point in doing all the other stuff if these aren't present.
			const auto& path = input.at("path").get_ref<const std::string&>();
			const auto length = input.at("length").get<const std::int32_t>();

			// Determine whether it is required to redirect to a remote zone to truncate the replica.
			DataObjInp input{};
			// TODO check length of path to see if it will be truncated
			std::strncpy(input.objPath, path.c_str(), sizeof(DataObjInp::objPath) - 1);
			rodsServerHost_t* remote_host{};
			const auto remote_flag = getAndConnRemoteZone(&_comm, &input, &remote_host, REMOTE_CREATE);
			if (remote_flag < 0) {
				*_output = strdup(nlohmann::json{
					{"message",
				     fmt::format("Error occurred while determining whether to redirect to remote zone for path [{}].",
				                 path.c_str())}}
				                      .dump()
				                      .c_str());
				return remote_flag;
			}

			// The data object is in a remote zone, so we need to redirect over there before continuing.
			if (remote_flag != LOCAL_HOST) {
				return rc_replica_truncate(remote_host->conn, _input, _output);
			}

			// Internally, we use an input structure to keep track of all the inputs once everything has been validated
			// up front in this function. After that, the supplied input is used to populate the structure.
			truncate_input inp{};
			const auto clear_key_value_pair = irods::at_scope_exit{[&inp] { clearKeyVal(&inp.options); }};

			// These values were extracted from the JSON input above.
			inp.path = path.c_str();
			inp.length = length;

			// Find the admin_mode option and include the appropriate keyword if found. Do not allow unprivileged users
			// to use this keyword. If an unprivileged user is found to be attempting to use admin_mode, a message is
			// logged for the administrator.
			const auto admin_mode_iter = input.find("admin_mode");
			if (input.end() != admin_iter && admin_iter->get<bool>()) {
				if (!irods::is_privileged_client(*_comm)) {
					const auto msg = fmt::format("User [{}#{}] is not authorized to use the admin keyword.",
					                             _comm->clientUser.userName,
					                             _comm->clientUser.rodsZone);

					log_api::warn("{}: {}", __func__, msg);

					*_output = strdup(nlohmann::json{{"message", msg}}.dump().c_str());

					return CAT_INSUFFICIENT_PRIVILEGE_LEVEL;
				}

				addKeyVal(&inp.options, ADMIN_KW, "");
			}

			// Get the target_resource and replica_number options. Ensure that they are not being used at the same time
			// because they are incompatible parameters. They are incompatible parameters because they can contradict
			// one another as to what the user is instructing the API to do.
			const auto target_resource_iter = input.find("target_resource");
			const auto replica_number_iter = input.find("replica_number");
			if (input.end() != target_resource_iter && input.end() != replica_number_iter) {
				*_output = strdup(
					nlohmann::json{{"message", "'target_resource' and 'replica_number' are incompatible options."}}
						.dump()
						.c_str());
				return USER_INCOMPATIBLE_PARAMS;
			}

			if (input.end() != target_resource_iter) {
				addKeyVal(&inp.options, RESC_NAME_KW, target_resource_iter->get_ref<const std::string&>().c_str());
			}

			if (input.end() != replica_number_iter) {
				addKeyVal(&inp.options, REPL_NUM_KW, replica_number_iter->get_ref<const std::string&>().c_str());
			}

			return truncate_replica(*_comm, inp, _output);
		}
		catch (const irods::exception& e) {
			*_output = strdup(
				nlohmann::json{{"message", fmt::format("iRODS exception occurred: [{}]", e.client_display_what())}}
					.dump()
					.c_str());
			return e.code();
		}
		catch (const nlohmann::json::out_of_range& e) {
			*_output = strdup(
				nlohmann::json{{"message", fmt::format("Failed to find some member in JSON input: [{}]", e.what())}}
					.dump()
					.c_str());
			return JSON_VALIDATION_ERROR;
		}
		catch (const nlohmann::json::parse_error& e) {
			*_output = strdup(nlohmann::json{{"message", fmt::format("Failed to parse input to JSON: [{}]", e.what())}}
			                      .dump()
			                      .c_str());
			return JSON_VALIDATION_ERROR;
		}
		catch (const nlohmann::json::type_error& e) {
			*_output = strdup(nlohmann::json{{"message", fmt::format("Unexpected type in JSON input: [{}]", e.what())}}
			                      .dump()
			                      .c_str());
			return JSON_VALIDATION_ERROR;
		}
		catch (const std::exception& e) {
			*_output = strdup(
				nlohmann::json{{"message", fmt::format("std::exception occurred: [{}]", e.what())}}.dump().c_str());
			return SYS_INTERNAL_ERR;
		}
		catch (...) {
			*_output = strdup(nlohmann::json{{"message", "Unknown error occurred."}}.dump().c_str());
			return SYS_UNKNOWN_ERR;
		}
	} // rs_replica_truncate
} //namespace

const operation_type op = rs_replica_truncate;
auto fn_ptr = reinterpret_cast<funcPtr>(call_replica_truncate);
