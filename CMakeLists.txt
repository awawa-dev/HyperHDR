cmake_minimum_required(VERSION 3.0.0)

message( STATUS "CMake Version: ${CMAKE_VERSION}" )

PROJECT(HyperHdr)

# Parse semantic version of version file
include (${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake)
file (STRINGS "version" HYPERHDR_VERSION)
SetVersionNumber(HYPERHDR ${HYPERHDR_VERSION})


# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# auto prepare .qrc files
set(CMAKE_AUTORCC ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Configure CCache if available
find_program(CCACHE_FOUND ccache)
if ( CCACHE_FOUND )
	if (DEFINED ENV{CCACHE_DIR})
		message ( STATUS "Found CCache and its env settings: $ENV{CCACHE_DIR}. Will try to use it." )
		set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
		set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
	else()
		message ( WARNING "Found CCache but env settings: CCACHE_DIR is not set. Skipping." )
	endif()
else()
	message ( STATUS "CCache is not found" )
endif(CCACHE_FOUND)

# find QT libs
find_package(Qt6 COMPONENTS Core Gui Network Sql Widgets SerialPort QUIET)

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
	find_package(Qt5 COMPONENTS Core Gui Network SerialPort Sql Widgets REQUIRED)
	message( STATUS "Found Qt Version: ${Qt5Core_VERSION}" )
	IF ( "${Qt5Core_VERSION}" VERSION_LESS "${QT_MIN_VERSION}" )
		message( FATAL_ERROR "Your Qt version is to old! Minimum required ${QT_MIN_VERSION}" )
	ENDIF()
	SET( Qt_VERSION 5 )	
ENDIF()


# Set build variables
SET ( DO_NOT_USE_QT_VERSION_6_LIBS        OFF )
SET ( DEFAULT_PROTOBUF                    ON  )
SET ( DEFAULT_BOBLIGHT                    ON  )
SET ( DEFAULT_WS281XPWM                   OFF )
SET ( DEFAULT_AVAHI                       ON  )
SET ( DEFAULT_USE_SHARED_AVAHI_LIBS       ON  )
SET ( DEFAULT_USE_SYSTEM_FLATBUFFERS_LIBS OFF )
SET ( DEFAULT_USE_SYSTEM_MBEDTLS_LIBS     OFF )
SET ( DEFAULT_MF                          OFF )
SET ( DEFAULT_DX                          OFF )
SET ( DEFAULT_AVF                         OFF )
SET ( DEFAULT_MAC_SYSTEM                  OFF )
SET ( DEFAULT_V4L2                        OFF )
SET ( DEFAULT_X11                         OFF )
SET ( DEFAULT_PIPEWIRE                    OFF )
SET ( DEFAULT_FRAMEBUFFER                 OFF )
SET ( DEFAULT_SOUNDCAPWINDOWS             OFF )
SET ( DEFAULT_SOUNDCAPMACOS               OFF )
SET ( DEFAULT_CEC                         OFF )

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
	
	SET ( DEFAULT_AVF        ON  )
	SET ( DEFAULT_MAC_SYSTEM ON  )
elseif ( "${PLATFORM}" MATCHES "rpi" )	
	SET ( DEFAULT_WS281XPWM  ON )
	SET ( DEFAULT_CEC        ON )
	
	# GL dispatcher
	find_library (GLD GLdispatch)
	if(NOT GLD)
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
		message(STATUS "Check if Raspberry Pi (0-yes): ${IS_RASPBERRY}")
	else()
		message(WARNING "Model info not exists. Assuming that it is Raspberry Pi")
		SET (IS_RASPBERRY  0)
	endif()

	if(${IS_RASPBERRY} EQUAL -1)
		message(STATUS "This is not Raspberry Pi. Default compiler settings.")
	elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "(armv7l|armv8)")
	    add_compile_options(-march=armv8-a+crc -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -funsafe-math-optimizations)
	    message(STATUS "RPI 3/4 (armv7l or armv8) detected for optimization")
	else()
		message(STATUS "Default compiler settings for Raspberry.")
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
	SET ( DEFAULT_AVAHI    OFF)
endif()

STRING( TOUPPER "-DPLATFORM_${PLATFORM}" PLATFORM_DEFINE)
STRING( REPLACE "-DEV" "" PLATFORM_DEFINE "${PLATFORM_DEFINE}" )
ADD_DEFINITIONS( ${PLATFORM_DEFINE} )

# Add project specific cmake modules (find, etc)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (DEFAULT_X11)
	if (NOT CMAKE_CROSSCOMPILING)
		find_package(XLibs QUIET )
	endif()	
	if (NOT XLibs_FOUND)
		message( WARNING "X11 libs not found. Disabling support for X11.")
		SET ( DEFAULT_X11                          OFF )
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

if (DEFAULT_PIPEWIRE)
	find_package(Qt${Qt_VERSION} COMPONENTS DBus QUIET )
	if (NOT Qt${Qt_VERSION}DBus_FOUND)
		message( WARNING "QT dbus library is required for PipeWire/Portal support" )
		SET ( DEFAULT_PIPEWIRE                    OFF )
	else()
	
		pkg_check_modules(PIPEWIRE libpipewire-0.3)
		if(NOT PIPEWIRE_FOUND OR NOT PIPEWIRE_INCLUDE_DIRS OR NOT PIPEWIRE_LIBRARIES)
			message( WARNING "Pipewire >= 3.0 not found (did you install libpipewire-0.3-dev?). Disabling support for PipeWire software grabber.")
			SET ( DEFAULT_PIPEWIRE                    OFF )
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
	IF ((NOT EXISTS "/opt/vc/lib" OR NOT EXISTS "/opt/vc/include") AND
	    (NOT EXISTS "/usr/lib/arm-linux-gnueabihf/libvchostif.a" OR NOT EXISTS "/usr/include/vcinclude/vcore.h") AND
		(NOT EXISTS "/usr/lib/aarch64-linux-gnu/libbcm_host.so.0" OR NOT EXISTS "/usr/include/interface/vchiq_arm/vchiq_if.h"))
		message( WARNING "CEC Rpi mode support could be disabled. Could not find Rpi developers libs: arm32 > /opt/vc/lib,/opt/vc/include,/usr/lib/arm-linux-gnueabihf,/usr/include/vcinclude, aarch64 > /usr/lib/aarch64-linux-gnu/libbcm_host.so.0,/usr/include/interface/vchiq_arm/vchiq_if.h" )
	endif()
endif()

# 7-zip as support
if (WIN32)
	# 7zip
	cmake_policy(SET CMP0053 NEW)
	set(MYENV "PROGRAMFILES(X86)")

	find_program(SEVENZIP_BIN
		NAMES 7z 7za
		HINTS "${MINGWDIR}" "${MINGWLIBS}/bin" "$MYENV/7-zip" "$ENV{ProgramFiles}/7-zip" "$ENV{ProgramW6432}/7-zip"
		PATH_SUFFIXES bin
		DOC "7zip executable"
	)

	if (SEVENZIP_BIN)
		message( STATUS "7-zip found: ${SEVENZIP_BIN}")
	else()
		message( FATAL_ERROR  "Please install 7-zip")
	endif (SEVENZIP_BIN)
endif()

# protobuf
if (DEFAULT_PROTOBUF)
	find_package(Protobuf QUIET)
	
	if (WIN32 AND NOT Protobuf_FOUND)
		if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/windows.zip OR
		   NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/bin)
			message( STATUS "Downloading protobuf library")
			file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows)
			file(DOWNLOAD https://github.com/awawa-dev/protobuf/releases/download/v3.20.0/windows.zip
						  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/windows.zip
						  STATUS PROTO_DOWNLOAD_STATUS
						  EXPECTED_HASH MD5=9fd170a6cfa29919e40f5642b2c52e92)
			list(GET PROTO_DOWNLOAD_STATUS 0 PROTO_DOWNLOAD_STATUS_CODE)
			if(PROTO_DOWNLOAD_STATUS_CODE AND NOT PROTO_DOWNLOAD_STATUS_CODE EQUAL 0)
				message( FATAL_ERROR "Could not download protobuf library")
			endif()
		endif()
		execute_process(
			COMMAND ${SEVENZIP_BIN} x ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/windows.zip -o${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/ -aoa -y
			RESULT_VARIABLE STATUS_PROTO_EXTRACT
			OUTPUT_VARIABLE OUTPUT_PROTO
		)
		if(STATUS_PROTO_EXTRACT AND NOT STATUS_PROTO_EXTRACT EQUAL 0)
			message( FATAL_ERROR "Could not extract protobuf library binaries: ${STATUS_PROTO_EXTRACT} ${OUTPUT_PROTO}")		
		endif()		
		
		set (Protobuf_FOUND 1)
		set (Protobuf_PROTOC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/bin/protoc.exe)
		set (PROTOBUF_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/include)
		set (PROTOBUF_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/protobuf-windows/lib/libprotobuf$<$<CONFIG:DEBUG>:d>.lib)
		add_executable(protobuf::protoc IMPORTED)
		set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}")        
	endif()
		
	if (NOT Protobuf_FOUND)
		message( STATUS "Warning: could not find protobuf libraries (macOS: protobuf, Linux: libprotobuf-dev protobuf-compiler, Windows: need to compile yourself)" )
		SET ( DEFAULT_PROTOBUF OFF )
	endif()
endif()

# set the build options
option(ENABLE_BOBLIGHT   "Enable BOBLIGHT component" ${DEFAULT_BOBLIGHT} )
message(STATUS "ENABLE_BOBLIGHT   = ${ENABLE_BOBLIGHT}")

option(ENABLE_SPIDEV   "Enable the SPIDEV device" ${DEFAULT_SPIDEV} )
message(STATUS "ENABLE_SPIDEV   = ${ENABLE_SPIDEV}")

option(ENABLE_V4L2 "Enable the V4L2 grabber" ${DEFAULT_V4L2})
message(STATUS "ENABLE_V4L2 = ${ENABLE_V4L2}")

option(ENABLE_X11 "Enable the X11 Linux system grabber" ${DEFAULT_X11})
message(STATUS "ENABLE_X11 = ${ENABLE_X11}")

option(ENABLE_FRAMEBUFFER "Enable the framebuffer Linux system grabber" ${DEFAULT_FRAMEBUFFER})
message(STATUS "ENABLE_FRAMEBUFFER = ${ENABLE_FRAMEBUFFER}")

option(ENABLE_PIPEWIRE "Enable the pipewire/portal Linux system grabber" ${DEFAULT_PIPEWIRE})
message(STATUS "ENABLE_PIPEWIRE = ${ENABLE_PIPEWIRE}")

option(ENABLE_WS281XPWM "Enable the WS281x-PWM device" ${DEFAULT_WS281XPWM} )
message(STATUS "ENABLE_WS281XPWM = ${ENABLE_WS281XPWM}")

option(ENABLE_AVAHI "Enable Zeroconf" ${DEFAULT_AVAHI})
message(STATUS "ENABLE_AVAHI = " ${ENABLE_AVAHI})

option(ENABLE_MF "Enable Windows Media Foundation grabber" ${DEFAULT_MF})
message(STATUS "ENABLE_MF = ${ENABLE_MF}")

option(ENABLE_DX "Enable Windows DirectX 11 system grabber" ${DEFAULT_DX})
message(STATUS "ENABLE_DX = ${ENABLE_DX}")

option(ENABLE_AVF "Enable AV Foundation macOS grabber" ${DEFAULT_AVF})
message(STATUS "ENABLE_AVF = ${ENABLE_AVF}")

option(ENABLE_MAC_SYSTEM "Enable macOS system grabber" ${DEFAULT_MAC_SYSTEM})
message(STATUS "ENABLE_MAC_SYSTEM = ${ENABLE_MAC_SYSTEM}")

option(ENABLE_SOUNDCAPWINDOWS "Enable Windows Sound Capture" ${DEFAULT_SOUNDCAPWINDOWS})
message(STATUS "ENABLE_SOUNDCAPWINDOWS = ${ENABLE_SOUNDCAPWINDOWS}")

option(ENABLE_SOUNDCAPMACOS "Enable macOS Sound Capture" ${DEFAULT_SOUNDCAPMACOS})
message(STATUS "ENABLE_SOUNDCAPMACOS = ${ENABLE_SOUNDCAPMACOS}")

option(ENABLE_SOUNDCAPLINUX "Enable Linux Sound Capture" ${DEFAULT_SOUNDCAPLINUX})
message(STATUS "ENABLE_SOUNDCAPLINUX = ${ENABLE_SOUNDCAPLINUX}")

option(ENABLE_CEC "Enable CEC" ${DEFAULT_CEC})
message(STATUS "ENABLE_CEC = ${ENABLE_CEC}")

option(ENABLE_PROTOBUF "Enable PROTOBUF" ${DEFAULT_PROTOBUF})
message(STATUS "ENABLE_PROTOBUF = ${ENABLE_PROTOBUF}")

message(STATUS "USE_STATIC_QT_PLUGINS = ${USE_STATIC_QT_PLUGINS}")

SET ( FLATBUFFERS_INSTALL_BIN_DIR ${CMAKE_BINARY_DIR}/flatbuf )
SET ( FLATBUFFERS_INSTALL_LIB_DIR ${CMAKE_BINARY_DIR}/flatbuf )

find_package(GitVersion)

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
include_directories(${CMAKE_SOURCE_DIR}/dependencies/include)
include_directories(${CMAKE_SOURCE_DIR}/include)

# enable C++11; MSVC doesn't have c++11 feature switch
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	include(CheckCXXCompilerFlag)
	CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
	CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
	if (CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-psabi")
	endif()
	if(COMPILER_SUPPORTS_CXX11)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	elseif(COMPILER_SUPPORTS_CXX0X)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
	else()
			message(STATUS "No support for C++11 detected. Compilation will most likely fail on your compiler")
	endif()
endif()

# Use GNU gold linker if available
if (NOT WIN32)
	include (${CMAKE_CURRENT_SOURCE_DIR}/cmake/LDGold.cmake)
endif()

# Don't create new dynamic tags (RUNPATH)
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND NOT APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")
endif()

# setup -rpath to search for shared libs in BINARY/../lib folder
if (UNIX AND NOT APPLE)
	SET(CMAKE_SKIP_BUILD_RPATH FALSE)
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}:$ORIGIN/../lib")
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
endif()

add_definitions(${QT_DEFINITIONS})

# libjpegturbo
if ( ENABLE_V4L2 OR ENABLE_MF )
	if(WIN32)
		if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows)
			file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows)
		endif()
		if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/windows.zip OR
			NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/lib)
			message( STATUS "Downloading libjpegturbo library")
			file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows)
			file(DOWNLOAD https://github.com/awawa-dev/libjpeg-turbo/releases/download/2.1.3/windows.zip
						  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/windows.zip
						  STATUS JPEG_DOWNLOAD_STATUS
						  EXPECTED_HASH MD5=2CF284387AC832270736893C4EE05852)
			list(GET JPEG_DOWNLOAD_STATUS 0 JPEG_DOWNLOAD_STATUS_CODE)
			if(JPEG_DOWNLOAD_STATUS_CODE AND NOT JPEG_DOWNLOAD_STATUS_CODE EQUAL 0)
				message( FATAL_ERROR "Could not download libjpegturbo library")
			endif()
		endif()
		execute_process(
			COMMAND ${SEVENZIP_BIN} x ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/windows.zip -o${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/ -aoa -y
			RESULT_VARIABLE STATUS_EXTRACT
			OUTPUT_VARIABLE OUTPUT1
		)
		if(STATUS_EXTRACT AND NOT STATUS_EXTRACT EQUAL 0)
		    message( FATAL_ERROR "Could not extract libjpegturbo library")
		else()
		    message( STATUS "libjpegturbo library extracted")			
		endif()
		set (TURBOJPEG_FOUND 1)
		
		set (TURBOJPEG_LIBRARY_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/bin")
		set (TURBOJPEG_LINK_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/lib/turbojpeg.lib")
		set (TURBOJPEG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/jpeg-windows/include")
	else()
		FIND_PACKAGE(PkgConfig REQUIRED)
		pkg_check_modules(TURBOJPEG REQUIRED libturbojpeg>=2.0)
	endif ()
	
	message( STATUS "TURBOJPEG_LIBRARY_DIRS=${TURBOJPEG_LIBRARY_DIRS}")
	message( STATUS "TURBOJPEG_LINK_LIBRARIES=${TURBOJPEG_LINK_LIBRARIES}")
	message( STATUS "TURBOJPEG_INCLUDE_DIRS=${TURBOJPEG_INCLUDE_DIRS}")
endif()

# Embedded QT plugins
if (USE_STATIC_QT_PLUGINS)
	foreach(guiPlugin ${Qt${Qt_VERSION}Gui_PLUGINS} Qt${Qt_VERSION}::Gui)		
		get_target_property(file ${guiPlugin} LOCATION)
		if (NOT IS_SYMLINK ${file})
			message("Using static QT plugin: " ${file})
			set(STATIC_QT_PLUGINS_LIBS ${STATIC_QT_PLUGINS_LIBS} ${file})
		endif()
	endforeach()
	foreach(sqlPlugin ${Qt${Qt_VERSION}Sql_PLUGINS} Qt${Qt_VERSION}::Sql)
		message( STATUS "TURBOJPEG_INCLUDE_DIRS=${sqlPlugin}")
		get_target_property(file ${sqlPlugin} LOCATION)
		if (NOT IS_SYMLINK ${file})
			message("Using static QT plugin: " ${file})
			set(STATIC_QT_PLUGINS_LIBS ${STATIC_QT_PLUGINS_LIBS} ${file})
		endif()
	endforeach()	
endif()

# Add the source/lib directories
add_subdirectory(dependencies)
add_subdirectory(sources)

# Add resources directory
add_subdirectory(resources)

# remove generated files on make cleaan too
LIST( APPEND GENERATED_QRC
	${CMAKE_BINARY_DIR}/WebConfig.qrc
	${CMAKE_BINARY_DIR}/HyperhdrConfig.h
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${GENERATED_QRC}" )

# uninstall target
configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# enable make package - no code after this line !
include (${CMAKE_CURRENT_SOURCE_DIR}/cmake/packages.cmake)

# external targets
if (WIN32 AND TARGET apidoc AND TARGET flatbuffers AND TARGET flatc AND TARGET mbedcrypto)
	set_target_properties(apidoc flatbuffers flatc flathash lib mbedcrypto mbedtls mbedx509 resources uninstall PROPERTIES FOLDER ExternalLibsTargets)	
else()
	set_target_properties(resources uninstall PROPERTIES FOLDER ExternalLibsTargets)
endif()

