# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/SoundCapLinux)

FILE ( GLOB SOUNDCAPLINUX_SOURCES "${CURRENT_HEADER_DIR}/SoundCapL*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(SoundCapLinux ${SOUNDCAPLINUX_SOURCES} )

find_package(ALSA REQUIRED)

include_directories(${ALSA_INCLUDE_DIRS})

target_link_libraries(SoundCapLinux
	hyperhdr-base
	${QT_LIBRARIES}
        ${ALSA_LIBRARIES}
)
