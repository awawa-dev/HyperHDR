# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/mqtt)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/mqtt)

FILE ( GLOB mqtt_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(mqtt OBJECT ${mqtt_SOURCES} )

target_link_libraries(mqtt
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	qmqtt
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers( mqtt REUSE_FROM precompiled_hyperhdr_headers)
endif()
