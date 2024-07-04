#!/bin/bash

# detect CI
if [ "$SYSTEM_COLLECTIONID" != "" ]; then
	# Azure Pipelines
	echo "Azure detected"
	CI_NAME="$(echo "$AGENT_OS" | tr '[:upper:]' '[:lower:]')"
	CI_BUILD_DIR="$BUILD_SOURCESDIRECTORY"
	CI_TYPE="azure"
elif [ "$HOME" != "" ]; then
	# GitHub Actions
	echo "Github Actions detected"
	CI_NAME="$(uname -s | tr '[:upper:]' '[:lower:]')"
	CI_BUILD_DIR="$GITHUB_WORKSPACE"
	CI_TYPE="github_action"
else
	# for executing in non ci environment
	CI_NAME="$(uname -s | tr '[:upper:]' '[:lower:]')"
	CI_TYPE="other"
fi

if [ ${BUILD_ARCHIVES} = true ]; then
	echo "Build the package archive"
	IS_ARCHIVE_SKIPPED=" -DDO_NOT_BUILD_ARCHIVES=OFF"	
else
	echo "Do not build the package archive"
	IS_ARCHIVE_SKIPPED=" -DDO_NOT_BUILD_ARCHIVES=ON"
fi

# set environment variables if not exists (debug)
[ -z "${BUILD_TYPE}" ] && BUILD_TYPE="Release"

[ -z "${USE_STANDARD_INSTALLER_NAME}" ] && USE_STANDARD_INSTALLER_NAME="OFF"

echo "Platform: ${PLATFORM}, build type: ${BUILD_TYPE}, CI_NAME: $CI_NAME, docker image: ${DOCKER_IMAGE}, docker type: ${DOCKER_TAG}, is archive enabled: ${IS_ARCHIVE_SKIPPED}, use ccache: ${USE_CCACHE}, reset ccache: ${RESET_CACHE}"

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
		BUILD_OPTION="${IS_ARCHIVE_SKIPPED}"
	else
		echo "Not using ccache"
		BUILD_OPTION="-DUSE_CCACHE_CACHING=OFF ${IS_ARCHIVE_SKIPPED}"
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
		BUILD_OPTION="${IS_ARCHIVE_SKIPPED}"
		cache_env="export CCACHE_DIR=/.ccache && ccache -z"
		ls -a .ccache
	else
		echo "Not using ccache"		
		BUILD_OPTION="-DUSE_CCACHE_CACHING=OFF ${IS_ARCHIVE_SKIPPED}"
		cache_env="true"
	fi
	
	echo "Build option: ${BUILD_OPTION}, ccache: ${cache_env}"

	if [[ "$DOCKER_TAG" == "ArchLinux" ]]; then
		echo "Arch Linux detected"
		cp cmake/linux/arch/* .
		executeCommand="makepkg"
		chmod -R a+rw ${CI_BUILD_DIR}/deploy
		versionFile=`cat version`
		sed -i "s/{VERSION}/${versionFile}/" PKGBUILD
		if [ ${USE_CCACHE} = true ]; then
			sed -i "s/{BUILD_OPTION}/${BUILD_OPTION} -DUSE_PRECOMPILED_HEADERS=OFF/" PKGBUILD
		else
			sed -i "s/{BUILD_OPTION}/${BUILD_OPTION}/" PKGBUILD
		fi
		chmod -R a+rw ${CI_BUILD_DIR}/.ccache
	else
		executeCommand="cd build && ( cmake ${BUILD_OPTION} -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DDEBIAN_NAME_TAG=${DOCKER_TAG} -DUSE_STANDARD_INSTALLER_NAME=${USE_STANDARD_INSTALLER_NAME} ../ || exit 2 )"
		executeCommand+=" && ( make -j $(nproc) package || exit 3 )"
	fi

	# run docker
	docker run --rm \
	-v "${CI_BUILD_DIR}/.ccache:/.ccache" \
	-v "${CI_BUILD_DIR}/deploy:/deploy" \
	-v "${CI_BUILD_DIR}:/source:ro" \
	$REGISTRY_URL:$DOCKER_TAG \
	/bin/bash -c "${cache_env} && cd / && mkdir -p hyperhdr && cp -r source/. /hyperhdr &&
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
