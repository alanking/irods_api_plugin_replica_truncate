#include "irods/plugins/api/rc_replica_truncate.h"

#include "irods/plugins/api/replica_truncate_common.h" // For API plugin number.

#include <irods/procApiRequest.h>
#include <irods/rodsErrorTable.h>

auto rc_replica_truncate(RcComm* _comm, DataObjInp* _input, BytesBuf** _output) -> int
{
	if (!_message || !_response) {
		return USER__NULL_INPUT_ERR;
	}

	return procApiRequest(_comm,
	                      APN_REPLICA_TRUNCATE,
	                      _input,
	                      nullptr,
	                      reinterpret_cast<void**>(_output), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	                      nullptr);
} // rc_replica_truncate
