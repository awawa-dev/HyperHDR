set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/flatbuffers)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/flatbuffers)

########################################### FLATBUFFERS #####################################################

set(Flatbuffer_GENERATED_FBS
	${CURRENT_HEADER_DIR}/parser/hyperhdr_reply_generated.h
	${CURRENT_HEADER_DIR}/parser/hyperhdr_request_generated.h
)

set(Flatbuffer_FBS
	${CURRENT_HEADER_DIR}/parser/hyperhdr_reply.fbs
	${CURRENT_HEADER_DIR}/parser/hyperhdr_request.fbs
)

FOREACH(FBS_FILE ${Flatbuffer_FBS})
	compile_flattbuffer_schema(${FBS_FILE} "${CURRENT_HEADER_DIR}/parser")
ENDFOREACH(FBS_FILE)

set_source_files_properties(
	${Flatbuffer_GENERATED_FBS} PROPERTIES GENERATED TRUE
)


############################################# PARSER #######################################################

FILE ( GLOB FLATBUF_PARSER_SOURCES "${CURRENT_HEADER_DIR}/parser/*.h" "${CURRENT_SOURCE_DIR}/parser/*.cpp" )

include_directories("${CURRENT_HEADER_DIR}/parser")

add_library(flatbuffers-parser OBJECT
	${FLATBUF_PARSER_SOURCES}
	${Flatbuffer_GENERATED_FBS}
)

target_link_libraries(flatbuffers-parser PRIVATE
	flatbuffers
)

############################################# SERVER #######################################################

FILE ( GLOB FLATBUF_SERVER_SOURCES "${CURRENT_HEADER_DIR}/server/*.h" "${CURRENT_SOURCE_DIR}/server/*.cpp" )

include_directories("${CURRENT_HEADER_DIR}/server")

add_library(flatbuffers-server OBJECT
	${FLATBUF_SERVER_SOURCES}
)

target_link_libraries(flatbuffers-server
	flatbuffers-parser
	Qt${Qt_VERSION}::Network
	Qt${Qt_VERSION}::Core
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(flatbuffers-server REUSE_FROM precompiled_hyperhdr_headers)
endif()

############################################# CLIENT #######################################################

FILE ( GLOB FLATBUF_CLIENT_SOURCES "${CURRENT_HEADER_DIR}/client/*.h"  "${CURRENT_SOURCE_DIR}/client/*.cpp" )

include_directories("${CURRENT_HEADER_DIR}/client")

add_library(flatbuffers-client OBJECT
	${FLATBUF_CLIENT_SOURCES}
)

target_link_libraries(flatbuffers-client
	flatbuffers-parser
	Qt${Qt_VERSION}::Network
	Qt${Qt_VERSION}::Core
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(flatbuffers-client REUSE_FROM precompiled_hyperhdr_headers)
endif()
