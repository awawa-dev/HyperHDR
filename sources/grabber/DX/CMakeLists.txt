# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/DX)

FILE ( GLOB DX_SOURCES "${CURRENT_HEADER_DIR}/Dx*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(DX-grabber ${DX_SOURCES} )

target_link_libraries(DX-grabber
	hyperhdr-base
	${QT_LIBRARIES}
)
