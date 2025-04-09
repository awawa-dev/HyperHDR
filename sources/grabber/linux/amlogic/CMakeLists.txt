# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber/linux/amlogic)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/linux/amlogic)

# Grabber
FILE ( GLOB AMLOGIC_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(amlogic-grabber OBJECT ${AMLOGIC_SOURCES} )

target_link_libraries(amlogic-grabber
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(amlogic-grabber REUSE_FROM precompiled_hyperhdr_headers)
endif()

