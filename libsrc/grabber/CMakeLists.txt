if (ENABLE_AVF)
	add_subdirectory(AVF)
endif (ENABLE_AVF)

if (ENABLE_V4L2)
	add_subdirectory(v4l2)
endif (ENABLE_V4L2)

if (ENABLE_WMF)
	add_subdirectory(MF)
endif (ENABLE_WMF)

if (ENABLE_SOUNDCAPLINUX)
	add_subdirectory(SoundCapLinux)
endif (ENABLE_SOUNDCAPLINUX)

if (ENABLE_SOUNDCAPWINDOWS)
	add_subdirectory(SoundCapWindows)
endif (ENABLE_SOUNDCAPWINDOWS)

if (ENABLE_SOUNDCAPMACOS)
	add_subdirectory(SoundCapMacOS)
endif (ENABLE_SOUNDCAPMACOS)
