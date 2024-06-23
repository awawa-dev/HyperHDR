# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/linux/framebuffer)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/linux/framebuffer)

# Grabber
FILE ( GLOB FRAMEBUFFER_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(framebuffer-grabber OBJECT ${FRAMEBUFFER_SOURCES} )

target_link_libraries(framebuffer-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(framebuffer-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
