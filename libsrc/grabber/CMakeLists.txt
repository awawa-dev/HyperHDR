if (ENABLE_V4L2)
	add_subdirectory(v4l2)
endif (ENABLE_V4L2)

if (ENABLE_QTC)
	add_subdirectory(MF)
endif (ENABLE_QTC)