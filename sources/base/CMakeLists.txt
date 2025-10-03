# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/base)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/base)

FILE ( GLOB HyperHDR_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

qt_add_resources(HyperHDR_RESOURCES ${CURRENT_SOURCE_DIR}/resource.qrc )

add_library(hyperhdr-base OBJECT
	${HyperHDR_SOURCES}
	${HyperHDR_RESOURCES}
)

target_link_libraries(hyperhdr-base
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	infinite-engine
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(hyperhdr-base REUSE_FROM precompiled_hyperhdr_headers)
endif()
