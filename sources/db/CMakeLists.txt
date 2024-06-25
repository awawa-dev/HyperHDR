# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/db)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/db)

FILE ( GLOB DB_SOURCES "${CURRENT_HEADER_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(database OBJECT ${DB_SOURCES})

target_link_libraries(database
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
	sqlite3
)

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(database REUSE_FROM precompiled_hyperhdr_headers)
endif()
