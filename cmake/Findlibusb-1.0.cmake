if (NOT WIN32)	
	if (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)
		# in cache already
		set(LIBUSB_FOUND TRUE)
	else (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)
		find_path(LIBUSB_1_INCLUDE_DIR
			NAMES
				libusb.h
			PATHS
				/usr/include
				/usr/local/include
				/opt/local/include
				/sw/include
			PATH_SUFFIXES
				libusb-1.0
		)

		find_library(LIBUSB_1_LIBRARY
		NAMES
			usb-1.0 usb
		PATHS
			/usr/lib
			/usr/local/lib
			/opt/local/lib
			/sw/lib
		)

		set(LIBUSB_1_INCLUDE_DIRS ${LIBUSB_1_INCLUDE_DIR}  )
		set(LIBUSB_1_LIBRARIES ${LIBUSB_1_LIBRARY} )

		if (LIBUSB_1_INCLUDE_DIRS AND LIBUSB_1_LIBRARIES)
			set(LIBUSB_1_FOUND TRUE)
		endif (LIBUSB_1_INCLUDE_DIRS AND LIBUSB_1_LIBRARIES)

		if (LIBUSB_1_FOUND)
			if (NOT libusb_1_FIND_QUIETLY)
				message(STATUS "Found libusb-1.0:")
				message(STATUS " - Includes: ${LIBUSB_1_INCLUDE_DIRS}")
				message(STATUS " - Libraries: ${LIBUSB_1_LIBRARIES}")
			endif (NOT libusb_1_FIND_QUIETLY)
		else (LIBUSB_1_FOUND)
			unset(LIBUSB_1_LIBRARY CACHE)
			if (libusb_1_FIND_REQUIRED)
				message(FATAL_ERROR "Could not find libusb")
			endif (libusb_1_FIND_REQUIRED)
		endif (LIBUSB_1_FOUND)

		# show the LIBUSB_1_INCLUDE_DIRS and LIBUSB_1_LIBRARIES variables only in the advanced view
		mark_as_advanced(LIBUSB_1_INCLUDE_DIRS LIBUSB_1_LIBRARIES)

	endif (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)
	
elseif(ENABLE_USB_HID)

	message(STATUS "Searching  libusb-1.0:...................................")
	if (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)
		# in cache already
		message(STATUS "Searching  libusb-1.0: found")
		set(LIBUSB_FOUND TRUE)
	else (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)
		message(STATUS "Searching  libusb-1.0: starts for Windows")
		message(STATUS "Searching  libusb-1.0: ${CMAKE_SOURCE_DIR}/resources/include/libusb")
		message(STATUS "Searching  libusb-1.0: ${CMAKE_SOURCE_DIR}/resources/dll/libusb")
		find_path(LIBUSB_1_INCLUDE_DIR
			NAMES
				libusb.h
			PATHS				
				"${CMAKE_SOURCE_DIR}/resources/include/libusb"
			REQUIRED
		)

		find_library(LIBUSB_1_LIBRARY
			NAMES
				usb-1.0 usb libusb-1.0 libusb
			PATHS
				"${CMAKE_SOURCE_DIR}/resources/dll/libusb"
			REQUIRED
		)
		
		find_library(LIBUSB_2_LIBRARY
			NAMES
				hidapi
			PATHS
				"${CMAKE_SOURCE_DIR}/resources/dll/libusb"
			REQUIRED
		)

		set(LIBUSB_1_INCLUDE_DIRS ${LIBUSB_1_INCLUDE_DIR}  )
		set(LIBUSB_1_LIBRARIES "${LIBUSB_1_LIBRARY}" )
		
		message(STATUS "Searching  libusb-1.0: include => ${LIBUSB_1_INCLUDE_DIR}")
		message(STATUS "Searching  libusb-1.0: lib => ${LIBUSB_1_LIBRARY}")		
		message(STATUS "Searching  libusb-hidapi: lib => ${LIBUSB_2_LIBRARY}")		

		if (LIBUSB_1_INCLUDE_DIRS AND LIBUSB_1_LIBRARIES)
			set(LIBUSB_1_FOUND TRUE)
		endif (LIBUSB_1_INCLUDE_DIRS AND LIBUSB_1_LIBRARIES)

		if (LIBUSB_1_FOUND)
			if (NOT libusb_1_FIND_QUIETLY)
				message(STATUS "Found libusb-1.0:")
				message(STATUS " - Includes: ${LIBUSB_1_INCLUDE_DIRS}")
				message(STATUS " - Libraries: ${LIBUSB_1_LIBRARIES}")
			endif (NOT libusb_1_FIND_QUIETLY)
		else (LIBUSB_1_FOUND)
			unset(LIBUSB_1_LIBRARY CACHE)
			if (libusb_1_FIND_REQUIRED)
				message(FATAL_ERROR "Could not find libusb")
			endif (libusb_1_FIND_REQUIRED)
		endif (LIBUSB_1_FOUND)

		# show the LIBUSB_1_INCLUDE_DIRS and LIBUSB_1_LIBRARIES variables only in the advanced view
		#mark_as_advanced(LIBUSB_1_INCLUDE_DIRS LIBUSB_1_LIBRARIES)
	endif (LIBUSB_1_LIBRARIES AND LIBUSB_1_INCLUDE_DIRS)

endif()
                                                              
