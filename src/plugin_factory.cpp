#include "irods/plugins/api/private/project_template_common.hpp"
#include "irods/plugins/api/project_template_common.h" // For API plugin number.

#include <irods/apiHandler.hpp>
#include <irods/client_api_allowlist.hpp>
#include <irods/rcMisc.h>
#include <irods/rodsPackInstruct.h>

// The plugin factory function must always be defined.
extern "C" auto plugin_factory(
	[[maybe_unused]] const std::string& _instance_name, // NOLINT(bugprone-easily-swappable-parameters)
	[[maybe_unused]] const std::string& _context) -> irods::api_entry*
{
#ifdef RODS_SERVER
	// If your API endpoint is designed to be invocable by non-admins, then you need to
	// add the API plugin number to the allowlist.
	irods::client_api_allowlist::add(APN_PROJECT_TEMPLATE);
#endif // RODS_SERVER

	// TODO We need to be able to add API plugin numbers to the API plugin number map.

	// clang-format off
	irods::apidef_t def{
		APN_PROJECT_TEMPLATE,
		const_cast<char*>(RODS_API_VERSION),
		NO_USER_AUTH,
		NO_USER_AUTH,
		"STR_PI",
		0,
		"STR_PI",
		0,
		op,
		"api_project_template",
		irods::clearInStruct_noop,
		irods::clearOutStruct_noop, // Remove this line if targeting a 4.3.0 server.
		fn_ptr
	};
	// clang-format on

	auto* api = new irods::api_entry{def}; // NOLINT(cppcoreguidelines-owning-memory)

	// TODO Demonstrate how to add new serialization types.

	api->in_pack_key = "STR_PI";
	api->in_pack_value = STR_PI;

	api->out_pack_key = "STR_PI";
	api->out_pack_value = STR_PI;

	return api;
} // plugin_factory
