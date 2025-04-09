# - Try to find PipeWire

find_package(PkgConfig REQUIRED)

if(PKG_CONFIG_FOUND)
	pkg_check_modules(PCM_X11 x11)
	
	find_path(SCREEN_X11_INCLUDE_DIR NAMES X11/Xlib.h PATHS ${PCM_X11_INCLUDEDIR} PATH_SUFFIXES X11)	
	find_library(SCREEN_X11_LIBRARY NAMES X11 PATHS ${PCM_X11_LIBDIR})
	
else()
	message( STATUS "PKG config not found. Fallback to default searching." )
	find_path(SCREEN_X11_INCLUDE_DIR X11/Xlib.h)	
	find_library(SCREEN_X11_LIBRARY X11)
endif()



include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(XLibs REQUIRED_VARS SCREEN_X11_INCLUDE_DIR SCREEN_X11_LIBRARY )

if(XLibs_FOUND)
	set(XLibs_INCLUDE_DIRS ${SCREEN_X11_INCLUDE_DIR} )
	set(XLibs_LIBRARIES ${SCREEN_X11_LIBRARY} )
	set(XLibs_DEFINITIONS -DHAS_XLIBS=1)
else()
	message( FATAL_ERROR "Could not found libx11-dev ( ${SCREEN_X11_INCLUDE_DIR}, ${SCREEN_X11_LIBRARY} )" )
endif()

mark_as_advanced(Xlibs_INCLUDE_DIRS Xlibs_LIBRARIES)