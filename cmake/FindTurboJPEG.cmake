#  FindTurboJPEG.cmake
#  TURBOJPEG_FOUND
#  TurboJPEG_INCLUDE_DIRS
#  TurboJPEG_LIBRARY

if (ENABLE_WMF)
	find_path(TurboJPEG_INCLUDE_DIRS
		NAMES turbojpeg.h
		PATHS
		"${CMAKE_SOURCE_DIR}/resources/include/jpeg"
	)

	find_library(TurboJPEG_LIBRARY
		NAMES turbojpeg turbojpeg-static
		PATHS
		"${CMAKE_SOURCE_DIR}/resources/dll/jpeg"
	)
elseif (ENABLE_AVF)
	find_path(TurboJPEG_INCLUDE_DIRS
		NAMES turbojpeg.h
		PATHS
		"/usr/local/opt/jpeg-turbo/include"
	)

	find_library(TurboJPEG_LIBRARY
		NAMES turbojpeg turbojpeg-static
		PATHS
		"/usr/local/opt/jpeg-turbo/lib"
	)
else()
	find_path(TurboJPEG_INCLUDE_DIRS
		NAMES turbojpeg.h
		PATH_SUFFIXES include
	)

	find_library(TurboJPEG_LIBRARY
		NAMES turbojpeg turbojpeg-static
		PATH_SUFFIXES bin lib
	)
endif()

if(TurboJPEG_INCLUDE_DIRS AND TurboJPEG_LIBRARY)
	include(CheckCSourceCompiles)
	include(CMakePushCheckState)

	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${TurboJPEG_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${TurboJPEG_LIBRARY})

	check_c_source_compiles("#include <turbojpeg.h>\nint main(void) { tjhandle h=tjInitCompress(); return 0; }" TURBOJPEG_WORKS)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(TurboJPEG
	FOUND_VAR TURBOJPEG_FOUND
	REQUIRED_VARS TurboJPEG_LIBRARY TurboJPEG_INCLUDE_DIRS TURBOJPEG_WORKS
	TurboJPEG_INCLUDE_DIRS TurboJPEG_LIBRARY
)