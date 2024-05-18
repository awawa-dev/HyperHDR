# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/commandline)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/commandline)

FILE ( GLOB Parser_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(commandline ${Parser_SOURCES} )

target_link_libraries(commandline
	hyperhdr-base
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(commandline REUSE_FROM precompiled_hyperhdr_headers)
endif()
