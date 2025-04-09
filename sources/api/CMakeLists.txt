# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/api)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/api)

FILE ( GLOB_RECURSE Api_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

set(Api_RESOURCES ${CURRENT_SOURCE_DIR}/JSONRPC_schemas.qrc )

add_library(hyperhdr-api OBJECT
	${Api_SOURCES}
	${Api_RESOURCES}
)

target_link_libraries(hyperhdr-api
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(hyperhdr-api REUSE_FROM precompiled_hyperhdr_headers)
endif()
