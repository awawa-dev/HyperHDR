
# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/blackborder)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/blackborder)

FILE ( GLOB Blackborder_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(blackborder	OBJECT ${Blackborder_SOURCES} )

target_link_libraries(blackborder
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(blackborder REUSE_FROM precompiled_hyperhdr_headers)
endif()
