if (ENABLE_SOUNDCAPLINUX)
	add_subdirectory(linux)
endif (ENABLE_SOUNDCAPLINUX)

if (ENABLE_SOUNDCAPWINDOWS)
	add_subdirectory(windows)
endif (ENABLE_SOUNDCAPWINDOWS)

if (ENABLE_SOUNDCAPMACOS)
	add_subdirectory(macos)
endif (ENABLE_SOUNDCAPMACOS)
