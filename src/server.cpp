#include "irods/plugins/api/private/replica_truncate_common.hpp"
#include "irods/plugins/api/replica_truncate_common.h" // For API plugin number.

#include <irods/apiHandler.hpp>
#include <irods/irods_logger.hpp>
#include <irods/rodsErrorTable.h>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cstring> // For strdup.

namespace
{
	using log_api = irods::experimental::log::api;

	auto call_replica_truncate(irods::api_entry* _api, RsComm* _comm, const char* _input, char** _output) -> int
	{
		return _api->call_handler<const char*, char**>(_comm, _input, _output);
	} // call_replica_truncate

	auto rs_replica_truncate([[maybe_unused]] RsComm* _comm, const char* _input, char** _output) -> int
	{
		if (!_input || !_output) {
			*_output =
				strdup(nlohmann::json{{"message", "Received nullptr for input and/or output pointer."}}.dump().c_str());
			return USER__NULL_INPUT_ERR;
		}

		try {
			const auto input = nlohmann::json::parse(_input);

			if (!input.is_object()) {
				*_output = strdup(nlohmann::json{
					{"message", fmt::format("Expected input to be a JSON object. Received input: [{}]", _input)}}
				                      .dump()
				                      .c_str());
				return JSON_VALIDATION_ERROR;
			}

			const auto path_iter = input.find("path");
			if (input.end() == length_iter) {
				*_output = strdup(nlohmann::json{
					{"message", fmt::format("Input is missing 'path' key. Received input: [{}]", _input)}}
				                      .dump()
				                      .c_str());
				return JSON_VALIDATION_ERROR;
			}

			const auto length_iter = input.find("length");
			if (input.end() == length_iter) {
				*_output = strdup(nlohmann::json{
					{"message", fmt::format("Input is missing 'length' key. Received input: [{}]", _input)}}
				                      .dump()
				                      .c_str());
				return JSON_VALIDATION_ERROR;
			}

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
				return truncate_replica_with_target_resource(*path_iter, *length_iter, *target_resource_iter);
			}
			else if (input.end() != replica_number_iter) {
				return truncate_replica_with_replica_number(*path_iter, *length_iter, *replica_number_iter);
			}

			return truncate_replica(*path_iter, *length_iter);
		}
		catch (const nlohmann::json::parse_error& e) {
			*_output = strdup(
				nlohmann::json{{"message", fmt::format("Failed to parse input to JSON. Received input: [{}]", _input)}}
					.dump()
					.c_str());
			return JSON_VALIDATION_ERROR;
		}
		catch (...) {
			*_output =
				strdup(nlohmann::json{{"message", fmt::format("Unknown error occurred. Received input: [{}]", _input)}}
			               .dump()
			               .c_str());
			return SYS_UNKNOWN_ERR;
		}
	} // rs_replica_truncate
} //namespace

const operation_type op = rs_replica_truncate;
auto fn_ptr = reinterpret_cast<funcPtr>(call_replica_truncate);
