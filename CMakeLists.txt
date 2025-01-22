if (WIN32)
	cmake_minimum_required(VERSION 3.25.0)
	cmake_policy(SET CMP0141 NEW)
else()
	cmake_minimum_required(VERSION 3.16.0)
endif()

message( STATUS "CMake Version: ${CMAKE_VERSION}" )

PROJECT(HyperHdr)


set(OUR_CMAKE_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_modules)
set(CMAKE_MODULE_PATH ${OUR_CMAKE_MODULES} ${CMAKE_MODULE_PATH})

# print version
include (version)
file (STRINGS "version" HYPERHDR_VERSION)
SetVersionNumber(HYPERHDR ${HYPERHDR_VERSION})


# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# auto prepare .qrc files
set(CMAKE_AUTORCC ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set build variables
SET ( DEFAULT_PROTOBUF                    ON  )
SET ( DEFAULT_WS281XPWM                   OFF )
SET ( DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS ON  )
SET ( DEFAULT_USE_SYSTEM_MQTT_LIBS        OFF )
SET ( DEFAULT_MF                          OFF )
SET ( DEFAULT_DX                          OFF )
SET ( DEFAULT_AVF                         OFF )
SET ( DEFAULT_MAC_SYSTEM                  OFF )
SET ( DEFAULT_V4L2                        OFF )
SET ( DEFAULT_X11                         OFF )
SET ( DEFAULT_PIPEWIRE                    OFF )
SET ( DEFAULT_PIPEWIRE_EGL                OFF )
SET ( DEFAULT_FRAMEBUFFER                 OFF )
SET ( DEFAULT_SOUNDCAPWINDOWS             OFF )
SET ( DEFAULT_SOUNDCAPMACOS               OFF )
SET ( DEFAULT_CEC                         OFF )
SET ( DEFAULT_BONJOUR                     ON  )
SET ( DEFAULT_MQTT                        ON  )
SET ( DEFAULT_STATIC_QT_PLUGINS           OFF )
SET ( DEFAULT_PRECOMPILED_HEADERS         ON  )
SET ( DEFAULT_ZSTD                        ON  )
SET ( DEFAULT_POWER_MANAGEMENT            ON  )
SET ( DEFAULT_SYSTRAY                     ON  )
SET ( DEFAULT_SHARED_LIBS                 ON  )
SET ( DEFAULT_EMBEDDED_WEB_RESOURCES      OFF )

# Configure CCache if available
option(USE_CCACHE_CACHING "Enable CCache caching" ON )
if (USE_CCACHE_CACHING)
	find_program(CCACHE_FOUND ccache HINTS "${CMAKE_CURRENT_SOURCE_DIR}/build")
	if ( CCACHE_FOUND )		
		if (DEFINED ENV{CCACHE_DIR})
			set (CCACHE_DIR_CACHE_LOCATION $ENV{CCACHE_DIR})
			message ( STATUS "Found CCache (${CCACHE_FOUND}) and its env settings: ${CCACHE_DIR_CACHE_LOCATION}. Will try to use it." )			
		elseif (WIN32 AND (NOT CMAKE_GITHUB_ACTION))
			set (CCACHE_DIR_CACHE_LOCATION "$ENV{AppData}/.ccache")
			message ( STATUS "Found CCache (${CCACHE_FOUND}) and set ccache dir: ${CCACHE_DIR_CACHE_LOCATION}. Will try to use it." )
		else()
			set (CCACHE_DIR_CACHE_LOCATION "${CMAKE_BINARY_DIR}/.ccache")
			message ( STATUS "Found CCache (${CCACHE_FOUND}) and set ccache dir: ${CCACHE_DIR_CACHE_LOCATION}. Will try to use it." )
		endif()

		if (WIN32)
			file(COPY_FILE
				${CCACHE_FOUND} ${CMAKE_BINARY_DIR}/cl.exe
				ONLY_IF_DIFFERENT
			)
			set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
			message ( STATUS "Writing CCache config to user profile: $ENV{USERPROFILE}/.ccache/ccache.conf")
			configure_file(${OUR_CMAKE_MODULES}/ccache.windows.conf.in $ENV{USERPROFILE}/.ccache/ccache.conf)
			configure_file(${OUR_CMAKE_MODULES}/Directory.Build.props.in ${CMAKE_BINARY_DIR}/Directory.Build.props)
		else()
			set(HYPERHDR_CCACHE_PARAMS
				CCACHE_MAXSIZE=600M
				CCACHE_NOCOMPRESS=true
				CCACHE_DIR=${CCACHE_DIR_CACHE_LOCATION}
				CCACHE_SLOPPINESS=pch_defines,time_macros
				CCACHE_INODECACHE=true
				CACHE STRING "CCache parameters")
			mark_as_advanced(HYPERHDR_CCACHE_PARAMS)

			set(CMAKE_C_COMPILER_LAUNCHER ${CMAKE_COMMAND} -E env ${HYPERHDR_CCACHE_PARAMS} ${CCACHE_FOUND})
			set(CMAKE_CXX_COMPILER_LAUNCHER ${CMAKE_COMMAND} -E env ${HYPERHDR_CCACHE_PARAMS} ${CCACHE_FOUND})

			message(STATUS "Using CCache for compilation. It is invoked as: ${CMAKE_CXX_COMPILER_LAUNCHER}")
		endif()
	else()
		set( USE_CCACHE_CACHING           OFF )
		message ( STATUS "CCache is not found" )
	endif(CCACHE_FOUND)
endif(USE_CCACHE_CACHING)

# find QT libs
find_package(Qt6 COMPONENTS Core Network SerialPort QUIET)

if (Qt6Core_FOUND AND NOT (DO_NOT_USE_QT_VERSION_6_LIBS STREQUAL "ON"))	
	message( STATUS "Found Qt Version: ${Qt6Core_VERSION}" )
	SET( Qt_VERSION 6 )	
ELSE()
	if (DO_NOT_USE_QT_VERSION_6_LIBS STREQUAL "ON")
		message( STATUS "Search for QT version 6 is disabled: QT5 is preferred. Enable QT6 by setting DO_NOT_USE_QT_VERSION_6_LIBS to OFF." )
	else()
		message( STATUS "QT version 6 not found. Searching for QT version 5 instead." )
	endif()
	SET(QT_MIN_VERSION "5.10.0")
	find_package(Qt5 COMPONENTS Core Network SerialPort REQUIRED)
	message( STATUS "Found Qt Version: ${Qt5Core_VERSION}" )
	IF ( "${Qt5Core_VERSION}" VERSION_LESS "${QT_MIN_VERSION}" )
		message( FATAL_ERROR "Your Qt version is to old! Minimum required ${QT_MIN_VERSION}" )
	ENDIF()
	SET( Qt_VERSION 5 )	
ENDIF()

IF ( ${CMAKE_SYSTEM} MATCHES "Linux" )	
	SET ( DEFAULT_SOUNDCAPLINUX ON )
	SET ( DEFAULT_V4L2          ON )
	SET ( DEFAULT_X11           ON )
	SET ( DEFAULT_FRAMEBUFFER   ON )
	SET ( DEFAULT_PIPEWIRE      ON )
	SET ( DEFAULT_SPIDEV        ON )
	message( STATUS "Debian version: ${DEBIAN_NAME_TAG}" )
ELSEIF ( WIN32 )
	SET ( DEFAULT_DX              ON  )
	SET ( DEFAULT_MF              ON  )
	SET ( DEFAULT_SOUNDCAPWINDOWS ON  )
ELSE()
	SET ( DEFAULT_SPIDEV      OFF )
ENDIF()

if ( NOT DEFINED PLATFORM )
	if ( APPLE )
		SET( PLATFORM "osx")
	elseif ( WIN32 )
		SET( PLATFORM "windows")
	elseif ( "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86" )
		SET( PLATFORM "linux")
	elseif ( "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "arm" OR "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
		SET( PLATFORM "rpi")
		

		FILE( READ /proc/cpuinfo SYSTEM_CPUINFO )
		STRING ( TOLOWER "${SYSTEM_CPUINFO}" SYSTEM_CPUINFO )
		if ( "${SYSTEM_CPUINFO}" MATCHES "amlogic" AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4 )
			SET( PLATFORM "amlogic" )
		elseif ( ("${SYSTEM_CPUINFO}" MATCHES "amlogic" OR "${SYSTEM_CPUINFO}" MATCHES "odroid-c2" OR "${SYSTEM_CPUINFO}" MATCHES "vero4k") AND ${CMAKE_SIZEOF_VOID_P} EQUAL 8 )
			SET( PLATFORM "amlogic64" )		
		endif()
	endif()
	if ( PLATFORM )
		message( STATUS "PLATFORM is not defined, evaluated platform: ${PLATFORM}")
	else()
		message( FATAL_ERROR "PLATFORM is not defined and could not be evaluated. Set -DPLATFORM=<rpi|amlogic|amlogic64|linux|linux-dev|osx|osx-dev>")
	endif()
endif()

message( STATUS "PLATFORM: ${PLATFORM}")

# Macro to get path of first sub dir of a dir, used for MAC OSX lib/header searching
MACRO(FIRSTSUBDIR result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist "${curdir}/${child}")
	  	BREAK()
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

if ( "${PLATFORM}" MATCHES "osx" )
	SET ( DEFAULT_SOUNDCAPMACOS ON )
	
	include_directories("/opt/X11/include/")
	
	if(APPLE AND EXISTS /usr/local/opt/qt6)			
		INCLUDE(CheckCXXCompilerFlag)
		CHECK_CXX_COMPILER_FLAG("-Werror=unguarded-availability" REQUIRES_UNGUARDED_AVAILABILITY)
		IF(REQUIRES_UNGUARDED_AVAILABILITY)
			LIST(APPEND CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} "-Werror=unguarded-availability")
		ENDIF()
	
		list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt6")
	elseif(APPLE AND EXISTS /usr/local/opt/qt5)	
		list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt5")
	endif()
	
	SET ( DEFAULT_AVF                    ON  )	
	SET ( DEFAULT_EMBEDDED_WEB_RESOURCES OFF )

	if (CMAKE_OSX_DEPLOYMENT_TARGET)
		set(CMAKE_OSX_HYPERHDR_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
	else()
		string(REGEX MATCH "MacOSX([0-9]+)" _ ${CMAKE_OSX_SYSROOT}) 
		if (${CMAKE_MATCH_COUNT} EQUAL "1")
			set(CMAKE_OSX_HYPERHDR_DEPLOYMENT_TARGET ${CMAKE_MATCH_1})
		endif()
	endif(CMAKE_OSX_DEPLOYMENT_TARGET)

	if (CMAKE_OSX_HYPERHDR_DEPLOYMENT_TARGET)
		message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET: ${CMAKE_OSX_HYPERHDR_DEPLOYMENT_TARGET}")
		SET(CMAKE_OSX_DEPLOYMENT_TARGET_COPY ${CMAKE_OSX_HYPERHDR_DEPLOYMENT_TARGET})
		string(REPLACE "." ";" CMAKE_OSX_DEPLOYMENT_TARGET_COPY_LIST ${CMAKE_OSX_DEPLOYMENT_TARGET_COPY})
		list(GET CMAKE_OSX_DEPLOYMENT_TARGET_COPY_LIST 0 MACOS_MAJOR_VERSION)
		message(STATUS "MAJOR MACOS VERSION: ${MACOS_MAJOR_VERSION}")

		if (${MACOS_MAJOR_VERSION} GREATER_EQUAL "15")
			message(STATUS "Looking for: ScreenCaptureKit")
			find_library(MACOS_SCK ScreenCaptureKit)
			if (MACOS_SCK)
				SET ( DEFAULT_MAC_SYSTEM             ON  )
			else()
				message(WARNING "Could not find: ScreenCaptureKit")
			endif(MACOS_SCK)
		else()
			SET ( DEFAULT_MAC_SYSTEM             ON  )
		endif()
	endif(CMAKE_OSX_HYPERHDR_DEPLOYMENT_TARGET)

elseif ( "${PLATFORM}" MATCHES "rpi" )	
	SET ( DEFAULT_WS281XPWM  ON  )
	SET ( DEFAULT_CEC        ON  )
	SET ( DEFAULT_SYSTRAY    OFF )
	
	# GL dispatcher
	find_library (GLD GLdispatch)
	if(NOT GLD AND NOT CMAKE_CROSSCOMPILING)
		message(FATAL_ERROR "GLdispatch library not found. Install libglvnd-dev")
	endif()
		

	message("Found: ${GLD}")
	add_library(GLD SHARED IMPORTED)

	if(EXISTS "/sys/firmware/devicetree/base/model" OR EXISTS "/proc/device-tree/model")
		if(EXISTS "/sys/firmware/devicetree/base/model")
			file(STRINGS "/sys/firmware/devicetree/base/model" ARM_MODEL NEWLINE_CONSUME)
		else()
			file(STRINGS "/proc/device-tree/model" ARM_MODEL NEWLINE_CONSUME)
		endif()
		
		STRING ( TOLOWER "${ARM_MODEL}" ARM_MODEL)
		message(STATUS "Arm model info: ${ARM_MODEL}")
	
		string(FIND "${ARM_MODEL}" "raspberry" IS_RASPBERRY)
		if(${IS_RASPBERRY} EQUAL -1)
			set (IS_RASPBERRY_PI OFF)
		else()
			set (IS_RASPBERRY_PI ON)
		endif()
		message(STATUS "Check if Raspberry Pi: ${IS_RASPBERRY_PI}")
	else()
		message(WARNING "Model info not exists. Assuming that it is Raspberry Pi")
		SET (IS_RASPBERRY_PI  ON)
	endif()

	if (IS_RASPBERRY_PI)
		message(STATUS "Platform variant: Raspberry Pi")
		SET ( DEFAULT_POWER_MANAGEMENT            ON )
		SET ( DEFAULT_PIPEWIRE                    ON )
	else()
		message(STATUS "Platform variant: generic ARM")
	endif()
	
elseif ( "${PLATFORM}" STREQUAL "amlogic" )
	SET ( DEFAULT_AMLOGIC    ON )
elseif ( "${PLATFORM}" STREQUAL "amlogic64" )
	SET ( DEFAULT_AMLOGIC    ON )
elseif ( "${PLATFORM}" MATCHES "linux" )
	if ( "${PLATFORM}" STREQUAL "linux-dev" )
		SET ( DEFAULT_AMLOGIC    ON)
		SET ( DEFAULT_WS281XPWM  ON )
	endif()
elseif ( "${PLATFORM}" STREQUAL "imx6" )
	
elseif (WIN32)
	SET ( DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS OFF )
endif()

STRING( TOUPPER "-DPLATFORM_${PLATFORM}" PLATFORM_DEFINE)
STRING( REPLACE "-DEV" "" PLATFORM_DEFINE "${PLATFORM_DEFINE}" )
ADD_DEFINITIONS( ${PLATFORM_DEFINE} )

if (DEFAULT_X11)
	if (NOT CMAKE_CROSSCOMPILING)
		find_package(XLibs QUIET )
	endif()	
	if (NOT XLibs_FOUND)
		message( WARNING "X11 libs not found. Disabling support for X11.")
		SET ( DEFAULT_X11                          OFF )
	endif()
endif()

if (DEFAULT_SYSTRAY)
	if(UNIX AND NOT APPLE)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(AppIndicator ayatana-appindicator3-0.1)
		if (NOT AppIndicator_FOUND OR NOT AppIndicator_INCLUDE_DIRS OR NOT AppIndicator_LIBRARIES)
			message( WARNING "AppIndicator dev package (eg. libayatana-appindicator3-dev) not found. Disabling support for the systray.")
			SET ( DEFAULT_SYSTRAY                          OFF )
		endif()
	endif()
endif()

if (DEFAULT_FRAMEBUFFER)
	include(CheckIncludeFile)
	check_include_file("linux/fb.h" HAVE_FB)
	if (NOT HAVE_FB)
		if (NOT CMAKE_CROSSCOMPILING)
			message( WARNING "<Linux/fb.h> not found. Disabling framebuffer grabber.")
			SET ( DEFAULT_FRAMEBUFFER                     OFF )
		else()
			message( WARNING "<Linux/fb.h> not found. You may need to disable ENABLE_FRAMEBUFFER")
		endif()
	endif()
endif()

# D-bus support
set ( HYPERHDR_HAVE_DBUS                  OFF )
if (UNIX AND NOT APPLE AND (DEFAULT_POWER_MANAGEMENT OR DEFAULT_PIPEWIRE))
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(Systemd IMPORTED_TARGET GLOBAL libsystemd>=238)
    if(NOT TARGET PkgConfig::Systemd)
		message( WARNING "libsystemd>=238 development library is required for the D-Bus support. It is also required for the Power Management and the Pipewire grabber support." )
		SET ( DEFAULT_POWER_MANAGEMENT            OFF )
		SET ( DEFAULT_PIPEWIRE                    OFF )
	else()
		set ( HYPERHDR_HAVE_DBUS                  ON )
	endif()
endif()

# Find OpenSSL
find_package(OpenSSL)
if (NOT OPENSSL_FOUND)
	message( WARNING "OpenSSL could not be found. HyperHDR HTTPS instance and Philips Hue support will be disabled." )
endif()

# Pipewire support
if (DEFAULT_PIPEWIRE)
	FIND_PACKAGE(PkgConfig REQUIRED)
	pkg_check_modules(PIPEWIRE libpipewire-0.3)
	if(NOT PIPEWIRE_FOUND OR NOT PIPEWIRE_INCLUDE_DIRS OR NOT PIPEWIRE_LIBRARIES)
		message( WARNING "Pipewire >= 3.0 not found (did you install libpipewire-0.3-dev?). Disabling support for the PipeWire software grabber.")
		SET ( DEFAULT_PIPEWIRE                    OFF )
	else()
		if(POLICY CMP0072)
			cmake_policy(SET CMP0072 NEW)
		endif()
		find_package(OpenGL)
		if (NOT OpenGL_OpenGL_FOUND OR NOT OpenGL_EGL_FOUND)
			message( WARNING "OpenGL/EGL not found. Disabling DMA buffers for the Pipewire software grabber.")
		else()
			SET ( DEFAULT_PIPEWIRE_EGL                ON )
		endif()
	endif()
endif()

# CEC library
find_library (CEC_SUPPORT p8-platform NAMES libp8-platform.so.2 libp8-platform p8-platform )
if(NOT CEC_SUPPORT AND DEFAULT_CEC)
	message( WARNING "libp8-platform library not found. Install libp8-platform-dev. CEC support is disabled." )
	SET ( DEFAULT_CEC OFF )
elseif(DEFAULT_CEC)
	message( STATUS "libCEC support library found: " ${CEC_SUPPORT} )
endif()

if(DEFAULT_CEC)
	FIND_PACKAGE(PkgConfig REQUIRED)
	pkg_check_modules (CEC libcec>=6.0.0)
	if(NOT CEC_FOUND)
		message( WARNING "Could not find: libcec>=6. Disabling CEC support." )
		SET ( DEFAULT_CEC OFF )
	endif()
endif()

if (CMAKE_CROSSCOMPILING)
	set ( DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS OFF )
	SET ( DEFAULT_SHARED_LIBS                 OFF )
	set ( DEFAULT_EMBEDDED_WEB_RESOURCES      ON  )
endif()

if ( APPLE )
	set ( DEFAULT_EMBEDDED_WEB_RESOURCES      ON  )
	set ( USE_EMBEDDED_WEB_RESOURCES          ON  )
endif()

# set the build options
 if(NOT WIN32)
	string(ASCII 27 EscChar)
	set(ColorReset  "${EscChar}[m")
	set(RedColor    "${EscChar}[31m")
	set(GreenColor  "${EscChar}[32m")
	set(YellowColor "${EscChar}[33m")
	set(BlueColor   "${EscChar}[34m")
	set(CyanColor   "${EscChar}[36m")
endif()

function(colorMe value1 value2)
	if (value2)
		message( STATUS  "${value1}${GreenColor}${value2}${ColorReset}")
	else()
		message( STATUS  "${value1}${RedColor}${value2}${ColorReset}")
	endif()
endfunction()

message( STATUS "\n${CyanColor}LED DEVICES${ColorReset}")

option(ENABLE_SPIDEV   "Enable the SPIDEV device" ${DEFAULT_SPIDEV} )
colorMe("ENABLE_SPIDEV = " ${ENABLE_SPIDEV})

option(ENABLE_WS281XPWM "Enable the WS281x-PWM device" ${DEFAULT_WS281XPWM} )
colorMe("ENABLE_WS281XPWM = " ${ENABLE_WS281XPWM})

message( STATUS "\n${CyanColor}SOFTWARE GRABBERS${ColorReset}")

option(ENABLE_DX "Enable Windows DirectX 11 system grabber" ${DEFAULT_DX})
colorMe("ENABLE_DX = " ${ENABLE_DX})

option(ENABLE_FRAMEBUFFER "Enable the framebuffer Linux system grabber" ${DEFAULT_FRAMEBUFFER})
colorMe("ENABLE_FRAMEBUFFER = " ${ENABLE_FRAMEBUFFER})

option(ENABLE_MAC_SYSTEM "Enable macOS system grabber" ${DEFAULT_MAC_SYSTEM})
colorMe("ENABLE_MAC_SYSTEM = " ${ENABLE_MAC_SYSTEM})

option(ENABLE_PIPEWIRE "Enable the pipewire/portal Linux system grabber" ${DEFAULT_PIPEWIRE})
colorMe("ENABLE_PIPEWIRE = " ${ENABLE_PIPEWIRE})

option(ENABLE_PIPEWIRE_EGL "Enable the pipewire EGL extension" ${DEFAULT_PIPEWIRE_EGL})
if (DEFAULT_PIPEWIRE)
	colorMe("ENABLE_PIPEWIRE_EGL = " ${ENABLE_PIPEWIRE_EGL})
endif()

option(ENABLE_X11 "Enable the X11 Linux system grabber" ${DEFAULT_X11})
colorMe("ENABLE_X11 = " ${ENABLE_X11})

message( STATUS "\n${CyanColor}HARDWARE GRABBERS${ColorReset}")

option(ENABLE_AVF "Enable AV Foundation macOS grabber" ${DEFAULT_AVF})
colorMe("ENABLE_AVF = " ${ENABLE_AVF})

option(ENABLE_MF "Enable Windows Media Foundation grabber" ${DEFAULT_MF})
colorMe("ENABLE_MF = " ${ENABLE_MF})

option(ENABLE_V4L2 "Enable the V4L2 grabber" ${DEFAULT_V4L2})
colorMe("ENABLE_V4L2 = " ${ENABLE_V4L2})

message( STATUS "\n${CyanColor}SOUND CAPTURING${ColorReset}")

option(ENABLE_SOUNDCAPLINUX "Enable Linux Sound Capture" ${DEFAULT_SOUNDCAPLINUX})
colorMe("ENABLE_SOUNDCAPLINUX = " ${ENABLE_SOUNDCAPLINUX})

option(ENABLE_SOUNDCAPMACOS "Enable macOS Sound Capture" ${DEFAULT_SOUNDCAPMACOS})
colorMe("ENABLE_SOUNDCAPMACOS = " ${ENABLE_SOUNDCAPMACOS})

option(ENABLE_SOUNDCAPWINDOWS "Enable Windows Sound Capture" ${DEFAULT_SOUNDCAPWINDOWS})
colorMe("ENABLE_SOUNDCAPWINDOWS = " ${ENABLE_SOUNDCAPWINDOWS})

message( STATUS "\n${CyanColor}SERVICE SUPPORT${ColorReset}")

option(ENABLE_BONJOUR "Enable the network discovery service" ${DEFAULT_BONJOUR} )
colorMe("ENABLE_BONJOUR = " ${ENABLE_BONJOUR})

option(ENABLE_CEC "Enable CEC" ${DEFAULT_CEC})
colorMe("ENABLE_CEC = " ${ENABLE_CEC})

option(ENABLE_MQTT "Enable MQTT" ${DEFAULT_MQTT})
colorMe("ENABLE_MQTT = " ${ENABLE_MQTT})

option(ENABLE_POWER_MANAGEMENT "Enable Power Management support" ${DEFAULT_POWER_MANAGEMENT})
colorMe("ENABLE_POWER_MANAGEMENT = " ${ENABLE_POWER_MANAGEMENT})

option(ENABLE_PROTOBUF "Enable PROTOBUF" ${DEFAULT_PROTOBUF})
colorMe("ENABLE_PROTOBUF = " ${ENABLE_PROTOBUF})

option(ENABLE_SYSTRAY "Enable SYSTRAY" ${DEFAULT_SYSTRAY})
colorMe("ENABLE_SYSTRAY = " ${ENABLE_SYSTRAY})

option(ENABLE_ZSTD "Enable ZSTD support" ${DEFAULT_ZSTD})
colorMe("ENABLE_ZSTD = " ${ENABLE_ZSTD})

message( STATUS "\n${CyanColor}BUILD FEATURES${ColorReset}")

option(USE_SHARED_LIBS "Prefer shared libraries over static ones" ${DEFAULT_SHARED_LIBS})
colorMe("USE_SHARED_LIBS = " ${USE_SHARED_LIBS})

option(USE_EMBEDDED_WEB_RESOURCES "Use embedded web-server resources" ${DEFAULT_EMBEDDED_WEB_RESOURCES})
colorMe("USE_EMBEDDED_WEB_RESOURCES = " ${USE_EMBEDDED_WEB_RESOURCES})

option(USE_PRECOMPILED_HEADERS "Enable precompiled headers (PCH)" ${DEFAULT_PRECOMPILED_HEADERS})
colorMe("USE_PRECOMPILED_HEADERS = " ${USE_PRECOMPILED_HEADERS})

colorMe("USE_CCACHE_CACHING = " ${USE_CCACHE_CACHING})

option(USE_SYSTEM_MQTT_LIBS "Use system qmqtt libs" ${DEFAULT_USE_SYSTEM_MQTT_LIBS})
colorMe("USE_SYSTEM_MQTT_LIBS = " ${USE_SYSTEM_MQTT_LIBS})

option(USE_SYSTEM_FLATBUFFERS_LIBS "Use system flatbuffers libs" ${DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS})
colorMe("USE_SYSTEM_FLATBUFFERS_LIBS = " ${USE_SYSTEM_FLATBUFFERS_LIBS})

option(USE_STATIC_QT_PLUGINS "Enable static QT plugins" ${DEFAULT_STATIC_QT_PLUGINS})
colorMe("USE_STATIC_QT_PLUGINS = " ${USE_STATIC_QT_PLUGINS})

if(UNIX AND NOT APPLE)
	option(USE_STANDARD_INSTALLER_NAME "Use the standardized Linux installer name" OFF)
	colorMe("USE_STANDARD_INSTALLER_NAME = " ${USE_STANDARD_INSTALLER_NAME})

	colorMe("HYPERHDR_HAVE_DBUS = " ${HYPERHDR_HAVE_DBUS})
else()
	SET ( USE_STANDARD_INSTALLER_NAME      OFF )
endif()

message( STATUS "\n")

# next part

SET ( FLATBUFFERS_INSTALL_BIN_DIR ${CMAKE_BINARY_DIR}/flatbuf )
SET ( FLATBUFFERS_INSTALL_LIB_DIR ${CMAKE_BINARY_DIR}/flatbuf )

find_package(GitVersion)

if(ENABLE_ZSTD)
	find_package(zstd)
endif()

# configure a header file to pass some of the CMake settings
# to the source code
configure_file("${PROJECT_SOURCE_DIR}/HyperhdrConfig.h.in" "${PROJECT_BINARY_DIR}/HyperhdrConfig.h")
include_directories("${PROJECT_BINARY_DIR}")

# Define the global output path of binaries
SET(LIBRARY_OUTPUT_PATH    ${PROJECT_BINARY_DIR}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_PATH})
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})

# Add the project include directory as additional include path
include_directories(${CMAKE_SOURCE_DIR}/include)

# enable C++11; MSVC doesn't have c++11 feature switch
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	include(CheckCXXCompilerFlag)
	CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
	CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
	CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
	if (CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-psabi")
	endif()
	if(COMPILER_SUPPORTS_CXX17)
			message(STATUS "Enabling support for C++17")
			set(CMAKE_CXX_STANDARD 17)
	else()
			message(STATUS "No support for C++17 detected. Compilation will most likely fail on your compiler")
	endif()
else()
	include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("/std:c++20" COMPILER_SUPPORTS_CXX20)
    if (COMPILER_SUPPORTS_CXX20)
		set(CMAKE_CXX_STANDARD 20)
		message(STATUS "Enabling MSVC support for c++20")
    endif()
endif()

# Use GNU gold linker if available
if (NOT WIN32)
	include (LDGold)
endif()

# Don't create new dynamic tags (RUNPATH)
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND NOT APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
endif()

# setup -rpath to search for shared libs in BINARY/../lib folder
if (UNIX AND NOT APPLE)
	SET(CMAKE_SKIP_BUILD_RPATH FALSE)
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
	list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../lib/external")
	SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif ()

# find Threads libs
find_package(Threads REQUIRED)

# add QT5 dependency
IF ( CMAKE_CROSSCOMPILING AND NOT TARGET Qt${Qt_VERSION}::moc AND NOT TARGET Qt${Qt_VERSION}::rcc)

	file(GLOB QT_BIN ${QT_BIN_PATH})

	set(QT_MOC_EXECUTABLE ${QT_BIN}/moc)
	add_executable(Qt${Qt_VERSION}::moc IMPORTED)
	set_property(TARGET Qt${Qt_VERSION}::moc PROPERTY IMPORTED_LOCATION ${QT_MOC_EXECUTABLE})

	set(QT_RCC_EXECUTABLE ${QT_BIN}/rcc)
	add_executable(Qt${Qt_VERSION}::rcc IMPORTED)
	set_property(TARGET Qt${Qt_VERSION}::rcc PROPERTY IMPORTED_LOCATION ${QT_RCC_EXECUTABLE})

	message(STATUS "QT_BIN_PATH = ${QT_BIN}")
	message(STATUS "QT_MOC_EXECUTABLE = ${QT_MOC_EXECUTABLE}")
	message(STATUS "QT_RCC_EXECUTABLE = ${QT_RCC_EXECUTABLE}")

ENDIF()

# MSVC options
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	# Search for Windows SDK
	find_package(WindowsSDK REQUIRED)
	message(STATUS "WINDOWS SDK: ${WINDOWSSDK_LATEST_DIR} ${WINDOWSSDK_LATEST_NAME}")
	message(STATUS "MSVC VERSION: ${MSVC_VERSION}")
	
	FIRSTSUBDIR(SUBDIRQT "C:/Qt")
	SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${SUBDIRQT}/msvc2019_64")
	if ((Qt5Core_FOUND AND NOT (DEFINED ENV{Qt5_DIR})) OR (Qt6Core_FOUND AND NOT (DEFINED ENV{Qt6_DIR})))
		message(STATUS "Set Qt${Qt_VERSION} module path: ${SUBDIRQT}")
		SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SUBDIRQT}/msvc2019_64/lib/cmake/Qt${Qt_VERSION}")
	endif()

	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT hyperhdr)
	add_definitions(/MP)
endif()

add_definitions(${QT_DEFINITIONS})

# libjpegturbo
find_package(TurboJPEG REQUIRED)

message( STATUS "TURBOJPEG_LIBRARY_DIRS=${TurboJPEG_LIBRARY}")
message( STATUS "TURBOJPEG_INCLUDE_DIRS=${TurboJPEG_INCLUDE_DIRS}")

# Embedded QT plugins
if (USE_STATIC_QT_PLUGINS AND TARGET Qt6::qmake)
	get_target_property(QT6_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
	execute_process(
		COMMAND ${QT6_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
		OUTPUT_VARIABLE QT6_PLUGINS_DIR
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	file(GLOB resultPlugins "${QT6_PLUGINS_DIR}/tls/*openssl*")
	foreach(file ${resultPlugins})
		if (NOT IS_SYMLINK ${file})
			message("Using static QT plugin: " ${file})
			set(STATIC_QT_PLUGINS_LIBS ${STATIC_QT_PLUGINS_LIBS} ${file})
		endif()
	endforeach()	
endif()

if (WIN32)
	if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
			${${PROJECT_NAME}_BINARY_DIR}/Bin
			CACHE PATH
			"Single Directory for all Libraries")
	endif()

	if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
			${${PROJECT_NAME}_BINARY_DIR}/Bin
			CACHE PATH
			"Single Directory for all Executables")
	endif()
endif()

# remove generated files on make cleaan too
LIST( APPEND GENERATED_QRC
	${CMAKE_BINARY_DIR}/WebConfig.qrc
	${CMAKE_BINARY_DIR}/HyperhdrConfig.h
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${GENERATED_QRC}" )

# uninstall target
configure_file( "${OUR_CMAKE_MODULES}/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# Add the source/lib directories
add_subdirectory(external)
add_subdirectory(sources)

# Add resources directory
add_subdirectory(resources)

# enable make package - no code after this line !
include (${CMAKE_CURRENT_SOURCE_DIR}/cmake/packages.cmake)

# external targets
if (WIN32 AND TARGET stb AND TARGET flatbuffers AND TARGET protobuf-nanopb AND TARGET lunasvg AND TARGET flatc AND TARGET qmqtt AND TARGET libzstd_static AND TARGET libzstd_shared AND TARGET zstd AND TARGET clean-all AND TARGET sqlite3 AND TARGET precompiled_hyperhdr_headers)
	set_target_properties(stb qmqtt flatbuffers protobuf-nanopb lunasvg flatc resources uninstall libzstd_static libzstd_shared zstd clean-all sqlite3 precompiled_hyperhdr_headers PROPERTIES FOLDER ExternalLibsTargets)	
else()
	set_target_properties(resources uninstall PROPERTIES FOLDER ExternalLibsTargets)
endif()

if (NOT ENABLE_BONJOUR)	
	message( FATAL_ERROR "The network discovery service (ENABLE_BONJOUR) is an important feature of HyperHDR. If you know what you are doing, remove this safe-break.")
endif()

