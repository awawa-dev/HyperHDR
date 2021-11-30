# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/api)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/api)

FILE ( GLOB_RECURSE Api_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

set(Api_RESOURCES ${CURRENT_SOURCE_DIR}/JSONRPC_schemas.qrc )

add_library(hyperhdr-api
	${Api_SOURCES}
	${Api_RESOURCES}
)

target_link_libraries(hyperhdr-api
	hyperhdr-base
	hyperhdr-utils
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Gui
	Qt${Qt_VERSION}::Network
)
