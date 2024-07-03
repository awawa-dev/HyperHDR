# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/sound-capture/macos)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/sound-capture/macos)

FILE ( GLOB SOUNDCAPMACOS_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )

add_library(sound-capture-macos OBJECT ${SOUNDCAPMACOS_SOURCES} )

target_link_libraries(sound-capture-macos
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers AND (NOT APPLE))
	target_precompile_headers(sound-capture-macos REUSE_FROM precompiled_hyperhdr_headers)
endif()
