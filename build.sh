#!/bin/bash

print_manual()
{
	EscChar="\033"
	ColorReset="${EscChar}[m"
	RedColor="${EscChar}[31;1m"
	GreenColor="${EscChar}[32;1m"
	YellowColor="${EscChar}[33;1m"
	YellowColor2="${EscChar}[33m"
	BlueColor="${EscChar}[34;1m"
	CyanColor="${EscChar}[36;1m"

	printf "\n${GreenColor}Required environmental options:${ColorReset}"
	printf "\n${YellowColor}PLATFORM${ColorReset} - one of the supported targets: osx|windows|linux|rpi"
	printf "\n${YellowColor}DOCKER_TAG${ColorReset} | ${YellowColor}DOCKER_IMAGE${ColorReset} - both are required only for linux|rpi platforms:"

	printf "\n   Debian => ${YellowColor2}bullseye${ColorReset} | ${YellowColor2}x86_64${ColorReset}"
	printf "\n   Debian => ${YellowColor2}bullseye${ColorReset} | ${YellowColor2}arm-32bit-armv6l${ColorReset}"
	printf "\n   Debian => ${YellowColor2}bullseye${ColorReset} | ${YellowColor2}arm-64bit-aarch64${ColorReset}"
	printf "\n   Debian => ${YellowColor2}bookworm${ColorReset} | ${YellowColor2}x86_64${ColorReset}"
	printf "\n   Debian => ${YellowColor2}bookworm${ColorReset} | ${YellowColor2}arm-32bit-armv6l${ColorReset}"
	printf "\n   Debian => ${YellowColor2}bookworm${ColorReset} | ${YellowColor2}arm-64bit-aarch64${ColorReset}"
	printf "\n   Ubuntu => ${YellowColor2}jammy${ColorReset} | ${YellowColor2}x86_64${ColorReset}"
	printf "\n   Ubuntu => ${YellowColor2}noble${ColorReset} | ${YellowColor2}x86_64${ColorReset}"
	printf "\n   Fedora => ${YellowColor2}Fedora_40${ColorReset} | ${YellowColor2}x86_64${ColorReset}"
	printf "\n   ArchLinux => ${YellowColor2}ArchLinux${ColorReset} | ${YellowColor2}x86_64${ColorReset}"

	printf "\n\n${GreenColor}Optional environmental options:${ColorReset}"
	printf "\n${CyanColor}BUILD_TYPE${ColorReset} - Release|Debug, default is Release version"
	printf "\n${CyanColor}BUILD_ARCHIVES${ColorReset} - false|true, cpack will build ZIP package"
	printf "\n${CyanColor}USE_STANDARD_INSTALLER_NAME${ColorReset} - false|true, use standard Linux package naming"
	printf "\n${CyanColor}USE_CCACHE${ColorReset} - false|true, use ccache if available"
	printf "\n${CyanColor}RESET_CACHE${ColorReset} - false|true, reset ccache storage"
	printf "\n\n${GreenColor}Example of usage:${ColorReset}\n${YellowColor}PLATFORM=rpi DOCKER_TAG=bullseye DOCKER_IMAGE=arm-64bit-aarch64 ./build.sh${ColorReset}"
	printf "\nInstallers from Docker builds will be ready in the ${RedColor}deploy${ColorReset} folder"
	printf "\n\n"
	exit 0
}

if [[ "$PLATFORM" == "" || ( ("$PLATFORM" == "linux" || "$PLATFORM" == "rpi") && ( "$DOCKER_IMAGE" = "" || "$DOCKER_TAG" = "" ) ) ]]; then
	print_manual
fi

# detect CI
if [ "$SYSTEM_COLLECTIONID" != "" ]; then
	# Azure Pipelines
	echo "Azure detected"
	CI_NAME="$(echo "$AGENT_OS" | tr '[:upper:]' '[:lower:]')"
	CI_BUILD_DIR="$BUILD_SOURCESDIRECTORY"
	CI_TYPE="azure"
elif [ "$GITHUB_ACTIONS" != "" ]; then
	# GitHub Actions
	echo "Github Actions detected"
	CI_NAME="$(uname -s | tr '[:upper:]' '[:lower:]')"
	CI_BUILD_DIR="$GITHUB_WORKSPACE"
	CI_TYPE="github_action"
else
	# for executing in non ci environment
	echo "Local system build detected"
	CI_NAME="$(uname -s | tr '[:upper:]' '[:lower:]')"
	CI_TYPE="other"
	CI_BUILD_DIR="$PWD"
fi

# set environment variables if not exists
[ -z "${BUILD_TYPE}" ] && BUILD_TYPE="Release"
[ -z "${USE_STANDARD_INSTALLER_NAME}" ] && USE_STANDARD_INSTALLER_NAME=false
[ -z "${USE_CCACHE}" ] && USE_CCACHE=true
[ -z "${RESET_CACHE}" ] && RESET_CACHE=false
[ -z "${BUILD_ARCHIVES}" ] && BUILD_ARCHIVES=true


printf "\nPLATFORM = %s" ${PLATFORM}
printf "\nDOCKER_TAG = %s" ${DOCKER_TAG}
printf "\nDOCKER_IMAGE = %s" ${DOCKER_IMAGE}
printf "\nBUILD_TYPE = %s" ${BUILD_TYPE}
printf "\nBUILD_ARCHIVES = %s" ${BUILD_ARCHIVES}
printf "\nUSE_STANDARD_INSTALLER_NAME = %s" ${USE_STANDARD_INSTALLER_NAME}
printf "\nUSE_CCACHE = %s" ${USE_CCACHE}
printf "\nRESET_CACHE = %s" ${RESET_CACHE}
printf "\n"

if [ ${BUILD_ARCHIVES} = true ]; then
	echo "Build the package archive"
	ARCHIVE_OPTION=" -DBUILD_ARCHIVES=ON"	
else
	echo "Do not build the package archive"
	ARCHIVE_OPTION=" -DBUILD_ARCHIVES=OFF"
fi

if [ ${USE_STANDARD_INSTALLER_NAME} = true ]; then
	echo "Use standard naming"
	ARCHIVE_OPTION=" ${ARCHIVE_OPTION} -DUSE_STANDARD_INSTALLER_NAME=ON"	
else
	echo "Do not use standard naming"
	ARCHIVE_OPTION=" ${ARCHIVE_OPTION} -DUSE_STANDARD_INSTALLER_NAME=OFF"
fi

echo "Platform: ${PLATFORM}, build type: ${BUILD_TYPE}, CI_NAME: $CI_NAME, docker image: ${DOCKER_IMAGE}, docker type: ${DOCKER_TAG}, archive options: ${ARCHIVE_OPTION}, use ccache: ${USE_CCACHE}, reset ccache: ${RESET_CACHE}"

# clear ccache if neccesery
if [ ${RESET_CACHE} = true ]; then
	echo "Clearing ccache"
	rm -rf .ccache || true
	rm -rf build/.ccache || true
fi

# Build the package on osx or linux
if [[ "$CI_NAME" == 'osx' || "$CI_NAME" == 'darwin' ]]; then
	echo "Start: osx or darwin"

	if [ ${USE_CCACHE} = true ]; then
		echo "Using ccache"		
		if [[ $(uname -m) == 'arm64' ]]; then
			BUILD_OPTION=""
		else
			BUILD_OPTION="-DUSE_PRECOMPILED_HEADERS=OFF"
			export CCACHE_COMPILERCHECK=content
		fi
	else
		echo "Not using ccache"
		BUILD_OPTION="-DUSE_CCACHE_CACHING=OFF"
	fi

	echo "Build option: ${BUILD_OPTION}"

	mkdir -p build/.ccache
	ls -a build/.ccache
	cd build
	ccache -z -d ./.ccache || true
	cmake -DPLATFORM=${PLATFORM} ${BUILD_OPTION} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 -DCMAKE_INSTALL_PREFIX:PATH=/usr/local ../ || exit 2
	make -j $(sysctl -n hw.ncpu) || exit 3
	sudo cpack || exit 3
	ccache -sv -d ./.ccache || true
	exit 0;
	exit 1 || { echo "---> HyperHDR compilation failed! Abort"; exit 5; }

elif [[ $CI_NAME == *"mingw64_nt"* || "$CI_NAME" == 'windows_nt' ]]; then
	echo "Start: windows"	
	echo "Number of cores: $NUMBER_OF_PROCESSORS"

	if [ ${USE_CCACHE} = true ]; then
		echo "Using ccache"
		BUILD_OPTION="${ARCHIVE_OPTION}"
	else
		echo "Not using ccache"
		BUILD_OPTION="-DUSE_CCACHE_CACHING=OFF ${ARCHIVE_OPTION}"
	fi

	if [[ $CI_TYPE == "github_action" ]]; then
		export CCACHE_COMPILERCHECK=content
		export CCACHE_NOCOMPRESS=true
		BUILD_OPTION="${BUILD_OPTION} -DCMAKE_GITHUB_ACTION=ON"
	else
		BUILD_OPTION="${BUILD_OPTION} -DCMAKE_GITHUB_ACTION=OFF"
	fi

	echo "Build option: ${BUILD_OPTION}"

	mkdir -p build/.ccache

	cd build
	cmake -G "Visual Studio 17 2022" ${BUILD_OPTION} -A x64 -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ../ || exit 2
	./ccache.exe -zp || true
	cmake --build . --target package --config Release -- -nologo -v:m -maxcpucount || exit 3
	./ccache.exe -sv || true

	exit 0;

elif [[ "$CI_NAME" == 'linux' ]]; then
	echo "Compile Hyperhdr with DOCKER_IMAGE = ${DOCKER_IMAGE}, DOCKER_TAG = ${DOCKER_TAG} and friendly name DOCKER_NAME = ${DOCKER_NAME}"
	
	# set GitHub Container Registry url
	REGISTRY_URL="ghcr.io/awawa-dev/${DOCKER_IMAGE}"
	
	# take ownership of deploy dir
	mkdir -p ${CI_BUILD_DIR}/deploy
	mkdir -p .ccache

	if [ ${USE_CCACHE} = true ]; then
		echo "Using ccache"
		BUILD_OPTION="${ARCHIVE_OPTION}"
		cache_env="export CCACHE_DIR=/.ccache && ccache -z"
		ls -a .ccache
	else
		echo "Not using ccache"		
		BUILD_OPTION="-DUSE_CCACHE_CACHING=OFF ${ARCHIVE_OPTION}"
		cache_env="true"
	fi
	
	echo "Build option: ${BUILD_OPTION}, ccache: ${cache_env}"

	if [[ "$DOCKER_TAG" == "ArchLinux" ]]; then
		echo "Arch Linux detected"
		cp cmake/linux/arch/* .
		chmod -R a+rw ${CI_BUILD_DIR}/deploy
		versionFile=`cat version`
		executeCommand="echo \"GLIBC version: \$(ldd --version | head -1 | sed 's/[^0-9]*\([.0-9]*\)$/\1/')\""
		executeCommand=${executeCommand}" && sed -i \"s/{GLIBC_VERSION}/\$(ldd --version | head -1 | sed 's/[^0-9]*\([.0-9]*\)$/\1/')/\" PKGBUILD && makepkg"
		echo ${executeCommand}
		sed -i "s/{VERSION}/${versionFile}/" PKGBUILD
		if [ ${USE_CCACHE} = true ]; then
			sed -i "s/{BUILD_OPTION}/${BUILD_OPTION} -DUSE_PRECOMPILED_HEADERS=OFF/" PKGBUILD
		else
			sed -i "s/{BUILD_OPTION}/${BUILD_OPTION}/" PKGBUILD
		fi
		chmod -R a+rw ${CI_BUILD_DIR}/.ccache
	else
		executeCommand="cd build && ( cmake ${BUILD_OPTION} -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DDEBIAN_NAME_TAG=${DOCKER_TAG} ../ || exit 2 )"
		executeCommand+=" && ( make -j $(nproc) package || exit 3 )"
	fi

	# run docker
	docker run --rm \
	-v "${CI_BUILD_DIR}/.ccache:/.ccache" \
	-v "${CI_BUILD_DIR}/deploy:/deploy" \
	-v "${CI_BUILD_DIR}:/source:ro" \
	$REGISTRY_URL:$DOCKER_TAG \
	/bin/bash -c "${cache_env} && cd / && mkdir -p hyperhdr && cp -rf /source/. /hyperhdr &&
	cd /hyperhdr && mkdir build && (${executeCommand}) &&
	(cp /hyperhdr/build/bin/h* /deploy/ 2>/dev/null || : ) &&
	(cp /hyperhdr/build/Hyper* /deploy/ 2>/dev/null || : ) &&
	(cp /hyperhdr/Hyper*.zst /deploy/ 2>/dev/null || : ) &&
	(ccache -sv || true) &&
	exit 0;
	exit 1 " || { echo "---> HyperHDR compilation failed! Abort"; exit 5; }
	
	# overwrite file owner to current user
	sudo chown -fR $(stat -c "%U:%G" ${CI_BUILD_DIR}/deploy) ${CI_BUILD_DIR}/deploy
fi
