# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/performance-counters)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/performance-counters)

if (NOT APPLE)
	FILE ( GLOB_RECURSE performance_counters_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )
else()
	FILE ( GLOB_RECURSE performance_counters_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )
	LIST ( REMOVE_ITEM performance_counters_SOURCES "${CURRENT_SOURCE_DIR}/SystemPerformanceCounters.cpp" )
endif()

add_library(performance-counters OBJECT ${performance_counters_SOURCES})

target_link_libraries(performance-counters
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(performance-counters REUSE_FROM precompiled_hyperhdr_headers)
endif()

if(APPLE)
	set_source_files_properties(${CURRENT_SOURCE_DIR}/SystemPerformanceCounters.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
