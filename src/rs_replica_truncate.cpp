#include "irods/plugins/api/rs_replica_truncate.hpp"

#include "irods/plugins/api/replica_truncate_common.h" // For API plugin number.

#include <irods/rodsErrorTable.h>
#include <irods/irods_server_api_call.hpp>

auto rs_replica_truncate(RsComm* _comm, DataObjInp* _input, BytesBuf** _output) -> int
{
	if (!_message || !_response) {
		return USER__NULL_INPUT_ERR;
	}

	return irods::server_api_call_without_policy(APN_REPLICA_TRUNCATE, _comm, _input, _output);
} // rs_replica_truncate
