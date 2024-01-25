//#include "irods/plugins/api/replica_truncate_common.h"

#include <irods/client_connection.hpp>
#include <irods/irods_at_scope_exit.hpp>
#include <irods/irods_exception.hpp>
#include <irods/key_value_proxy.hpp>
#include <irods/procApiRequest.h>
#include <irods/rcMisc.h>	 // For set_ips_display_name()
#include <irods/rodsClient.h> // For load_client_api_plugins()

#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <optional>

namespace
{
	auto canonical(const std::string& path, rodsEnv& env) -> std::optional<std::string>
	{
		rodsPath_t input{};
		rstrcpy(input.inPath, path.data(), MAX_NAME_LEN);

		if (parseRodsPath(&input, &env) != 0) {
			return std::nullopt;
		}

		auto* escaped_path = escape_path(input.outPath);
		std::optional<std::string> p = escaped_path;
		std::free(escaped_path);

		return p;
	} // canonical
} // anonymous namespace

auto print_usage_info() -> void;

int main(int _argc, char* _argv[]) // NOLINT(modernize-use-trailing-return-type)
{
	set_ips_display_name("itruncate");

	namespace po = boost::program_options;

	po::options_description desc{""};

	// clang-format off
	desc.add_options()
		("size,s", po::value<std::size_t>(), "")
		("resource,R", po::value<std::string>(), "")
		("replica-number,n", po::value<int>(), "")
		("admin-mode,M", po::value<bool>()->default_value(false), "")
		("logical_path", po::value<std::string>(), "") // positional option
		("help,h", "");
	// clang-format on

	po::positional_options_description pod;
	pod.add("logical_path", 1);

	load_client_api_plugins();

	try {
		po::variables_map vm;
		po::store(po::command_line_parser(_argc, _argv).options(desc).positional(pod).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			print_usage_info();
			return 0;
		}

		DataObjInp input{};
		auto [cond_input, cond_input_lm] = irods::experimental::make_key_value_proxy();

		if (vm.count("logical_path") == 0) {
			fmt::print(stderr, "error: Missing LOGICAL_PATH\n");
			return 1;
		}

		auto input_logical_path = vm["logical_path"].as<std::string>();

		if (input_logical_path.empty()) {
			fmt::print(stderr, "error: Missing LOGICAL_PATH\n");
			return 1;
		}

		if (vm.count("size") == 0) {
			fmt::print(stderr, "error: Missing --size\n");
			return 1;
		}

        rodsEnv env;
        if (getRodsEnv(&env) < 0) {
            fmt::print(stderr, "Error: Could not get iRODS environment.\n");
            return 1;
        }

		if (const auto maybe_logical_path = canonical(input_logical_path, env); maybe_logical_path) {
			// Minus 1 to allow the last character to be a null character.
			std::strncpy(input.objPath, maybe_logical_path->c_str(), sizeof(DataObjInp::objPath) - 1);
		}
		else {
			fmt::print(stderr, "error: LOGICAL_PATH could not be made an absolute path.\n");
        	return 1;
    	}

		if (vm["admin-mode"].as<bool>()) {
			cond_input[ADMIN_KW] = "";
		}

		if (vm.count("resource")) {
			cond_input[RESC_NAME_KW] = vm["resource"].as<std::string>();
		}

		if (vm.count("replica-number")) {
			cond_input[REPL_NUM_KW] = vm["replica-number"].as<std::string>();
		}

		irods::experimental::client_connection conn;

		BytesBuf* output{};
		irods::at_scope_exit free_output{[&output] {
			clearBytesBuffer(output);
		}};

		const auto ec =
			procApiRequest(static_cast<RcComm*>(conn),
						   1'000'444, //APN_REPLICA_TRUNCATE,
						   &input,
						   nullptr,
						   reinterpret_cast<void**>(&output), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
						   nullptr);

		if (output && output->len > 0) {
			const auto* output_str = static_cast<char*>(output->buf);
			const auto& message = nlohmann::json::parse(output_str).at("message").get<const std::string>();
			fmt::print(stdout, "{}\n", message);
		}

		if (ec != 0) {
			fmt::print(stderr, "error: {}\n", ec);
			return 1;
		}

		return 0;
	}
	catch (const irods::exception& e) {
		fmt::print(stderr, "error: {}\n", e.client_display_what());
	}
	catch (const std::exception& e) {
		fmt::print(stderr, "error: {}\n", e.what());
	}

	return 1;
} // main

auto print_usage_info() -> void
{
	fmt::print(R"_(itruncate - Truncate a replica

Usage: itruncate [OPTIONS]... LOGICAL_PATH

Queries the iRODS Catalog using GenQuery2.
Truncates a replica of the specified data object at LOGICAL_PATH to the specified size in bytes.

LOGICAL_PATH must refer to an existing, at-rest data object.

Options:
  -s, --size=SIZE_IN_BYTES
  		Set the file size to SIZE bytes.

  -R, --resource=ROOT_RESOURCE
		Specify the root resource of the hierarchy with the replica to target.
		Incompatible with -n.

  -n, --replica-number=REPLICA_NUMBER
		Specify the number of the replica to target. Incompatible with -R.

  -M, --admin-mode
		If specified, execute with elevated privileges. Can only be used by rodsadmins.

  -h, --help
		Display this help message and exit.
)_");

	char name[] = "iquery (experimental)";
	printReleaseInfo(name);
} // print_usage_info
