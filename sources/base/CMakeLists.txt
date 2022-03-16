
# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/base)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/base)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}/../../sources/flatbufserver
)

FILE ( GLOB HyperHDR_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

SET(HyperHDR_RESOURCES ${CURRENT_SOURCE_DIR}/resource.qrc)

add_library(hyperhdr-base
	${HyperHDR_SOURCES}
	${HyperHDR_RESOURCES}
)

target_link_libraries(hyperhdr-base
	blackborder
	hyperhdr-utils
	flatbufserver
	flatbuffers
	leddevice
	effectengine
	database
	${QT_LIBRARIES}
)

if(ENABLE_BOBLIGHT)
	target_link_libraries(hyperhdr-base boblightserver)
endif()

if (ENABLE_AVAHI)
	target_link_libraries(hyperhdr-base bonjour)
endif ()
