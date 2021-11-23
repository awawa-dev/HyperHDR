
# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/protoserver)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/protoserver)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${PROTOBUF_INCLUDE_DIRS}
)

set(ProtoServer_PROTOS ${CURRENT_SOURCE_DIR}/message.proto )

protobuf_generate_cpp(ProtoServer_PROTO_SRCS ProtoServer_PROTO_HDRS ${ProtoServer_PROTOS} )
set_property(SOURCE ${CMAKE_SOURCE_DIR}/build/libsrc/protoserver/message.pb.cc PROPERTY SKIP_AUTOMOC ON)
set_property(SOURCE ${CMAKE_SOURCE_DIR}/build/libsrc/protoserver/message.pb.h PROPERTY SKIP_AUTOMOC ON)
### Split protoclient from protoserver as protoserver relates to HyperHDRDaemon and standalone capture binarys can't link to it

add_library(protoclient
	${CURRENT_SOURCE_DIR}/ProtoClientConnection.h
	${CURRENT_SOURCE_DIR}/ProtoClientConnection.cpp
	${ProtoServer_PROTO_SRCS}
	${ProtoServer_PROTO_HDRS}
)

add_library(protoserver
	${CURRENT_HEADER_DIR}/ProtoServer.h
	${CURRENT_SOURCE_DIR}/ProtoServer.cpp
)

# disable warnings for auto generated proto files, we can't change the files ....
SET_SOURCE_FILES_PROPERTIES ( ${ProtoServer_PROTO_SRCS} ${ProtoServer_PROTO_HDRS} ${ProtoServer_PROTOS} PROPERTIES COMPILE_FLAGS -w )

target_link_libraries(protoclient
	hyperhdr-base
	hyperhdr-utils
	${PROTOBUF_LIBRARY}
	Qt${Qt_VERSION}::Gui
)

target_link_libraries(protoserver
	hyperhdr-base
	hyperhdr-utils
	protoclient
	Qt${Qt_VERSION}::Gui
)
