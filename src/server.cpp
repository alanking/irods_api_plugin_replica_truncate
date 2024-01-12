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

	auto rs_replica_truncate(RsComm*, const char*, char**) -> int;

	auto call_replica_truncate(irods::api_entry* _api, RsComm* _comm, const char* _input, char** _output) -> int
	{
		return _api->call_handler<const char*, char**>(_comm, _input, _output);
	} // call_replica_truncate

	auto rs_replica_truncate(RsComm* _comm, const char* _input, char** _output) -> int
	{
		if (!_input || !_output) {
			log_api::error("{}: Received nullptr for input and/or output pointer.");
			return SYS_NULL_INPUT;
		}

		log_api::info("Project Template API received: [{}]", _input);

		*_output = strdup(fmt::format("YOUR MESSAGE: {}", _input).c_str());

		return 0;
	} // rs_replica_truncate
} //namespace

const operation_type op = rs_replica_truncate;
auto fn_ptr = reinterpret_cast<funcPtr>(call_replica_truncate);
