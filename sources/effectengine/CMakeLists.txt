# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/effectengine)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/effectengine)

FILE ( GLOB EffectEngineSOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(effectengine
	${EffectEngineSOURCES}
)

target_link_libraries(effectengine
	hyperhdr-base
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Gui
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(effectengine REUSE_FROM precompiled_hyperhdr_headers)
endif()
