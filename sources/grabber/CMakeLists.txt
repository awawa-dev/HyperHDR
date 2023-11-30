if (ENABLE_PIPEWIRE)
	add_subdirectory(pipewire)
endif (ENABLE_PIPEWIRE)

if (ENABLE_X11)
	add_subdirectory(X11)
endif (ENABLE_X11)

if (ENABLE_FRAMEBUFFER)
	add_subdirectory(framebuffer)
endif(ENABLE_FRAMEBUFFER)

if (ENABLE_MAC_SYSTEM)
	add_subdirectory(macOS)
endif (ENABLE_MAC_SYSTEM)

if (ENABLE_DX)
	add_subdirectory(DX)
endif (ENABLE_DX)

if (ENABLE_AVF)
	add_subdirectory(AVF)
endif (ENABLE_AVF)

if (ENABLE_V4L2)
	add_subdirectory(v4l2)
endif (ENABLE_V4L2)

if (ENABLE_MF)
	add_subdirectory(MF)
endif (ENABLE_MF)
