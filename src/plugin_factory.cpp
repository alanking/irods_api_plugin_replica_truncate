#include "irods/plugins/api/private/replica_truncate_common.hpp"
#include "irods/plugins/api/replica_truncate_common.h" // For API plugin number.

#include <irods/apiHandler.hpp>
#include <irods/client_api_allowlist.hpp>
#include <irods/rcMisc.h>
#include <irods/rodsPackInstruct.h>

extern "C" auto plugin_factory(
	[[maybe_unused]] const std::string& _instance_name, // NOLINT(bugprone-easily-swappable-parameters)
	[[maybe_unused]] const std::string& _context) -> irods::api_entry*
{
#ifdef RODS_SERVER
	irods::client_api_allowlist::add(APN_REPLICA_TRUNCATE);
#endif // RODS_SERVER

	// clang-format off
	irods::apidef_t def{
		APN_REPLICA_TRUNCATE,
		const_cast<char*>(RODS_API_VERSION),
		NO_USER_AUTH,
		NO_USER_AUTH,
		"STR_PI",
		0,
		"STR_PI",
		0,
		op,
		"api_replica_truncate",
		irods::clearInStruct_noop,
		irods::clearOutStruct_noop,
		fn_ptr
	};
	// clang-format on

	auto* api = new irods::api_entry{def}; // NOLINT(cppcoreguidelines-owning-memory)

	api->in_pack_key = "STR_PI";
	api->in_pack_value = STR_PI;

	api->out_pack_key = "STR_PI";
	api->out_pack_value = STR_PI;

	return api;
} // plugin_factory
