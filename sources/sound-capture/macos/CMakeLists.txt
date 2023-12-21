# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/sound-capture/macos)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/sound-capture/macos)

FILE ( GLOB SOUNDCAPMACOS_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )

add_library(sound-capture-macos ${SOUNDCAPMACOS_SOURCES} )

target_link_libraries(sound-capture-macos
	hyperhdr-base
	${QT_LIBRARIES}
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(sound-capture-macos REUSE_FROM precompiled_hyperhdr_headers)
endif()
