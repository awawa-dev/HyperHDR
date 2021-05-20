# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/cec)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/cec)

FILE ( GLOB CECHANDLER_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(cechandler ${CECHANDLER_SOURCES})

target_include_directories(cechandler PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/external/libcec/include")
target_link_libraries(cechandler
	${CEC_LIBRARIES}
	Qt${Qt_VERSION}::Network
	Qt${Qt_VERSION}::Core
)


