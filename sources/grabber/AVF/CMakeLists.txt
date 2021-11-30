# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/grabber/AVF)

FILE ( GLOB AVF_SOURCES "${CURRENT_HEADER_DIR}/AVF*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" "${CURRENT_SOURCE_DIR}/*.mm" )

add_library(AVF-grabber ${AVF_SOURCES} )

target_link_libraries(AVF-grabber
	hyperhdr-base
	${QT_LIBRARIES}
)

if(TURBOJPEG_FOUND)
	target_link_libraries(AVF-grabber ${TurboJPEG_LIBRARY})
endif(TURBOJPEG_FOUND)
