#=============================================================================
# SQLite
#=============================================================================

if (NOT USE_SHARED_LIBS)
	add_library(sqlite3 "${CMAKE_CURRENT_SOURCE_DIR}/sqlite/sqlite3.c")
else()
	add_library(sqlite3 SHARED "${CMAKE_CURRENT_SOURCE_DIR}/sqlite/sqlite3.c")
	if(WIN32)
	   target_compile_definitions(sqlite3
		  INTERFACE
			 "SQLITE_API=__declspec(dllimport)"
		  PRIVATE
			 "SQLITE_API=__declspec(dllexport)"
	   )
	else()
	   target_compile_definitions(sqlite3
		  INTERFACE
			 "SQLITE_API=__attribute__((visibility(\"default\")))"
	   )
	endif()
endif()

target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sqlite")
target_link_libraries(sqlite3 PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

install(
    TARGETS   sqlite3
    LIBRARY   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE   DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

set_target_properties(sqlite3 PROPERTIES
    OUTPUT_NAME   sqlite3
    PUBLIC_HEADER sqlite3.h
)

target_compile_definitions(sqlite3 PUBLIC
	SQLITE_THREADSAFE=2
	SQLITE_DEFAULT_MEMSTATUS=0
	SQLITE_DEFAULT_SYNCHRONOUS=3	
	SQLITE_OMIT_AUTHORIZATION
	SQLITE_OMIT_DEPRECATED
)

#=============================================================================
# Protobuf-nanopb
#=============================================================================

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

#=============================================================================
# RPi ws281x
#=============================================================================

if(ENABLE_WS281XPWM)
	file(READ "${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/CMakeLists.txt" FILE_CONTENTS)
	string(REPLACE "if(BUILD_SHARED)" "if(USE_SHARED_LIBS)" FILE_CONTENTS "${FILE_CONTENTS}")
	string(REPLACE "configure_file(version.h.in version.h)" [=[configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)]=] FILE_CONTENTS "${FILE_CONTENTS}")
	file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x/CMakeLists.txt" "${FILE_CONTENTS}")

	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rpi_ws281x)			
endif()

#=============================================================================
# LUNASVG
#=============================================================================


set(no_dev_warnings_backup "$CACHE{CMAKE_WARN_DEPRECATED}")
set(CMAKE_WARN_DEPRECATED OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lunasvg)
set_target_properties(lunasvg PROPERTIES POSITION_INDEPENDENT_CODE ON)
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

	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/flatbuffers)

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

		cmake_policy(PUSH)

		if (NOT USE_SHARED_LIBS)
			set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
			set(qmqtt_SHARED OFF)
		endif()

		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/qmqtt)

		cmake_policy(POP)
	endif()
endif()

#=============================================================================
# XZ
#=============================================================================

if ( ENABLE_XZ )
	if (NOT LIBLZMA_FOUND)
		message( STATUS "System library xz could not be found. Using embedded xz library.")
		option(BUILD_TESTING "" OFF)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/xz)
		set_target_properties(liblzma PROPERTIES INTERFACE_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/external/xz/src/liblzma/api>)		
		add_library(LibLZMA::LibLZMA ALIAS liblzma)
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

#=============================================================================
# STB
#=============================================================================

add_library(stb INTERFACE )
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_compile_definitions(stb INTERFACE
	STB_IMAGE_WRITE_IMPLEMENTATION
	STB_IMAGE_IMPLEMENTATION
)
