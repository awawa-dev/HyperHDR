# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/linux/v4l2)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/linux/v4l2)

FILE ( GLOB V4L2_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(v4l2-grabber OBJECT ${V4L2_SOURCES} )

target_link_libraries(v4l2-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	TurboJPEG::TurboJPEG
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(v4l2-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
