# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/bonjour)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/bonjour)

FILE ( GLOB Bonjour_SOURCES "${CURRENT_HEADER_DIR}/*.h"
							"${CURRENT_SOURCE_DIR}/*.cpp"
							"${CMAKE_SOURCE_DIR}/dependencies/bonjour/mdns.h")


add_library(bonjour ${Bonjour_SOURCES} )

target_link_libraries(bonjour
		hyperhdr-base
		hyperhdr-utils
		Qt${Qt_VERSION}::Network
)
