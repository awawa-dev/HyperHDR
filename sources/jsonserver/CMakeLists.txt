
# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/jsonserver)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/jsonserver)

FILE ( GLOB JsonServer_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(jsonserver ${JsonServer_SOURCES} )

target_link_libraries(jsonserver
	hyperhdr-api
	hyperhdr-base
	Qt${Qt_VERSION}::Network
	Qt${Qt_VERSION}::Gui
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(jsonserver REUSE_FROM precompiled_hyperhdr_headers)
endif()
