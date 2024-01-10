#include "irods/plugins/api/rc_project_template.h"

#include "irods/plugins/api/project_template_common.h" // For API plugin number.

#include <irods/procApiRequest.h>
#include <irods/rodsErrorTable.h>

auto rc_project_template(RcComm* _comm, const char* _message, char** _response) -> int
{
	if (!_message || !_response) {
		return SYS_INVALID_INPUT_PARAM;
	}

	return procApiRequest(_comm,
	                      APN_PROJECT_TEMPLATE,
	                      const_cast<char*>(_message), // NOLINT(cppcoreguidelines-pro-type-const-cast)
	                      nullptr,
	                      reinterpret_cast<void**>(_response), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	                      nullptr);
} // rc_project_template
