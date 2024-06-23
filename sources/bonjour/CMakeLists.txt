# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/bonjour)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/bonjour)

FILE ( GLOB Bonjour_SOURCES "${CURRENT_HEADER_DIR}/*.h"
							"${CURRENT_SOURCE_DIR}/*.cpp")

add_library(bonjour OBJECT ${Bonjour_SOURCES} )

target_include_directories(bonjour PRIVATE ${CMAKE_SOURCE_DIR}/external/mdns)

target_link_libraries(bonjour
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(bonjour REUSE_FROM precompiled_hyperhdr_headers)
endif()
