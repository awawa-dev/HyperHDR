# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/lut-calibrator)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/lut-calibrator)


FILE ( GLOB_RECURSE lut-calibrator_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(lut-calibrator OBJECT ${lut-calibrator_SOURCES})

target_link_libraries(lut-calibrator
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	linalg
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(lut-calibrator REUSE_FROM precompiled_hyperhdr_headers)
endif()
