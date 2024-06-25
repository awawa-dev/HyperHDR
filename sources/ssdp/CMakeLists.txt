# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/ssdp)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/ssdp)

FILE ( GLOB SSDP_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(ssdp OBJECT ${SSDP_SOURCES})

target_link_libraries(ssdp
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(ssdp REUSE_FROM precompiled_hyperhdr_headers)
endif()
