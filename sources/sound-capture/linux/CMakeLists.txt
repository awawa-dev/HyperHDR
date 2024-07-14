# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/sound-capture/linux)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/sound-capture/linux)

FILE ( GLOB SOUNDCAPLINUX_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(sound-capture-linux OBJECT ${SOUNDCAPLINUX_SOURCES} )

find_package(ALSA REQUIRED)

include_directories(${ALSA_INCLUDE_DIRS})

target_link_libraries(sound-capture-linux
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(sound-capture-linux REUSE_FROM precompiled_hyperhdr_headers)
endif()
