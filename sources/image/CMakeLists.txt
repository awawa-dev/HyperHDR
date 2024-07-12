# Define the current source locations

SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/image)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/image)

FILE ( GLOB_RECURSE image_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(image OBJECT ${image_SOURCES})

target_link_libraries(image
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(image REUSE_FROM precompiled_hyperhdr_headers)
endif()

