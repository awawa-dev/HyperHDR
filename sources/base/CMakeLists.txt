# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/base)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/base)

FILE ( GLOB HyperHDR_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

SET(HyperHDR_RESOURCES ${CURRENT_SOURCE_DIR}/resource.qrc)

add_library(hyperhdr-base
	${HyperHDR_SOURCES}
	${HyperHDR_RESOURCES}
)

target_link_libraries(hyperhdr-base
	blackborder
	hyperhdr-utils
	flatbuffers_server
	flatbuffers_client
	flatbuffers
	leddevice
	effectengine
	database
	${QT_LIBRARIES}
)

if(ENABLE_BOBLIGHT)
	target_link_libraries(hyperhdr-base boblightserver)
endif()

if (ENABLE_BONJOUR)
	target_link_libraries(hyperhdr-base bonjour)
endif ()

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(hyperhdr-base REUSE_FROM precompiled_hyperhdr_headers)
endif()
