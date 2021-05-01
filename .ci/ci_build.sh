#!/bin/bash

# detect CI
if [ "$SYSTEM_COLLECTIONID" != "" ]; then
	# Azure Pipelines
	echo "Azure detected"
	CI_NAME="$(echo "$AGENT_OS" | tr '[:upper:]' '[:lower:]')"
	CI_BUILD_DIR="$BUILD_SOURCESDIRECTORY"
elif [ "$HOME" != "" ]; then
	# GitHub Actions
	echo "Github Actions detected"
	CI_NAME="$(uname -s | tr '[:upper:]' '[:lower:]')"
	CI_BUILD_DIR="$GITHUB_WORKSPACE"
else
	# for executing in non ci environment
	CI_NAME="$(uname -s | tr '[:upper:]' '[:lower:]')"
fi

# set environment variables if not exists (debug)
[ -z "${BUILD_TYPE}" ] && BUILD_TYPE="Release"

echo "Platform: ${PLATFORM}, build type: ${BUILD_TYPE}, CI_NAME: $CI_NAME, docker image: ${DOCKER_IMAGE}, docker type: ${DOCKER_TAG}"

# Build the package on osx or linux
if [[ "$CI_NAME" == 'osx' || "$CI_NAME" == 'darwin' ]]; then
	echo "Start: osx or darwin"
	if [[ "$USE_CCACHE" == '1' ]]; then
		echo "Using ccache"

		# Init ccache
		mkdir -p .ccache
		cd .ccache
		CCACHE_PATH=$PWD
		cd ..
        cachecommand="-DCMAKE_C_COMPILER_LAUNCHER=ccache"
		export CCACHE_DIR=${CCACHE_PATH} && export CCACHE_COMPRESS=true && export CCACHE_COMPRESSLEVEL=6 && export CCACHE_MAXSIZE=400M
		echo "CCache parameters: ${cachecommand}"		
		ls -a .ccache

		mkdir build || exit 1
		cd build
		ccache -p
		cmake ${cachecommand} -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX:PATH=/usr/local ../ || exit 2
		make -j $(sysctl -n hw.ncpu) package || exit 3

		exit 0;
		exit 1 || { echo "---> HyperHDR compilation failed! Abort"; exit 5; }
	else
		echo "Not using ccache"
		mkdir build || exit 1
		cd build
		cmake -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX:PATH=/usr/local ../ || exit 2
		make -j $(sysctl -n hw.ncpu) package || exit 3
		exit 0;
		exit 1 || { echo "---> HyperHDR compilation failed! Abort"; exit 5; }
	fi
elif [[ $CI_NAME == *"mingw64_nt"* || "$CI_NAME" == 'windows_nt' ]]; then
	echo "Start: windows"
	
	echo "Number of Cores $NUMBER_OF_PROCESSORS"
	mkdir build || exit 1
	cd build
	cmake -G "Visual Studio 16 2019" -A x64 -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_GITHUB_ACTION=1 ../ || exit 2
	cmake --build . --target package --config Release -- -nologo -v:m -maxcpucount || exit 3
	exit 0;
	exit 1 || { echo "---> Hyperhdr compilation failed! Abort"; exit 5; }
elif [[ "$CI_NAME" == 'linux' ]]; then
	echo "Compile Hyperhdr with DOCKER_IMAGE = ${DOCKER_IMAGE}, DOCKER_TAG = ${DOCKER_TAG} and friendly name DOCKER_NAME = ${DOCKER_NAME}"
	
	# set GitHub Container Registry url
	REGISTRY_URL="ghcr.io/awawa-dev/${DOCKER_IMAGE}"
	
	# take ownership of deploy dir
	mkdir ${CI_BUILD_DIR}/deploy

	if [[ "$USE_CCACHE" == '1' ]]; then
		echo "Using cache"
		
		mkdir -p .ccache
		
        cachecommand="-DCMAKE_C_COMPILER_LAUNCHER=ccache"
		cache_env="export CCACHE_DIR=/.ccache && export CCACHE_COMPRESS=true && export CCACHE_COMPRESSLEVEL=6 && export CCACHE_MAXSIZE=400M"
		echo "CCache parameters: ${cachecommand}, env: ${cache_env}"
		
		ls -a .ccache
		# run docker
		docker run --rm \
		-v "${CI_BUILD_DIR}/.ccache:/.ccache" \
		-v "${CI_BUILD_DIR}/deploy:/deploy" \
		-v "${CI_BUILD_DIR}:/source:ro" \
		$REGISTRY_URL:$DOCKER_TAG \
		/bin/bash -c "${cache_env} && export -p && ccache -p && cd / && mkdir hyperhdr && cp -r source/. /hyperhdr &&
		cd /hyperhdr && mkdir build && cd build &&
		cmake ${cachecommand} -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ../ || exit 2 &&
		make -j $(nproc) package || exit 3 &&
		cp /hyperhdr/build/bin/h* /deploy/ 2>/dev/null || : &&
		cp /hyperhdr/build/Hyper* /deploy/ 2>/dev/null || : &&
		exit 0;
		exit 1 " || { echo "---> HyperHDR compilation failed! Abort"; exit 5; }
		ls -a .ccache
	else
		echo "Not using cache"
		
		# run docker
		docker run --rm \
		-v "${CI_BUILD_DIR}/deploy:/deploy" \
		-v "${CI_BUILD_DIR}:/source:ro" \
		$REGISTRY_URL:$DOCKER_TAG \
		/bin/bash -c "cd / && mkdir hyperhdr && cp -r source/. /hyperhdr &&
		cd /hyperhdr && mkdir build && cd build &&
		cmake -DPLATFORM=${PLATFORM} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ../ || exit 2 &&
		make -j $(nproc) package || exit 3 &&
		cp /hyperhdr/build/bin/h* /deploy/ 2>/dev/null || : &&
		cp /hyperhdr/build/Hyper* /deploy/ 2>/dev/null || : &&
		exit 0;
		exit 1 " || { echo "---> HyperHDR compilation failed! Abort"; exit 5; }
	fi
	
	# overwrite file owner to current user
	sudo chown -fR $(stat -c "%U:%G" ${CI_BUILD_DIR}/deploy) ${CI_BUILD_DIR}/deploy
fi
