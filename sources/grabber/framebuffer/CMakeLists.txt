# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/framebuffer)

# Grabber
FILE ( GLOB FRAMEBUFFER_SOURCES "${CURRENT_HEADER_DIR}/FrameBuf*.h" "${CURRENT_SOURCE_DIR}/FrameBufGrabber.cpp" "${CURRENT_SOURCE_DIR}/FrameBufWrapper.cpp" )

add_library(framebuffer-grabber ${FRAMEBUFFER_SOURCES} )

target_link_libraries(framebuffer-grabber hyperhdr-base ${QT_LIBRARIES} )
