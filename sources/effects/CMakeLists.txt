# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/effects)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/effects)

FILE ( GLOB EffectsSOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(effects OBJECT ${EffectsSOURCES})

target_link_libraries(effects
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(effects REUSE_FROM precompiled_hyperhdr_headers)
endif()
