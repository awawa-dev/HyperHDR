# - Try to find PipeWire

find_package(PkgConfig REQUIRED)

if(PKG_CONFIG_FOUND)
	pkg_check_modules(PCM_PIPEWIRE libpipewire-0.3)
	if (NOT PCM_PIPEWIRE_INCLUDEDIR)
		pkg_check_modules(PCM_PIPEWIRE libpipewire-0.2)
	endif()
	pkg_check_modules(PCM_SPA libspa-0.2)
	if (NOT PCM_SPA_INCLUDEDIR)
		pkg_check_modules(PCM_SPA libspa-0.1)
	endif()
	
	find_path(PIPEWIRE_INCLUDE_DIR NAMES pipewire/pipewire.h PATHS ${PCM_PIPEWIRE_INCLUDEDIR} PATH_SUFFIXES pipewire)

	find_path(SPA_INCLUDE_DIR NAMES spa/support/plugin.h PATHS ${PCM_SPA_INCLUDEDIR} PATH_SUFFIXES spa)

	find_library(PIPEWIRE_LIBRARY NAMES pipewire-0.3 PATHS ${PCM_PIPEWIRE_LIBDIR})

	if (NOT PIPEWIRE_LIBRARY)
		find_library(PIPEWIRE_LIBRARY NAMES pipewire-0.2 PATHS ${PCM_PIPEWIRE_LIBDIR})
	endif()
else()
	message( STATUS "PKG config not found. Fallback to default searching." )
	find_path(PIPEWIRE_INCLUDE_DIR pipewire/pipewire.h)
	find_path(SPA_INCLUDE_DIR NAMES spa/support/plugin.h)
	find_library(PIPEWIRE_LIBRARY pipewire-0.3)
	if (NOT PIPEWIRE_LIBRARY)
		find_library(PIPEWIRE_LIBRARY NAMES pipewire-0.2 PATHS ${PCM_PIPEWIRE_LIBDIR})
	endif()	
endif()



include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Pipewire REQUIRED_VARS PIPEWIRE_LIBRARY PIPEWIRE_INCLUDE_DIR SPA_INCLUDE_DIR )

if(PIPEWIRE_FOUND)
	set(PIPEWIRE_INCLUDE_DIRS ${PIPEWIRE_INCLUDE_DIR} ${SPA_INCLUDE_DIR})
	set(PIPEWIRE_LIBRARIES ${PIPEWIRE_LIBRARY})
	set(PIPEWIRE_DEFINITIONS -DHAS_PIPEWIRE=1)
else()
	message( FATAL_ERROR "Could not found libpipewire-dev >= 0.2 and libspa-dev >= 0.2 (${PCM_PIPEWIRE_INCLUDEDIR}, ${PCM_SPA_INCLUDEDIR}, ${PCM_PIPEWIRE_LIBDIR})" )
endif()

mark_as_advanced(PIPEWIRE_INCLUDE_DIR PIPEWIRE_LIBRARY)