# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/macOS)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/macOS)

FILE ( GLOB MACOS_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )

add_library(MACOS-grabber ${MACOS_SOURCES} )

target_link_libraries(MACOS-grabber
	hyperhdr-base
	${QT_LIBRARIES}
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(MACOS-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
