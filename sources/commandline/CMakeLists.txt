# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/commandline)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/commandline)

FILE ( GLOB Parser_SOURCES "${CURRENT_HEADER_DIR}/BooleanOption.h" "${CURRENT_SOURCE_DIR}/BooleanOption.cpp"
			   "${CURRENT_HEADER_DIR}/Option.h" "${CURRENT_SOURCE_DIR}/Option.cpp"
			   "${CURRENT_HEADER_DIR}/Parser.h" "${CURRENT_SOURCE_DIR}/Parser.cpp"
)

add_library(commandline OBJECT ${Parser_SOURCES} )

target_link_libraries(commandline
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(commandline REUSE_FROM precompiled_hyperhdr_headers)
endif()
