
# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/led-drivers)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/sources/led-drivers)

IF ( Qt5SerialPort_FOUND OR Qt6SerialPort_FOUND )
	SET( HAVE_SERIAL_LED ON) 
	MESSAGE (STATUS "Found Qt SerialPort library")
	include_directories( dev_serial )
else()
	MESSAGE (WARNING "Not found Qt SerialPort Library. Serial devices are disabled.")
endif()

FILE ( GLOB Leddevice_SOURCES
	"${CURRENT_HEADER_DIR}/*.h"
	"${CURRENT_SOURCE_DIR}/*.cpp"		
	"${CURRENT_HEADER_DIR}/net/*.h"
	"${CURRENT_SOURCE_DIR}/net/*.cpp"
	"${CURRENT_HEADER_DIR}/other/*.h"
	"${CURRENT_SOURCE_DIR}/other/*.cpp"
)

IF ( HAVE_SERIAL_LED )	
	FILE ( GLOB Leddevice_SERIAL_SOURCES "${CURRENT_HEADER_DIR}/serial/*.h" "${CURRENT_SOURCE_DIR}/serial/*.cpp")
endif()

if (UNIX AND NOT APPLE)
	FILE ( GLOB Leddevice_PWM_SOURCES "${CURRENT_HEADER_DIR}/pwm/pi-blaster/*.h" "${CURRENT_SOURCE_DIR}/pwm/pi-blaster/*.cpp")
endif()

if ( ENABLE_SPIDEV )
	FILE ( GLOB Leddevice_SPI_SOURCES "${CURRENT_HEADER_DIR}/spi/*.h" "${CURRENT_SOURCE_DIR}/spi/*.cpp")
endif()

if ( ENABLE_WS281XPWM )
	include_directories(../../external/rpi_ws281x)
	FILE ( GLOB Leddevice_rpi_ws281x "${CURRENT_HEADER_DIR}/pwm/rpi_ws281x/*.h" "${CURRENT_SOURCE_DIR}/pwm/rpi_ws281x/*.cpp")
	list( APPEND Leddevice_PWM_SOURCES ${Leddevice_rpi_ws281x})
endif()

set(LedDevice_RESOURCES ${CURRENT_SOURCE_DIR}/LedDeviceSchemas.qrc )

SET( Leddevice_SOURCES
	${Leddevice_SOURCES}
	${LedDevice_RESOURCES}
	${Leddevice_SERIAL_SOURCES}
	${Leddevice_SPI_SOURCES}
	${Leddevice_PWM_SOURCES}
)

add_library(led-drivers OBJECT ${Leddevice_SOURCES} )

target_link_libraries(led-drivers
	Qt${Qt_VERSION}::Core
	Qt${Qt_VERSION}::Network
)

IF ( HAVE_SERIAL_LED )
	target_link_libraries(led-drivers Qt${Qt_VERSION}::SerialPort)
endif()

if(WIN32)
	target_link_libraries(led-drivers ws2_32)
endif()

if(ENABLE_WS281XPWM)
	target_link_libraries(led-drivers ws2811)
endif()

if (USE_STATIC_QT_PLUGINS)
	target_link_libraries(led-drivers ${STATIC_QT_PLUGINS_LIBS})
endif()

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
	target_precompile_headers(led-drivers REUSE_FROM precompiled_hyperhdr_headers)
endif()
