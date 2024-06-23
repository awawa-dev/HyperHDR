# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/cec)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/cec)

FILE ( GLOB CECHANDLER_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(cechandler OBJECT ${CECHANDLER_SOURCES})

target_include_directories(cechandler PUBLIC ${CEC_INCLUDE_DIRS})

target_link_libraries(cechandler
	${CEC_LIBRARIES}
	Qt${Qt_VERSION}::Network
	Qt${Qt_VERSION}::Core
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(cechandler REUSE_FROM precompiled_hyperhdr_headers)
endif()



