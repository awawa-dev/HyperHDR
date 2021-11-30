# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/SoundCapMacOS)

FILE ( GLOB SOUNDCAPMACOS_SOURCES "${CURRENT_HEADER_DIR}/SoundCapMac*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )

add_library(SoundCapMacOS ${SOUNDCAPMACOS_SOURCES} )

target_link_libraries(SoundCapMacOS
	hyperhdr-base
	${QT_LIBRARIES}
)
