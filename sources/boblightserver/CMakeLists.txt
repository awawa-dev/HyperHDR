
# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/boblightserver)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/boblightserver)

FILE ( GLOB BoblightServer_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(boblightserver ${BoblightServer_SOURCES} )

target_link_libraries(boblightserver
	hyperhdr-base
	hyperhdr-utils
	${QT_LIBRARIES}
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(boblightserver REUSE_FROM precompiled_hyperhdr_headers)
endif()

