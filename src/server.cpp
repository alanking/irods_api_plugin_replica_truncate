#include "irods/plugins/api/private/project_template_common.hpp"
#include "irods/plugins/api/project_template_common.h" // For API plugin number.

#include <irods/apiHandler.hpp>
#include <irods/catalog_utilities.hpp> // Requires linking against libnanodbc.so
#include <irods/irods_logger.hpp>
//#include <irods/irods_re_serialization.hpp>
#include <irods/rodsErrorTable.h>

#include <fmt/format.h>

#include <cstring> // For strdup.

namespace
{
	using log_api = irods::experimental::log::api;

	//
	// Function Prototypes
	//

	auto call_project_template(irods::api_entry*, RsComm*, const char*, char**) -> int;

	auto rs_project_template(RsComm*, const char*, char**) -> int;

	//
	// Function Implementations
	//

	auto call_project_template(irods::api_entry* _api, RsComm* _comm, const char* _msg, char** _resp) -> int
	{
		return _api->call_handler<const char*, char**>(_comm, _msg, _resp);
	} // call_project_template

	auto rs_project_template(RsComm* _comm, const char* _msg, char** _resp) -> int
	{
		if (!_msg || !_resp) {
			log_api::error("Inalid input: received nullptr for message pointer and/or response pointer.");
			return SYS_INVALID_INPUT_PARAM;
		}

		log_api::info("Project Template API received: [{}]", _msg);

		// Depending on the API's requirements, it may need to redirect to the provider.
		// This isn't necessary for this plugin, but we demonstrates how to do it anyway.
		try {
			namespace ic = irods::experimental::catalog;

			if (!ic::connected_to_catalog_provider(*_comm)) {
				log_api::trace("Redirecting request to catalog service provider.");

				auto* host_info = ic::redirect_to_catalog_provider(*_comm);

				return procApiRequest(
					host_info->conn,
					APN_PROJECT_TEMPLATE,
					_msg,
					nullptr,
					reinterpret_cast<void**>(_resp), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
					nullptr);
			}

			ic::throw_if_catalog_provider_service_role_is_invalid();
		}
		catch (const irods::exception& e) {
			log_api::error(e.what());
			return e.code();
		}

		// Echo the message back to the client.
		*_resp = strdup(fmt::format("YOUR MESSAGE: {}", _msg).c_str());

		return 0;
	} // rs_project_template
} //namespace

const operation_type op = rs_project_template;
auto fn_ptr = reinterpret_cast<funcPtr>(call_project_template);
