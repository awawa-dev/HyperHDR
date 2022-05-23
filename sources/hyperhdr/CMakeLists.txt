# generate windows .rc file for this binary
if (WIN32)
	include(${CMAKE_SOURCE_DIR}/cmake/win/win_rc.cmake)
	generate_win_rc_file(hyperhdr)
endif()

if (WIN32)
	add_executable(hyperhdr
		hyperhdr.h
		systray.h
		hyperhdr.cpp
		systray.cpp
		main.cpp
		${hyperhdr_WIN_RC_PATH}
		WinSuspend.cpp
		WinSuspend.h)
else()
	add_executable(hyperhdr
		hyperhdr.h
		systray.h
		hyperhdr.cpp
		systray.cpp
		main.cpp
		${hyperhdr_WIN_RC_PATH}
		)
endif()


# promote hyperhdr as GUI app
if (WIN32)
	target_link_options(hyperhdr PUBLIC /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

target_link_libraries(hyperhdr
	commandline
	hyperhdr-base
	effectengine
	jsonserver
	flatbufserver
	webserver
	ssdp
	database
	resources
	Qt${Qt_VERSION}::Widgets
)

if (USE_STATIC_QT_PLUGINS)
	target_link_libraries(hyperhdr ${STATIC_QT_PLUGINS_LIBS})
endif()


if (ENABLE_PROTOBUF)
	target_link_libraries(hyperhdr protoserver)
endif ()

if (ENABLE_AVAHI)
	target_link_libraries(hyperhdr bonjour)
endif ()

if (ENABLE_CEC)
	target_link_libraries(hyperhdr cechandler)
endif ()
		
if (ENABLE_V4L2)
	target_link_libraries(hyperhdr v4l2-grabber)
	target_link_libraries(hyperhdr ${TURBOJPEG_LINK_LIBRARIES})
endif ()

if (ENABLE_MF)
	target_link_libraries(hyperhdr MF-grabber)
	target_link_libraries(hyperhdr ${TURBOJPEG_LINK_LIBRARIES})
endif ()

if (ENABLE_DX)
	target_link_libraries(hyperhdr DX-grabber)
endif ()

if (ENABLE_X11)
	target_link_libraries(hyperhdr X11-grabber "${CMAKE_DL_LIBS}")
endif ()

if (ENABLE_FRAMEBUFFER)
	target_link_libraries(hyperhdr framebuffer-grabber "${CMAKE_DL_LIBS}")
endif ()

if (ENABLE_PIPEWIRE)
	target_link_libraries(hyperhdr Pipewire-grabber "${CMAKE_DL_LIBS}")
endif ()

if (ENABLE_MAC_SYSTEM)
	target_link_libraries(hyperhdr
		MACOS-grabber
		"-framework CoreGraphics"
		"-framework Foundation")
endif()

if (ENABLE_AVF)
	target_link_libraries(hyperhdr 
		AVF-grabber
		"-framework CoreGraphics"
		"-framework AVFoundation"
		"-framework Foundation"
		"-framework CoreMedia"
		"-framework CoreVideo"
		"-framework Accelerate"
		"-framework IOKit")
endif ()

if (ENABLE_SOUNDCAPWINDOWS)
	target_link_libraries(hyperhdr SoundCapWindows)
endif ()

if (ENABLE_SOUNDCAPLINUX)
	target_link_libraries(hyperhdr SoundCapLinux)
endif ()

if (ENABLE_SOUNDCAPMACOS)
	target_link_libraries(hyperhdr SoundCapMacOS)
endif ()


if(APPLE)
	install ( TARGETS hyperhdr DESTINATION "share/.." COMPONENT "HyperHDR" )	
elseif(NOT WIN32)
	install ( TARGETS hyperhdr DESTINATION "share/hyperhdr/bin" COMPONENT "HyperHDR" )
	install ( DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/debian/service DESTINATION "share/hyperhdr/" COMPONENT "HyperHDR" )
	install ( FILES ${CMAKE_SOURCE_DIR}/resources/icons/hyperhdr-icon-32px.png DESTINATION "share/hyperhdr/icons" COMPONENT "HyperHDR" )

	# Desktop file for hyperhdr
	install ( FILES ${CMAKE_SOURCE_DIR}/cmake/desktop/hyperhdr_128.png DESTINATION "share/hyperhdr/desktop" COMPONENT "HyperHDR" )
	install ( FILES ${CMAKE_SOURCE_DIR}/cmake/desktop/hyperhdr.desktop DESTINATION "share/hyperhdr/desktop" COMPONENT "HyperHDR" )
else()
	install ( TARGETS hyperhdr DESTINATION "bin" COMPONENT "HyperHDR" )

	#set( CMAKE_INSTALL_UCRT_LIBRARIES TRUE )
	#set( CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE )
	include( InstallRequiredSystemLibraries )
endif()

if(CMAKE_HOST_UNIX AND NOT APPLE)
	install( CODE "EXECUTE_PROCESS(COMMAND ln -sf \"../share/hyperhdr/bin/hyperhdr\" \"${CMAKE_BINARY_DIR}/symlink_hyperhdr\" )" COMPONENT "HyperHDR" )
	install( FILES ${CMAKE_BINARY_DIR}/symlink_hyperhdr DESTINATION "bin" RENAME hyperhdr COMPONENT "HyperHDR" )
	install( CODE "FILE (REMOVE ${CMAKE_BINARY_DIR}/symlink_hyperhdr )" COMPONENT "HyperHDR" )
endif()

# Copy dependencies (not for OSX)
include(${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake)

if(WIN32)
		if (Qt_VERSION EQUAL 5)
			get_target_property(QT_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE} IMPORTED_LOCATION)
			get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
			find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
		else()
			get_filename_component(My_Qt6Core_EXECUTABLE_DIR ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS} ABSOLUTE)
			find_program(WINDEPLOYQT_EXECUTABLE windeployqt PATHS "${My_Qt6Core_EXECUTABLE_DIR}" NO_DEFAULT_PATH)
			if (NOT WINDEPLOYQT_EXECUTABLE)
				find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
			endif()
		endif()
		
		if (WINDEPLOYQT_EXECUTABLE AND NOT CMAKE_GITHUB_ACTION)
			set(WINDEPLOYQT_PARAMS_RUNTIME --verbose 0 --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler)
			message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE} PATH_HINT:${My_Qt6Core_EXECUTABLE_DIR}${QT_BIN_DIR}")
			add_custom_command(TARGET hyperhdr POST_BUILD COMMAND ${WINDEPLOYQT_EXECUTABLE} ${WINDEPLOYQT_PARAMS_RUNTIME} "$<TARGET_FILE:hyperhdr>")			
		endif()
endif()		

if (APPLE)
	DeployApple("hyperhdr")
elseif (NOT WIN32) # Unix
	DeployUnix("hyperhdr")
elseif(WIN32) # Windows
	DeployWindows("hyperhdr")
endif ()
