# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/grabber/SoundCapWindows)

FILE ( GLOB SOUNDCAPWINDOWS_SOURCES "${CURRENT_HEADER_DIR}/SoundCapW*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(SoundCapWindows ${SOUNDCAPWINDOWS_SOURCES} )

target_link_libraries(SoundCapWindows
	hyperhdr-base
	${QT_LIBRARIES}
)
