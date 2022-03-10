# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/v4l2)

FILE ( GLOB V4L2_SOURCES "${CURRENT_HEADER_DIR}/V4L2*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(v4l2-grabber ${V4L2_SOURCES} )

target_link_libraries(v4l2-grabber
	hyperhdr-base
	${QT_LIBRARIES}
)

target_include_directories(v4l2-grabber PUBLIC ${TURBOJPEG_INCLUDE_DIRS})
target_link_libraries(v4l2-grabber ${TURBOJPEG_LINK_LIBRARIES})
