if (ENABLE_PIPEWIRE)
	add_subdirectory(linux/pipewire)
endif (ENABLE_PIPEWIRE)

if (ENABLE_X11)
	add_subdirectory(linux/X11)
endif (ENABLE_X11)

if (ENABLE_FRAMEBUFFER)
	add_subdirectory(linux/framebuffer)
endif(ENABLE_FRAMEBUFFER)

if (ENABLE_V4L2)
	add_subdirectory(linux/v4l2)
endif (ENABLE_V4L2)

if (ENABLE_MAC_SYSTEM)
	add_subdirectory(osx/macOS)
endif (ENABLE_MAC_SYSTEM)

if (ENABLE_AVF)
	add_subdirectory(osx/AVF)
endif (ENABLE_AVF)

if (ENABLE_DX)
	add_subdirectory(windows/DX)
endif (ENABLE_DX)

if (ENABLE_MF)
	add_subdirectory(windows/MF)
endif (ENABLE_MF)
