
# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/flatbufserver)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/flatbufserver)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${FLATBUFFERS_INCLUDE_DIRS}
)

FILE ( GLOB FLATBUFSERVER_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

set(Flatbuffer_GENERATED_FBS
	hyperhdr_reply_generated.h
	hyperhdr_request_generated.h
)

set(Flatbuffer_FBS
	${CURRENT_SOURCE_DIR}/hyperhdr_reply.fbs
	${CURRENT_SOURCE_DIR}/hyperhdr_request.fbs
)
FOREACH(FBS_FILE ${Flatbuffer_FBS})
	compile_flattbuffer_schema(${FBS_FILE} ${CMAKE_CURRENT_SOURCE_DIR})
ENDFOREACH(FBS_FILE)

# let cmake know about new generated source files
set_source_files_properties(
	${Flatbuffer_GENERATED_FBS} PROPERTIES GENERATED TRUE
)

add_library(flatbufserver
	${FLATBUFSERVER_SOURCES}
	${Flatbuffer_GENERATED_FBS}
)

target_link_libraries(flatbufserver
	hyperhdr-utils
	flatbuffers
	Qt${Qt_VERSION}::Network
	Qt${Qt_VERSION}::Core
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(flatbufserver REUSE_FROM precompiled_hyperhdr_headers)
endif()
