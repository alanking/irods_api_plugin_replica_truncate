#! /bin/bash -e

set -x

usage() {
cat <<_EOF_
Builds packages for the replica_truncate API plugin.

Available options:

    -d, --debug             Build with symbols for debugging.
    -N, --ninja             Use ninja builder as the make tool.
    -j, --jobs              Number of jobs for make tool.
    --exclude-unit-tests    Indicates that unit tests should not be built.
    --custom-externals      Path to custom externals packages received via volume mount.
    --source-path           Path to the directory for source code in the filesystem local to this script.
    --build-path            Path to the directory for build artifacts in the filesystem local to this script.
    -h, --help              This message.
_EOF_
    exit
}

if [[ -z ${package_manager} ]] ; then
    echo "\$package_manager not defined"
    exit 1
fi

if [[ -z ${file_extension} ]] ; then
    echo "\$file_extension not defined"
    exit 1
fi

supported_package_manager_frontends=(
    "apt-get"
    "yum"
    "dnf"
)

if [[ ! " ${supported_package_manager_frontends[*]} " =~ " ${package_manager} " ]]; then
    echo "unsupported platform or package manager"
    exit 1
fi

install_packages() {
    if [ "${package_manager}" == "apt-get" ] ; then
        apt-get update
        dpkg -i "$@" || true
        apt-get install -fy --allow-downgrades
        for pkg_file in "$@" ; do
            pkg_name="$(dpkg-deb --field "${pkg_file}" Package)"
            pkg_arch="$(dpkg-deb --field "${pkg_file}" Architecture)"
            pkg_vers="$(dpkg-deb --field "${pkg_file}" Version)"

            pkg_vers_inst="$(dpkg-query  --showformat='${Version}' --show "${pkg_name}:${pkg_arch}")" ||
                (echo "could not verify installation of ${pkg_file}" && exit 1)
            if [ "${pkg_vers}" == "${pkg_vers_inst}" ] ; then
                echo "installed version of ${pkg_name}:${pkg_arch} does not match ${pkg_file}"
            fi
        done
    elif [ "${package_manager}" == "yum" ] ; then
        yum install -y "$@"
    elif [ "${package_manager}" == "dnf" ] ; then
        dnf install -y "$@"
    fi
}

make_program="make"
make_program_config=""
build_jobs=0
debug_config="-DCMAKE_BUILD_TYPE=Release"
unit_test_config="-DIRODS_UNIT_TESTS_BUILD=YES"
custom_externals=""
source_path="/src"
build_path="/build"

common_cmake_args=(
    -DCMAKE_COLOR_MAKEFILE=ON
    -DCMAKE_VERBOSE_MAKEFILE=ON
)

while [ -n "$1" ] ; do
    case "$1" in
        -N|--ninja)                   make_program_config="-GNinja";
                                      make_program="ninja";;
        -j|--jobs)                    shift; build_jobs=$(($1 + 0));;
        -d|--debug)                   debug_config="-DCMAKE_BUILD_TYPE=Debug -DCPACK_DEBIAN_COMPRESSION_TYPE=none";;
        --exclude-unit-tests)         unit_test_config="-DIRODS_UNIT_TESTS_BUILD=NO";;
        --custom-externals)           shift; custom_externals=$1;;
        --source-path)                shift; source_path=$1;;
        --build-path)                 shift; build_path=$1;;
        -h|--help)                    usage;;
    esac
    shift
done

# Set up directories for source and build.
if [[ ! -d "${source_path}" ]] ; then
    echo "No directory found for path to source: [${source_path}]"
	exit 1
fi

if [[ ! -d "${build_path}" ]]; then
    mkdir -p "${build_path}"
fi

cd "${build_path}"

# Run CMake
cmake ${make_program_config} ${debug_config} "${common_cmake_args[@]}" ${unit_test_config} ${msi_test_config} "${source_path}"

# Build
build_jobs=$(( !build_jobs ? $(nproc) - 1 : build_jobs )) #prevent maxing out CPUs

if [[ -z ${build_jobs} ]] ; then
    ${make_program} package
else
    echo "using [${build_jobs}] threads"
    ${make_program} -j ${build_jobs} package
fi
