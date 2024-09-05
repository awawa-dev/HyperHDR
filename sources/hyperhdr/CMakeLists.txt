# generate windows .rc file for this binary
if (WIN32)
	include(win_rc)
	generate_win_rc_file(hyperhdr)
endif()

add_executable(hyperhdr
	HyperHdrDaemon.h
	SystrayHandler.h
	HyperHdrDaemon.cpp
	SystrayHandler.cpp
	main.cpp
	${hyperhdr_WIN_RC_PATH}
)

# promote hyperhdr as GUI app
if (WIN32)
	target_link_options(hyperhdr PUBLIC /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

target_link_libraries(hyperhdr
	blackborder
	commandline
	database
	effects
	flatbuffers-client
	flatbuffers-parser
	flatbuffers-server
	hyperhdr-api
	hyperhdr-base
	hyperhdr-utils
	hyperimage
	image
	jsonserver
	json-utils
	led-drivers
	led-strip
	lut-calibrator
	performance-counters
	protobuf-nanopb
	resources
	ssdp
	suspend-handler
	utils-image
	webserver
	${APPKIT_FRAMEWORK}
)

if (WIN32 OR APPLE)
	target_link_libraries(hyperhdr systray-widget)
else()
	install(TARGETS systray-widget
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if(ENABLE_XZ)
	if (LIBLZMA_FOUND)
		target_link_libraries(hyperhdr LibLZMA::LibLZMA utils-xz)
	else()
		target_link_libraries(hyperhdr liblzma utils-xz)
	endif()
endif()

if (USE_STATIC_QT_PLUGINS)
	target_link_libraries(hyperhdr ${STATIC_QT_PLUGINS_LIBS})
endif()

if (ENABLE_PROTOBUF)
	target_link_libraries(hyperhdr proto-nano-server)
endif ()

if (ENABLE_MQTT)
	target_link_libraries(hyperhdr mqtt)
endif ()

if (ENABLE_BONJOUR)
	target_link_libraries(hyperhdr bonjour)
endif ()

if (ENABLE_CEC)
	target_link_libraries(hyperhdr cechandler)
endif ()
		
if (ENABLE_V4L2)
	target_link_libraries(hyperhdr v4l2-grabber)
endif ()

if (ENABLE_MF)
	target_link_libraries(hyperhdr grabber-MF)
endif ()

if (ENABLE_DX)
	target_link_libraries(hyperhdr grabber-DX)
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
	target_link_libraries(hyperhdr sound-capture-windows)
endif ()

if (ENABLE_SOUNDCAPLINUX)
	target_link_libraries(hyperhdr sound-capture-linux)
endif ()

if (ENABLE_SOUNDCAPMACOS)
	target_link_libraries(hyperhdr sound-capture-macos)
endif ()

# rules for installers
include(${OUR_CMAKE_MODULES}/../installers.cmake)

if (APPLE)
	DeployApple("hyperhdr")
elseif (NOT WIN32) # Unix
	DeployUnix("hyperhdr")
elseif(WIN32) # Windows
	DeployWindows("hyperhdr")
endif ()

if(USE_PRECOMPILED_HEADERS AND COMMAND target_precompile_headers)
    target_precompile_headers(hyperhdr REUSE_FROM precompiled_hyperhdr_headers)
endif()
