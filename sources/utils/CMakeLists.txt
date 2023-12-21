# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/utils)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/utils)

if (NOT APPLE)
	FILE ( GLOB_RECURSE Utils_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )
else()
	FILE ( GLOB_RECURSE Utils_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )
	LIST ( REMOVE_ITEM Utils_SOURCES "${CURRENT_SOURCE_DIR}/SystemPerformanceCounters.cpp" )
endif()

add_library(hyperhdr-utils
	${Utils_SOURCES}
)

target_include_directories(hyperhdr-utils PUBLIC ${TURBOJPEG_INCLUDE_DIRS})

target_link_libraries(hyperhdr-utils
	hyperhdr-base
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Gui
	Qt${Qt_VERSION}::Network
	${TURBOJPEG_LINK_LIBRARIES}
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(hyperhdr-utils REUSE_FROM precompiled_hyperhdr_headers)
endif()
