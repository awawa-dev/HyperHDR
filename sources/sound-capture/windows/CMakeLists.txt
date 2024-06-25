# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/sound-capture/windows)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/sound-capture/windows)

FILE ( GLOB SOUNDCAPWINDOWS_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(sound-capture-windows OBJECT ${SOUNDCAPWINDOWS_SOURCES} )

target_link_libraries(sound-capture-windows
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(sound-capture-windows REUSE_FROM precompiled_hyperhdr_headers)
endif()
