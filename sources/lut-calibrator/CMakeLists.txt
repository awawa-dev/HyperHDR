# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/lut-calibrator)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/lut-calibrator)


FILE ( GLOB_RECURSE lut-calibrator_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

# In-source builds generate Qt files under sources/lut-calibrator/lut-calibrator_autogen.
# Exclude generated outputs to avoid AutoMoc recursion/header-lookup failures.
list(FILTER lut-calibrator_SOURCES EXCLUDE REGEX ".*/lut-calibrator_autogen/.*")
list(FILTER lut-calibrator_SOURCES EXCLUDE REGEX ".*/CMakeFiles/.*")
list(FILTER lut-calibrator_SOURCES EXCLUDE REGEX ".*/moc_.*\\.cpp$")
list(FILTER lut-calibrator_SOURCES EXCLUDE REGEX ".*/qrc_.*\\.cpp$")

add_library(lut-calibrator OBJECT ${lut-calibrator_SOURCES})

target_link_libraries(lut-calibrator
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	linalg
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(lut-calibrator REUSE_FROM precompiled_hyperhdr_headers)
endif()
