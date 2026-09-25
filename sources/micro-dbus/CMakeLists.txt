# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/micro-dbus)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/micro-dbus)

FILE ( GLOB MICRO_DBUS_SOURCES "${CURRENT_HEADER_DIR}/*.h" "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(micro-dbus OBJECT ${MICRO_DBUS_SOURCES})

target_include_directories(micro-dbus PUBLIC ${DBUS_INCLUDE_DIRS})
target_link_directories(micro-dbus PUBLIC ${DBUS_LIBRARY_DIRS})
target_link_libraries(micro-dbus
	Qt${Qt_VERSION}::Network
	Qt${Qt_VERSION}::Core
	${DBUS_LIBRARIES}
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(micro-dbus REUSE_FROM precompiled_hyperhdr_headers)
endif()



