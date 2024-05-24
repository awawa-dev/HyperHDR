# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/v4l2)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/v4l2)

FILE ( GLOB V4L2_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(v4l2-grabber ${V4L2_SOURCES} )

target_link_libraries(v4l2-grabber
	hyperhdr-base
	TurboJPEG::TurboJPEG
	${QT_LIBRARIES}
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(v4l2-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
