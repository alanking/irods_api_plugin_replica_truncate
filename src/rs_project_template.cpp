#include "irods/plugins/api/rs_project_template.hpp"

#include "irods/plugins/api/project_template_common.h" // For API plugin number.

#include <irods/rodsErrorTable.h>
#include <irods/irods_server_api_call.hpp>

auto rs_project_template(RsComm* _comm, const char* _message, char** _response) -> int
{
	if (!_message || !_response) {
		return SYS_INVALID_INPUT_PARAM;
	}

	return irods::server_api_call_without_policy(APN_PROJECT_TEMPLATE, _comm, _message, _response);
} // rs_project_template
