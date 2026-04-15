# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/json-utils)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/json-utils)


FILE ( GLOB_RECURSE json_utils_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

# Exclude generated build artifacts from recursive source globs.
list(FILTER json_utils_SOURCES EXCLUDE REGEX ".*/[^/]*_autogen/.*")
list(FILTER json_utils_SOURCES EXCLUDE REGEX ".*/CMakeFiles/.*")
list(FILTER json_utils_SOURCES EXCLUDE REGEX ".*/moc_.*\\.cpp$")
list(FILTER json_utils_SOURCES EXCLUDE REGEX ".*/qrc_.*\\.cpp$")

add_library(json-utils OBJECT
	${json_utils_SOURCES}
)

target_link_libraries(json-utils
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(json-utils REUSE_FROM precompiled_hyperhdr_headers)
endif()
