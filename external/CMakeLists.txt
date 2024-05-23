if(ENABLE_WS281XPWM)
	add_library(ws281x
		${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/mailbox.c ${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/ws2811.c
		${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/pwm.c ${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/dma.c
		${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/pcm.c
		${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/rpihw.c)
endif()

#=============================================================================
# LUNASVG
#=============================================================================


set(no_dev_warnings_backup "$CACHE{CMAKE_WARN_DEPRECATED}")
set(CMAKE_WARN_DEPRECATED OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lunasvg)
set(CMAKE_WARN_DEPRECATED ${no_dev_warnings_backup} CACHE INTERNAL "" FORCE)

#=============================================================================
# FLATBUFFER
#=============================================================================

if (USE_SYSTEM_FLATBUFFERS_LIBS AND NOT CMAKE_CROSSCOMPILING)
	message(STATUS "${YellowColor}Try using the flatbuffers system libraries instead of the embedded submodule. Absolutely no guarantee.${ColorReset}")
	find_program(FLATBUFFERS_FLATC_EXECUTABLE NAMES flatc)
	find_package(FlatBuffers)

	if (NOT FLATBUFFERS_FOUND OR NOT FLATBUFFERS_FLATC_EXECUTABLE)
		message(WARNING "${RedColor}Could NOT find flatbuffers system libraries or the compilet, build embedded flatbuffers libraries and compiler${ColorReset}")
		#Fallback: build flatbuffers static library
		set(DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS OFF PARENT_SCOPE)
		set(USE_SYSTEM_FLATBUFFERS_LIBS OFF)
	else()
		message(STATUS "Found system FLATBUFFERS libs.")
	endif()
endif()

if (NOT USE_SYSTEM_FLATBUFFERS_LIBS OR CMAKE_CROSSCOMPILING)
	set(CMAKE_POLICY_DEFAULT_CMP0071 NEW)
	set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")	

	if (NOT CMAKE_CROSSCOMPILING AND USE_PRECOMPILED_HEADERS)
		set(FLATBUFFERS_ENABLE_PCH ON CACHE BOOL "")
		set(FLATBUFFERS_SKIP_MONSTER_EXTRA ON CACHE BOOL "")
	endif()

	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(FLATBUFFERS_CXX_FLAGS "-Wno-error")
	endif()

	if(CMAKE_CROSSCOMPILING AND NOT EXISTS ${FLATBUFFERS_FLATC_EXECUTABLE})
		message( STATUS "${YellowColor}Cross-compiling detected. Building flatc compiler for the host first.${ColorReset}")

		unset(FLATBUFFERS_FLATC_EXECUTABLE)
		set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "")
		set(FLATBUFFERS_BUILD_FLATHASH OFF CACHE BOOL "")
		set(FLATBUFFERS_HOST_FLATBUFFERS_DIR ${CMAKE_CURRENT_BINARY_DIR}/host_flatc)		
		file(MAKE_DIRECTORY ${FLATBUFFERS_HOST_FLATBUFFERS_DIR})

		EXECUTE_PROCESS ( WORKING_DIRECTORY ${FLATBUFFERS_HOST_FLATBUFFERS_DIR} RESULT_VARIABLE EXEC_RES
			COMMAND ${CMAKE_COMMAND} -E env --unset=CC --unset=CXX --unset=CXXFLAGS --unset=CFLAGS ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}/external/flatbuffers -DFLATBUFFERS_BUILD_TESTS=OFF -DFLATBUFFERS_BUILD_FLATLIB=OFF -DFLATBUFFERS_BUILD_FLATHASH=OFF)

		if (EXEC_RES AND NOT EXEC_RES EQUAL 0)
			message( FATAL_ERROR "Could not configure flatc compiler for the host")
		endif()

		EXECUTE_PROCESS( WORKING_DIRECTORY ${FLATBUFFERS_HOST_FLATBUFFERS_DIR} COMMAND ${CMAKE_COMMAND} --build . --config Release )

		find_program(FLATBUFFERS_FLATC_EXECUTABLE NAMES flatc PATHS ${FLATBUFFERS_HOST_FLATBUFFERS_DIR} ${FLATBUFFERS_HOST_FLATBUFFERS_DIR}/Release NO_DEFAULT_PATH)
		if (NOT FLATBUFFERS_FLATC_EXECUTABLE)
			message( FATAL_ERROR "Could not build flatc compiler for the host")
		endif()
	endif()

	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers)

	set(FLATBUFFERS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/include")

	IF (NOT CMAKE_CROSSCOMPILING)
		set(FLATBUFFERS_FLATC_EXECUTABLE "$<TARGET_FILE:flatc>")
	endif()

	message( STATUS "Using host flatc compiler: ${FLATBUFFERS_FLATC_EXECUTABLE}")
endif()

set(FLATBUFFERS_FLATC_EXECUTABLE ${FLATBUFFERS_FLATC_EXECUTABLE} PARENT_SCOPE)
set(FLATBUFFERS_INCLUDE_DIRS ${FLATBUFFERS_INCLUDE_DIRS} PARENT_SCOPE)
include_directories(${FLATBUFFERS_INCLUDE_DIRS})

if (FLATBUFFERS_INCLUDE_DIRS AND EXISTS "${FLATBUFFERS_INCLUDE_DIRS}/../package.json")		
		file(STRINGS "${FLATBUFFERS_INCLUDE_DIRS}/../package.json" _FLATBUFFERS_VERSION_STRING REGEX "^[ \t\r\n]+\"version\":[ \t\r\n]+\"[0-9]+.[0-9]+.[0-9]+\",")
		string(REGEX REPLACE "^[ \t\r\n]+\"version\":[ \t\r\n]+\"([0-9]+.[0-9]+.[0-9]+)\"," "\\1" FLATBUFFERS_PARSE_VERSION "${_FLATBUFFERS_VERSION_STRING}")
		message(STATUS "Using flatbuffers libraries version: \"${FLATBUFFERS_PARSE_VERSION}\"")
endif ()

function(compile_flattbuffer_schema SRC_FBS OUTPUT_DIR)
	string(REGEX REPLACE "\\.fbs$" "_generated.h" GEN_HEADER ${SRC_FBS})
	set_property(SOURCE ${GEN_HEADER} PROPERTY SKIP_AUTOMOC ON)

	add_custom_command(
		OUTPUT ${GEN_HEADER}
		COMMAND "${FLATBUFFERS_FLATC_EXECUTABLE}" -c --no-includes --gen-mutable
				--gen-object-api
				-o "${OUTPUT_DIR}"
				"${SRC_FBS}"
		DEPENDS "${FLATBUFFERS_FLATC_EXECUTABLE}" ${SRC_FBS})
endfunction()

#=============================================================================
# MBEDTLS
#=============================================================================

if (USE_SYSTEM_MBEDTLS_LIBS)
	message(STATUS "${YellowColor}Try using the mbedtls system libraries instead of the embedded submodule. Absolutely no guarantee.${ColorReset}")
	find_package(mbedtls)

	if (NOT MBEDTLS_FOUND)
		message(STATUS "${RedColor}Could NOT find system mbedtls, fallback to build embedded mbedtl version${ColorReset}")
		#Fallback: build mbedtls static libray inside project
		set(DEFAULT_USE_SYSTEM_MBEDTLS_LIBS OFF PARENT_SCOPE)
		set(USE_SYSTEM_MBEDTLS_LIBS OFF)
	else()
		message(STATUS "Found system MBEDTLS version: ${MBEDTLS_VERSION}")
	endif()
endif()

if (NOT USE_SYSTEM_MBEDTLS_LIBS)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared mbedtls libraries")
	set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedTLS tests")
	set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbedTLS programs")
	set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "Disable mbedTLS crash on warnings")
	set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Disable mbedTLS shared libraries")
	set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "Enable mbedTLS static libraries")

	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mbedtls)	

	set(MBEDTLS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/mbedtls/include")
	set(MBEDTLS_INCLUDE_DIR ${MBEDTLS_INCLUDE_DIR} PARENT_SCOPE)

	if (MBEDTLS_INCLUDE_DIR AND EXISTS "${MBEDTLS_INCLUDE_DIR}/mbedtls/build_info.h")
		file(STRINGS "${MBEDTLS_INCLUDE_DIR}/mbedtls/build_info.h" _MBEDTLS_VERSION_STRING REGEX "^#define MBEDTLS_VERSION_STRING[ \t\r\n]+\"[0-9]+.[0-9]+.[0-9]+\"")
		string(REGEX REPLACE "^#define MBEDTLS_VERSION_STRING[ \t\r\n]+\"([0-9]+.[0-9]+.[0-9]+)\"" "\\1" MBEDTLS_VERSION "${_MBEDTLS_VERSION_STRING}")
		message(STATUS "Using static mbedtls libraries version: \"${MBEDTLS_VERSION}\"")
	elseif (MBEDTLS_INCLUDE_DIR AND EXISTS "${MBEDTLS_INCLUDE_DIR}/mbedtls/version.h")
		file(STRINGS "${MBEDTLS_INCLUDE_DIR}/mbedtls/version.h" _MBEDTLS_VERSION_STRING REGEX "^#[\t ]*define[\t ]+MBEDTLS_VERSION_STRING[\t ]+\"[0-9]+.[0-9]+.[0-9]+\"")
		string(REGEX REPLACE "^.*MBEDTLS_VERSION_STRING.*([0-9]+.[0-9]+.[0-9]+).*" "\\1" MBEDTLS_VERSION "${_MBEDTLS_VERSION_STRING}")
		message(STATUS "Using static mbedtls libraries (build version \"${MBEDTLS_VERSION}\")")
	endif ()
endif ()

#=============================================================================
# MQTT
#=============================================================================

if ( ENABLE_MQTT )
	if (USE_SYSTEM_MQTT_LIBS)
		message(STATUS "${YellowColor}Try using the qmqtt system libraries instead of the embedded submodule. Absolutely no guarantee.${ColorReset}")

		find_package(Qmqtt)

		if (NOT QMQTT_FOUND)
			message(WARNING "${RedColor}Could NOT find qmqtt system libraries, build static qmqtt libraries${ColorReset}")
			#Fallback: build qmqtt static libray inside project
			set(DEFAULT_USE_SYSTEM_MQTT_LIBS OFF PARENT_SCOPE)
			set(USE_SYSTEM_MQTT_LIBS OFF)
		else()
			message(STATUS "Found system QMQTT libs.")
		endif()
	endif()

	if (NOT USE_SYSTEM_MQTT_LIBS)
		# HyperHDR workaround for fixed Qt5 version
		file(READ "${CMAKE_CURRENT_SOURCE_DIR}/qmqtt/CMakeLists.txt" FILE_CONTENTS)
		string(REPLACE "Qt5" "Qt${Qt_VERSION}" FILE_CONTENTS "${FILE_CONTENTS}")
		string(REPLACE "find_package" "#find_package" FILE_CONTENTS "${FILE_CONTENTS}")		
		file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/qmqtt/CMakeLists.txt" "${FILE_CONTENTS}")

		if (NOT WIN32)	
			set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
			set(qmqtt_SHARED OFF)
		endif()

		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/qmqtt)
	endif()
endif()

#=============================================================================
# XZ
#=============================================================================

if ( ENABLE_XZ )
	option(BUILD_TESTING "" OFF)
	if (NOT LIBLZMA_FOUND)
		message( STATUS "System library xz could not be found. Using embedded xz library.")
		if(MSVC)			
			set(BACKUP_OF_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})			
			string(REGEX REPLACE "(\/W[011123456789])" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
			message( STATUS "Disable warnings for xz library: ${CMAKE_CXX_FLAGS}")
		endif()
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/xz)
		if(MSVC)
			set(CMAKE_CXX_FLAGS ${BACKUP_OF_CMAKE_CXX_FLAGS})
			message( STATUS "Restoring compiling flags after xz: ${CMAKE_CXX_FLAGS}")			
		endif()		
	endif()
ENDIF()

#=============================================================================
# sdbus-cpp
#=============================================================================

if (UNIX AND NOT APPLE AND (ENABLE_POWER_MANAGEMENT OR ENABLE_PIPEWIRE))
	set(SDBUSCPP_BUILD_DOCS OFF CACHE BOOL "No doc")
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sdbus-cpp)
	set_target_properties(sdbus-c++-objlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
