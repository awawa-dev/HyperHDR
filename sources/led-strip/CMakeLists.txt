
# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/led-strip)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/led-strip)

FILE ( GLOB led_strip_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(led-strip	OBJECT ${led_strip_SOURCES} )

target_link_libraries(led-strip
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(led-strip REUSE_FROM precompiled_hyperhdr_headers)
endif()
