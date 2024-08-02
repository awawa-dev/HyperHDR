# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/windows/MF)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/windows/MF)

FILE ( GLOB GRABBER_MF_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(grabber-MF OBJECT ${GRABBER_MF_SOURCES} )

target_link_libraries(grabber-MF
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	TurboJPEG::TurboJPEG
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(grabber-MF REUSE_FROM precompiled_hyperhdr_headers)
endif()
