# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/osx/AVF)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/osx/AVF)

FILE ( GLOB AVF_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )

add_library(AVF-grabber OBJECT ${AVF_SOURCES} )

target_link_libraries(AVF-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(AVF-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()

if(APPLE)
	set_source_files_properties(${CURRENT_SOURCE_DIR}/AVFGrabber.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
