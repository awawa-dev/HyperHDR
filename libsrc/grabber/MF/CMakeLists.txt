# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/grabber/MF)

FILE ( GLOB WMF_SOURCES "${CURRENT_HEADER_DIR}/MF*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(WMF-grabber ${WMF_SOURCES} )

target_link_libraries(WMF-grabber
	hyperhdr-base
	${QT_LIBRARIES}
)

if(TURBOJPEG_FOUND)
	target_link_libraries(WMF-grabber ${TurboJPEG_LIBRARY})
endif(TURBOJPEG_FOUND)
