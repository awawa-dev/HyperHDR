#=============================================================================
# SQLite
#=============================================================================

add_library(sqlite3 STATIC "${CMAKE_CURRENT_SOURCE_DIR}/sqlite/sqlite3.c")

target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sqlite")

target_link_libraries(sqlite3 PRIVATE Threads::Threads)

target_compile_definitions(sqlite3 PRIVATE

    # HyperHDR uses one SQLite connection per thread.
    SQLITE_THREADSAFE=2

    # Less allocator/statistics overhead.
    SQLITE_DEFAULT_MEMSTATUS=0

    # HyperHDR SQL does not depend on these historical behaviors.
    SQLITE_DQS=0
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_MAX_EXPR_DEPTH=0

    # Not used by HyperHDR's SQLite wrapper.
    SQLITE_OMIT_AUTHORIZATION
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_UTF16
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_DECLTYPE
    SQLITE_OMIT_COMPILEOPTION_DIAGS
    SQLITE_OMIT_DESERIALIZE
    SQLITE_OMIT_GET_TABLE
    SQLITE_OMIT_COMPLETE
    SQLITE_OMIT_INCRBLOB
    SQLITE_OMIT_TRACE
    SQLITE_OMIT_JSON
    SQLITE_OMIT_INTROSPECTION_PRAGMAS
    SQLITE_OMIT_LOOKASIDE
)

if(MSVC)
     target_compile_options(sqlite3 PRIVATE /W0 $<$<CONFIG:Release>:/O1> $<$<CONFIG:RelWithDebInfo>:/O1> $<$<CONFIG:MinSizeRel>:/O1>)
else()
    target_compile_options(sqlite3 PRIVATE -Os -w)
endif()

set_target_properties(sqlite3 PROPERTIES OUTPUT_NAME sqlite3)

#=============================================================================
# Protobuf-nanopb
#=============================================================================

if (USE_SYSTEM_NANOPB_LIBS)
	find_package(nanopb)
	if (NOT nanopb_FOUND)
		message(WARNING "Could NOT find nanopb system libraries. Fallback to nanopb submodule.")
	endif()
endif()

if (NOT USE_SYSTEM_NANOPB_LIBS OR NOT nanopb_FOUND)
	set(PROTOBUF-NANOPB-SOURCES
			${CMAKE_CURRENT_SOURCE_DIR}/nanopb/pb.h
			${CMAKE_CURRENT_SOURCE_DIR}/nanopb/pb_common.h
			${CMAKE_CURRENT_SOURCE_DIR}/nanopb/pb_common.c
			${CMAKE_CURRENT_SOURCE_DIR}/nanopb/pb_encode.h
			${CMAKE_CURRENT_SOURCE_DIR}/nanopb/pb_encode.c
			${CMAKE_CURRENT_SOURCE_DIR}/nanopb/pb_decode.h
			${CMAKE_CURRENT_SOURCE_DIR}/nanopb/pb_decode.c)

	add_library(protobuf-nanopb OBJECT ${PROTOBUF-NANOPB-SOURCES})
	set_target_properties(protobuf-nanopb PROPERTIES OUTPUT_NAME protobuf-nanopb)
	install(TARGETS protobuf-nanopb EXPORT nanopb-targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

	target_include_directories(protobuf-nanopb INTERFACE
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nanopb>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
endif()

#=============================================================================
# RPi ws281x
#=============================================================================

if(ENABLE_WS281XPWM)
	file(READ "${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/CMakeLists.txt" FILE_CONTENTS)
	string(REPLACE "cmake_minimum_required(VERSION 3.0.0)" "cmake_minimum_required(VERSION 3.5.0)" FILE_CONTENTS "${FILE_CONTENTS}")
	string(REPLACE "if(BUILD_SHARED)" "if(USE_SHARED_LIBS AND ENABLE_DEPENDENCY_PACKAGING)" FILE_CONTENTS "${FILE_CONTENTS}")
	if(NOT USE_PRECOMPILED_HEADERS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/version")
		message("Renaming ws281x version file to avoid conflicts with c++20")
		file(RENAME "${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/version" "${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/version.txt")
		string(REPLACE "file(READ version PROJECT_VERSION)" "file(READ version.txt PROJECT_VERSION)" FILE_CONTENTS "${FILE_CONTENTS}")
	endif()
	string(REPLACE "configure_file(version.h.in version.h)" [=[configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)]=] FILE_CONTENTS "${FILE_CONTENTS}")
	file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/CMakeLists.txt" "${FILE_CONTENTS}")

	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x)
endif()

#=============================================================================
# LINALG
#=============================================================================

add_library(linalg INTERFACE)
target_compile_definitions(linalg INTERFACE LINALG_FORWARD_COMPATIBLE )
target_include_directories(linalg INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/linalg")

#=============================================================================
# LUNASVG
#=============================================================================

set(no_dev_warnings_backup "$CACHE{CMAKE_WARN_DEPRECATED}")
set(CMAKE_WARN_DEPRECATED OFF CACHE INTERNAL "" FORCE)

if (USE_SYSTEM_LUNASVG_LIBS)
	find_package(lunasvg GLOBAL)
	if (NOT lunasvg_FOUND)
		message(WARNING "Could NOT find lunasvg system libraries. Fallback to lunasvg submodule.")
	endif()
endif()

if (NOT USE_SYSTEM_LUNASVG_LIBS OR NOT lunasvg_FOUND)
	set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
	set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)


	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lunasvg)
	set(lunasvg_targets lunasvg plutovg)

	foreach(target IN LISTS lunasvg_targets)
		set(opts_before)
		set(opts_after)
		get_target_property(opts_before ${target} COMPILE_OPTIONS)

		if(MSVC)
			set_target_properties(${target} PROPERTIES MSVC_WARNING_LEVEL 0)
			set(opts_after ${opts_before})
			if(opts_after)
				list(FILTER opts_after EXCLUDE REGEX "/W[0-4]|/w")
			endif()
			list(APPEND opts_after "/W0")
		else()
			foreach(f ${opts_before})
				if(NOT f MATCHES "^-w$|^-Wall$|^-Wextra$")
					list(APPEND opts_after ${f})
				endif()
			endforeach()
			list(APPEND opts_after "-w")
		endif()

		set_target_properties(${target} PROPERTIES COMPILE_OPTIONS "${opts_after}")
		get_target_property(opts_now ${target} COMPILE_OPTIONS)
		message("Lunasvg target '${target}' compile options before: ${opts_before}  after: ${opts_now}")
	endforeach()
endif()

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
		message(STATUS "Found system FLATBUFFERS libs (${FLATBUFFERS_INCLUDE_DIRS}).")
		if (NOT (TARGET flatbuffers))
			message(STATUS "Creating interface for the FLATBUFFERS lib")
			add_library(flatbuffers INTERFACE)
			target_include_directories(flatbuffers INTERFACE ${FLATBUFFERS_INCLUDE_DIRS})
		endif ()
	endif()
endif()

if (NOT USE_SYSTEM_FLATBUFFERS_LIBS OR CMAKE_CROSSCOMPILING)
	cmake_policy(PUSH)

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

	file(RENAME "${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/.git" "${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/.git.hidden")

	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers)

	file(RENAME "${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/.git.hidden" "${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/.git")

	set(FLATBUFFERS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers/include")

	IF (NOT CMAKE_CROSSCOMPILING)
		set(FLATBUFFERS_FLATC_EXECUTABLE "$<TARGET_FILE:flatc>")
	else()
		message( STATUS "Using host flatc compiler: ${FLATBUFFERS_FLATC_EXECUTABLE}")
	endif()

	cmake_policy(POP)
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
# MQTT
#=============================================================================

if ( ENABLE_MQTT )
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core_mqtt)
		set_target_properties(core_mqtt PROPERTIES INTERFACE_SOURCES "")
		target_include_directories(core_mqtt PUBLIC ${CMAKE_SOURCE_DIR}/include/mqtt)
endif()

#=============================================================================
# ZSTD
#=============================================================================

if ( ENABLE_ZSTD )
	if (NOT zstd_FOUND)
		message( STATUS "System library zstd could not be found. Using embedded zstd library.")
		if(WIN32)
			set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_SOURCE_DIR}/external/zstd/lib")
		endif()
		set(ZSTD_BUILD_TESTS OFF)
		set(ZSTD_LEGACY_SUPPORT OFF)
		set(ZSTD_BUILD_PROGRAMS OFF)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zstd/build/cmake)
		set_target_properties(libzstd PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/external/zstd/lib>)
		add_library(zstd::zstd ALIAS libzstd)
	endif()
ENDIF()

#=============================================================================
# STB
#=============================================================================

add_library(stb INTERFACE)

if (USE_SYSTEM_STB_LIBS)
	find_path(STB_INCLUDE_DIR stb_image.h PATH_SUFFIXES stb)
	if (NOT STB_INCLUDE_DIR)
		message(WARNING "Could NOT find system STB libraries. Falling back to embedded STB.")
		target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
	else()
		message(STATUS "Found system STB libraries: ${STB_INCLUDE_DIR}")
		target_include_directories(stb INTERFACE ${STB_INCLUDE_DIR})
	endif()
else()
	target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
endif()

target_compile_definitions(stb INTERFACE
	STB_IMAGE_WRITE_IMPLEMENTATION
	STB_IMAGE_IMPLEMENTATION
)
