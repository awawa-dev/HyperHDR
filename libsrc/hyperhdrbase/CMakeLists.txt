
# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/hyperhdrbase)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/hyperhdrbase)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}/../../libsrc/flatbufserver
)

FILE ( GLOB Hyperion_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

SET(Hyperion_RESOURCES ${CURRENT_SOURCE_DIR}/resource.qrc)

add_library(hyperhdr-base
	${Hyperion_SOURCES}
	${Hyperion_RESOURCES}
)

target_link_libraries(hyperhdr-base
	blackborder
	hyperhdr-utils
	flatbufserver
	flatbuffers
	leddevice
	boblightserver
	effectengine
	database
	${QT_LIBRARIES}
)

if (ENABLE_AVAHI)
	target_link_libraries(hyperhdr-base bonjour)
endif ()
