
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

if ( ENABLE_SPIDEV OR ENABLE_SPI_FTDI )
	FILE ( GLOB Leddevice_SPI_SOURCES "${CURRENT_HEADER_DIR}/spi/DriverSpi*.h" "${CURRENT_HEADER_DIR}/spi/ProviderSpiInterface.h" "${CURRENT_SOURCE_DIR}/spi/DriverSpi*.cpp" "${CURRENT_HEADER_DIR}/spi/ProviderSpi.h" "${CURRENT_SOURCE_DIR}/spi/ProviderSpi.cpp")
endif()

if ( ENABLE_SPIDEV )
	FILE ( GLOB Leddevice_ProviderSpiGeneric "${CURRENT_HEADER_DIR}/spi/ProviderSpiGeneric.h" "${CURRENT_SOURCE_DIR}/spi/ProviderSpiGeneric.cpp")
	list( APPEND Leddevice_SPI_SOURCES ${Leddevice_ProviderSpiGeneric})
endif()

if ( ENABLE_SPI_FTDI )
	if(WIN32)
		FILE ( GLOB Leddevice_ProviderSpiFtdi "${CURRENT_HEADER_DIR}/spi/ProviderSpiFtdi.h" "${CURRENT_HEADER_DIR}/spi/ftdi/ftd2xx.h" "${CURRENT_SOURCE_DIR}/spi/ProviderSpiFtdi.cpp")
		list( APPEND Leddevice_SPI_SOURCES ${Leddevice_ProviderSpiFtdi})
	else()
		FILE ( GLOB Leddevice_ProviderSpiLibFtdi "${CURRENT_HEADER_DIR}/spi/ProviderSpiLibFtdi.h" "${CURRENT_HEADER_DIR}/spi/ftdi/ftdi.h" "${CURRENT_HEADER_DIR}/spi/ftdi/libusb.h" "${CURRENT_SOURCE_DIR}/spi/ProviderSpiLibFtdi.cpp")
		list( APPEND Leddevice_SPI_SOURCES ${Leddevice_ProviderSpiLibFtdi})
	endif(WIN32)
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
	glm
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
