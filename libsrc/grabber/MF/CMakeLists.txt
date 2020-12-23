# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/grabber/MF)

FILE ( GLOB QTC_SOURCES "${CURRENT_HEADER_DIR}/MF*.h"  "${CURRENT_SOURCE_DIR}/*.h"  "${CURRENT_SOURCE_DIR}/*.cpp" )

add_library(qtc-grabber ${QTC_SOURCES} )

target_link_libraries(qtc-grabber
	hyperion
	${QT_LIBRARIES}
)

if(TURBOJPEG_FOUND)
	target_link_libraries(qtc-grabber ${TurboJPEG_LIBRARY})
endif(TURBOJPEG_FOUND)
