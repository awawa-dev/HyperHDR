# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/osx/macOS)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/osx/macOS)

FILE ( GLOB MACOS_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )

add_library(MACOS-grabber OBJECT ${MACOS_SOURCES} )

target_link_libraries(MACOS-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(MACOS-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()

if(APPLE)
	set_source_files_properties(${CURRENT_SOURCE_DIR}/macOsGrabber.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
