# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/MF)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/MF)

FILE ( GLOB MF_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(MF-grabber OBJECT ${MF_SOURCES} )

target_link_libraries(MF-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	TurboJPEG::TurboJPEG
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(MF-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()
