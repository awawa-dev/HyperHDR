# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/proto-nano-server)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/proto-nano-server)

add_library(proto-nano-server
	${CURRENT_SOURCE_DIR}/ProtoNanoClientConnection.cpp
	${CURRENT_HEADER_DIR}/ProtoNanoClientConnection.h
	${CURRENT_SOURCE_DIR}/message.pb.c
	${CURRENT_HEADER_DIR}/message.pb.h	
	${CURRENT_SOURCE_DIR}/ProtoServer.cpp
	${CURRENT_HEADER_DIR}/ProtoServer.h
)

set_source_files_properties(${CURRENT_SOURCE_DIR}/message.pb.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_include_directories(proto-nano-server PUBLIC ${CURRENT_HEADER_DIR})

target_link_libraries(proto-nano-server PRIVATE
	protobuf-nanopb
	hyperhdr-base
	hyperhdr-utils
	Qt${Qt_VERSION}::Core
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(proto-nano-server REUSE_FROM precompiled_hyperhdr_headers)
endif()
