# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/systray)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/systray)

FILE ( GLOB SYSTRAY_SOURCES "${CURRENT_HEADER_DIR}/*.h" )

if(WIN32)
	list(APPEND SYSTRAY_SOURCES ${CURRENT_SOURCE_DIR}/SystrayWindows.cpp)	
else()
	if(APPLE)
		list(APPEND SYSTRAY_SOURCES ${CURRENT_SOURCE_DIR}/SystrayMacOS.mm)
	else()
		list(APPEND SYSTRAY_SOURCES ${CURRENT_SOURCE_DIR}/SystrayLinux.cpp)
	endif()
endif()

add_library(systray ${SYSTRAY_SOURCES} )




