# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/windows/DX)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/windows/DX)

FILE ( GLOB GRABBER_DX_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(grabber-DX OBJECT ${GRABBER_DX_SOURCES} )

target_link_libraries(grabber-DX
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(grabber-DX REUSE_FROM precompiled_hyperhdr_headers)
endif()
