#include "irods/plugins/api/private/replica_truncate_common.hpp"
#include "irods/plugins/api/replica_truncate_common.h" // For API plugin number.

#include <irods/apiHandler.hpp>
#include <irods/data_object_proxy.hpp>
#include <irods/filesystem.hpp>
#include <irods/getRemoteZoneResc.h> // For REMOTE_OPEN.
#include <irods/irods_at_scope_exit.hpp>
#include <irods/irods_exception.hpp>
#include <irods/irods_file_object.hpp>
#include <irods/irods_logger.hpp>
#include <irods/irods_resource_backport.hpp>
#include <irods/irods_resource_redirect.hpp>
#include <irods/irods_rs_comm_query.hpp>
#include <irods/key_value_proxy.hpp>
#include <irods/modDataObjMeta.h>
#include <irods/rodsConnect.h>
#include <irods/rodsErrorTable.h>
#include <irods/rsFileTruncate.hpp>
#include <irods/rsModDataObjMeta.hpp>

#include <fmt/format.h>

#include <boost/make_shared.hpp> // Needed for irods::file_object_ptr, which is a boost::shared_ptr...

#include <cstring> // For strdup.
#include <string>
#include <string_view>

namespace
{
	using log_api = irods::experimental::log::api;
	namespace data_object = irods::experimental::data_object;
	namespace fs = irods::experimental::filesystem;

	auto call_replica_truncate(irods::api_entry* _api, RsComm* _comm, const DataObjInp* _input, BytesBuf** _output)
		-> int
	{
		return _api->call_handler<const DataObjInp*, BytesBuf**>(_comm, _input, _output);
	} // call_replica_truncate

	auto make_output_struct(const std::string_view& _message) -> BytesBuf*
	{
		const auto json_str = nlohmann::json{{"message", _message.data()}}.dump();

		log_api::info("json: [{}]", json_str);

		auto* output = static_cast<BytesBuf*>(std::malloc(sizeof(BytesBuf)));
		output->buf = strdup(json_str.c_str());
		output->len = static_cast<int>(std::strlen(json_str.c_str()) + 1);

		return output;
	} // make_output_struct

	auto truncate_physical_data(RsComm& _comm,
	                            const std::string_view _physical_path,
	                            const std::string_view _hierarchy,
	                            rodsLong_t _length) -> int
	{
		std::string location{};
		if (const auto ret = irods::get_loc_for_hier_string(_hierarchy.data(), location); !ret.ok()) {
			return static_cast<int>(ret.code());
		}

		fileOpenInp_t inp{};
		std::strncpy(inp.fileName, _physical_path.data(), MAX_NAME_LEN);
		std::strncpy(inp.resc_hier_, _hierarchy.data(), MAX_NAME_LEN);
		std::strncpy(inp.addr.hostAddr, location.c_str(), NAME_LEN);
		inp.dataSize = _length;

		return rsFileTruncate(&_comm, &inp);
	} // truncate_physical_data

	auto rs_replica_truncate(RsComm* _comm, const DataObjInp* _input, BytesBuf** _output) -> int
	{
		if (!_input || !_output) {
			*_output = make_output_struct(fmt::format(
				"Cannot truncate object [{}]: Received nullptr for input and/or output pointer.", _input->objPath));
			return SYS_INVALID_INPUT_PARAM;
		}

		try {
			rodsServerHost_t* remote_host{};
			// TODO const_cast is super not good -- may need to change interface to not have const
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wwritable-strings"
			// REMOTE_OPEN is a string literal being passed to a char*.
			const auto remote_flag =
				getAndConnRemoteZone(_comm, const_cast<DataObjInp*>(_input), &remote_host, REMOTE_OPEN);
#pragma clang diagnostic pop
			if (remote_flag < 0) {
				*_output = make_output_struct(fmt::format(
					"Cannot truncate object [{}]: Error occurred while determining whether to redirect to remote zone.",
					_input->objPath));
				return remote_flag;
			}

			// The data object is in a remote zone, so we need to redirect over there before continuing.
			if (remote_flag != LOCAL_HOST) {
				return procApiRequest(
					remote_host->conn,
					APN_REPLICA_TRUNCATE,
					_input,
					nullptr,
					reinterpret_cast<void**>(_output), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
					nullptr);
			}

			const auto cond_input = irods::experimental::make_key_value_proxy(_input->condInput);

			// Do not allow unprivileged users to use this keyword. If an unprivileged user is found to be attempting to
			// use admin_mode, a message is logged for the administrator.
			if (cond_input.contains(ADMIN_KW) && !irods::is_privileged_client(*_comm)) {
				const auto msg =
					fmt::format("Cannot truncate object [{}]: User [{}#{}] is not authorized to use [{}] keyword.",
				                _input->objPath,
				                _comm->clientUser.userName,
				                _comm->clientUser.rodsZone,
				                ADMIN_KW);

				log_api::warn("{}: {}", __func__, msg);

				*_output = make_output_struct(msg);

				return CAT_INSUFFICIENT_PRIVILEGE_LEVEL;
			}

			// Get the target_resource and replica_number options. Ensure that they are not being used at the same time
			// because they are incompatible parameters. They are incompatible parameters because they can contradict
			// one another as to what the user is instructing the API to do.
			if (cond_input.contains(RESC_NAME_KW) && cond_input.contains(REPL_NUM_KW)) {
				*_output = make_output_struct(
					fmt::format("Cannot truncate object [{}]: '{}' and '{}' are incompatible options.",
				                _input->objPath,
				                RESC_NAME_KW,
				                REPL_NUM_KW));
				return USER_INCOMPATIBLE_PARAMS;
			}

			// Now, onto the truncating.

			// boost::make_shared is used here because irods::file_object_ptr is a boost::shared_ptr.
			irods::file_object_ptr file_obj = boost::make_shared<irods::file_object>();
			file_obj->logical_path(_input->objPath);

			// This is only required for the file_object_factory, which is required for the resolve hierarchy
			// interface.
			DataObjInfo* data_obj_info{};
			irods::at_scope_exit free_data_object_info{[&data_obj_info] { freeAllDataObjInfo(data_obj_info); }};

			// TODO const_cast is super not good -- may need to change interface to not have const
			const auto fac_err =
				irods::file_object_factory(_comm, const_cast<DataObjInp*>(_input), file_obj, &data_obj_info);
			if (!fac_err.ok() || !data_obj_info) {
				*_output = make_output_struct(
					fmt::format("Cannot truncate object [{}]: Error occurred getting data object info.",
				                _input->objPath));
				                //fac_err.result()));
				return static_cast<int>(fac_err.code());
			}

			std::string hierarchy{};
			if (const auto hier_str = cond_input.find(RESC_HIER_STR_KW); hier_str == cond_input.cend()) {
				// Don't look too closely at this - may cause eye irritation.
				//log_api::debug("{}: Resolving hierarchy for [{}]. {}:[{}], {}:[{}]", __func__, _input->objPath,
				//RESC_HIER_STR_KW, _input->objPath);
				auto resolve_hierarchy_tuple = std::make_tuple(file_obj, fac_err);
				// TODO const_cast is super not good -- may need to change interface to not have const
				std::tie(file_obj, hierarchy) = irods::resolve_resource_hierarchy(
					_comm, irods::WRITE_OPERATION, *(const_cast<DataObjInp*>(_input)), resolve_hierarchy_tuple);
			}
			else {
				// Leave a note in the logs because this is technically bypassing policy despite being an iRODS pattern.
				log_api::info("{}: [{}] keyword used to bypass hierarchy resolution for [{}].",
				              __func__,
				              RESC_HIER_STR_KW,
				              _input->objPath);
				hierarchy = (*hier_str).value().data();
			}

			const auto target_object = data_object::make_data_object_proxy(*data_obj_info);
			const auto target_replica = data_object::find_replica(target_object, hierarchy);
			if (!target_replica) {
				*_output = make_output_struct(
					fmt::format("Cannot truncate object [{}]: No replica found in requested hierarchy [{}].",
				                _input->objPath,
				                hierarchy));
				return SYS_REPLICA_DOES_NOT_EXIST;
			}

#if 0
			// This would be handled by voting, so... there's not much to be done.
			// Check that the object is at rest ahead of time so that we can get a detailed message.
			if (!target_object.at_rest()) {
				*_output = make_output_struct(
					fmt::format("Cannot truncate object [{}]: Object is not at rest.", _input->objPath));
				return LOCKED_DATA_OBJECT_ACCESS;
			}
#endif

			// TODO if requested replica number or resource name is not the target replica, we should bail with an
			// error.

#if 0
			// I'm not even really sure whether this situation is possible. Leaving it here just in case.
			if (target_replica->resource() == BUNDLE_RESC) {
				*_output = make_output_struct(fmt::format(
					"Cannot truncate object [{}]: Replica targeted for truncate resides on [{}]. Skipping.", _input->objPath, BUNDLE_RESC));
				return 0;
			}
#endif

			// The old truncate API skipped updating the catalog when the object is in a special collection, and so
			// shall we. In fact, we should not touch the object at all in this case because it is unclear what to do.
			if (target_replica->special_collection_info()) {
				*_output = make_output_struct(
					fmt::format("Cannot truncate object [{}]: Object is in a special collection.", _input->objPath));
				return 0;
			}

			if (target_replica->size() == _input->dataSize) {
				// Why, it's already the requested size. Done!
				*_output = make_output_struct(fmt::format(
					"Replica of [{}] targeted for truncate already has size [{}].", _input->objPath, _input->dataSize));
				return 0;
			}

			// First, truncate the data...
			if (const auto ec = truncate_physical_data(
					*_comm, target_replica->physical_path(), target_replica->hierarchy(), _input->dataSize);
			    ec < 0)
			{
				if (const auto truncate_errno = getErrno(ec); ENOENT != truncate_errno && EACCES != truncate_errno) {
					return ec;
				}
				else {
					log_api::info("An error occurred, but I guess it was all okay in the end");
				}
			}

			// TODO Investigate use of CHKSUM_KW here...
			const auto [register_keywords, register_keywords_lm] = irods::experimental::make_key_value_proxy(
				{{ALL_REPL_STATUS_KW, ""}, {DATA_SIZE_KW, std::to_string(_input->dataSize)}, {CHKSUM_KW, ""}});

			ModDataObjMetaInp inp{target_replica->get(), register_keywords.get()};

			if (const int ec = rsModDataObjMeta(_comm, &inp); ec < 0) {
				*_output = make_output_struct(fmt::format("Error occurred updating replica information for [{}] "
				                                          "after truncate. Catalog may be inconsistent with data.",
				                                          _input->objPath));
				return ec;
			}

			return 0;
		}
		catch (const irods::exception& e) {
			*_output = make_output_struct(fmt::format("iRODS exception occurred: [{}]", e.client_display_what()));
			return static_cast<int>(e.code());
		}
		catch (const nlohmann::json::exception& e) {
			*_output = make_output_struct(fmt::format("JSON error occurred: [{}]", e.what()));
			return JSON_VALIDATION_ERROR;
		}
		catch (const std::exception& e) {
			*_output = make_output_struct(fmt::format("std::exception occurred: [{}]", e.what()));
			return SYS_INTERNAL_ERR;
		}
		catch (...) {
			*_output = make_output_struct("Unknown error occurred.");
			return SYS_UNKNOWN_ERROR;
		}
	} // rs_replica_truncate
} //namespace

const operation_type op = rs_replica_truncate;
auto fn_ptr = reinterpret_cast<funcPtr>(call_replica_truncate);
