# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/hyperimage)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/hyperimage)

FILE ( GLOB HYPERIMAGE_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp")

add_library(hyperimage OBJECT ${HYPERIMAGE_SOURCES} )

target_link_libraries(hyperimage
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(hyperimage REUSE_FROM precompiled_hyperhdr_headers)
endif()
