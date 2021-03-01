macro(DeployUnix TARGET)
	if(EXISTS ${TARGET_FILE})
		message(STATUS "Collecting Dependencies for target file: ${TARGET_FILE}")
		include(GetPrerequisites)
		#"libsystemd0"
		set(SYSTEM_LIBS_SKIP
			"libc"
			"libglib-2"
			"libsystemd0"
			"libdl"
			"libexpat"
			"libfontconfig"
			"libgcc_s"			
			"libgcrypt"
			"libgpg-error"
			"libm"
			"libpthread"
			"librt"
			"libstdc++"
			"libudev"
			"libutil"			
			"libz"
			"libxrender1"
			"libxi6"
			"libxext6"
			"libx11-xcb1"
			"libsm"
			"libqt5gui5"
			"libice6"
			"libdrm2"
			"libxkbcommon0"
			"libwacom2"
			"libmtdev1"
			"libinput10"
			"libgudev-1.0-0"
			"libffi6"
			"libevdev2"
			"libqt5dbus5"
			"libuuid1"
			"libselinux1"
			"libmount1"
			"libblkid1"
			"libxcb-icccm4"
			"libxcb-image0"
			"libxcb-keysyms1"
			"libxcb-randr0"
			"libxcb-render-util0"
			"libxcb-render0"
			"libxcb-shape0"
			"libxcb-shm0"
			"libxcb-sync1"
			"libxcb-util0"
			"libxcb-xfixes0"
			"libxcb-xinerama0"
			"libxcb-xkb1"
			"libxcb1"
			"libxkbcommon-x11-0"
			"libxcb-xinput0"
			"libssl1.1"
			"libsqlite3-0"
		)

		if (APPLE)
			set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
		endif(APPLE)

		# Extract dependencies ignoring the system ones
		get_prerequisites(${TARGET_FILE} DEPENDENCIES 0 1 "" "")

		# Append symlink and non-symlink dependencies to the list
		set(PREREQUISITE_LIBS "")
		foreach(DEPENDENCY ${DEPENDENCIES})
			get_filename_component(resolved ${DEPENDENCY} NAME_WE)
			
			foreach(myitem ${SYSTEM_LIBS_SKIP})
				string(FIND ${myitem} ${resolved} _index)
				if (${_index} GREATER -1)
					break()									
				endif()
			endforeach()
					
			if (${_index} GREATER -1)
				continue() # Skip system libraries
			else()
				gp_resolve_item("${TARGET_FILE}" "${DEPENDENCY}" "" "" resolved_file)
				get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
				get_filename_component(file_canonical ${resolved_file} REALPATH)
				gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
				#message(STATUS "Basic check added: ${resolved_file}")
			endif()
		endforeach()

		# Detect the Qt5 plugin directory, source: https://github.com/lxde/lxqt-qtplugin/blob/master/src/CMakeLists.txt
		get_target_property(QT_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE} IMPORTED_LOCATION)
		execute_process(
			COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
			OUTPUT_VARIABLE QT_PLUGINS_DIR
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		
		if(GLD)
			SET(resolved_file ${GLD})
			message(STATUS "Adding GLD: ${resolved_file}")
			get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
			gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
			message(STATUS "Added GLD: ${resolved_file}")
			set(resolved_file "${resolved_file}.0")
			if(EXISTS ${resolved_file})
				#message(STATUS "Adding GLD: ${resolved_file}")
				get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
				#message(STATUS "Added GLD: ${resolved_file}")
			endif()
			get_filename_component(file_canonical ${resolved_file} REALPATH)
			gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
			message(STATUS "Added: ${file_canonical}")
		endif()
		
		find_library(LIB_GLX
			NAMES libGLX libGLX.so		
		)
				
		if(LIB_GLX)
			message(STATUS "libGLX found ${LIB_GLX}")
			SET(resolved_file ${LIB_GLX})
			message(STATUS "Adding LIB_GLX: ${resolved_file}")
			get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
			gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
			message(STATUS "Added LIB_GLX: ${resolved_file}")
			set(resolved_file "${resolved_file}.0")
			if(EXISTS ${resolved_file})
				#message(STATUS "Adding GLX: ${resolved_file}")
				get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
				#message(STATUS "Added GLX: ${resolved_file}")
			endif()
			get_filename_component(file_canonical ${resolved_file} REALPATH)
			gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
			message(STATUS "Added: ${file_canonical}")
		else()
			message(STATUS "libGLX not found")
		endif()

		# Copy Qt plugins to 'share/hyperhdr/lib'
		if(QT_PLUGINS_DIR)
			foreach(PLUGIN "platforms" "sqldrivers" "imageformats")
				if(EXISTS ${QT_PLUGINS_DIR}/${PLUGIN})
					file(GLOB files "${QT_PLUGINS_DIR}/${PLUGIN}/*")
					foreach(file ${files})
						get_prerequisites(${file} PLUGINS 0 1 "" "")

						foreach(DEPENDENCY ${PLUGINS})
							get_filename_component(resolved ${DEPENDENCY} NAME_WE)
							
							foreach(myitem ${SYSTEM_LIBS_SKIP})
									#message(STATUS "Checking ${myitem}")
									string(FIND ${myitem} ${resolved} _index)
									if (${_index} GREATER -1)
										#message(STATUS "${myitem} = ${resolved}")									
										break()									
									endif()
							endforeach()
								
							if (${_index} GREATER -1)
								#message(STATUS "QT skipped: ${resolved}")
								continue() # Skip system libraries
							else()						
								#message(STATUS "QT included: ${resolved}")
								gp_resolve_item("${file}" "${DEPENDENCY}" "" "" resolved_file)
								get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
								gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
								get_filename_component(file_canonical ${resolved_file} REALPATH)
								gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
								#message(STATUS "QT added: ${resolved_file}")
							endif()
						endforeach()

						install(
							FILES ${file}
							DESTINATION "share/hyperhdr/lib/${PLUGIN}"
							COMPONENT "HyperHDR"
						)
					endforeach()
				endif()
			endforeach()
		endif(QT_PLUGINS_DIR)

		# Create a qt.conf file in 'share/hyperhdr/bin' to override hard-coded search paths in Qt plugins
		file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "[Paths]\nPlugins=../lib/\n")
		install(
			FILES "${CMAKE_BINARY_DIR}/qt.conf"
			DESTINATION "share/hyperhdr/bin"
			COMPONENT "HyperHDR"
		)

		# Copy dependencies to 'share/hyperhdr/lib'
		foreach(PREREQUISITE_LIB ${PREREQUISITE_LIBS})
			message("Installing: " ${PREREQUISITE_LIB})
			install(
				FILES ${PREREQUISITE_LIB}
				DESTINATION "share/hyperhdr/lib"
				COMPONENT "HyperHDR"
			)
		endforeach()
		
		# install LUT
		install(FILES "${PROJECT_SOURCE_DIR}/resources/lut/lut_lin_tables.tar.xz" DESTINATION "share/hyperhdr/lut" COMPONENT "HyperHDR")		
	else()
		# Run CMake after target was built to run get_prerequisites on ${TARGET_FILE}
		add_custom_command(
			TARGET ${TARGET} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" "-DTARGET_FILE=$<TARGET_FILE:${TARGET}>"
			ARGS ${CMAKE_SOURCE_DIR}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			VERBATIM
		)
	endif()
endmacro()

macro(DeployWindows TARGET)
	if(EXISTS ${TARGET_FILE})
		message(STATUS "Collecting Dependencies for target file: ${TARGET_FILE}")
		find_package(Qt5Core REQUIRED)

		# Find the windeployqt binaries
		get_target_property(QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
		get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
		find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

		# Collect the runtime libraries
		get_filename_component(COMPILER_PATH "${CMAKE_CXX_COMPILER}" DIRECTORY)
		set(WINDEPLOYQT_PARAMS --no-angle --no-opengl-sw)

		execute_process(
			COMMAND "${CMAKE_COMMAND}" -E
			env "PATH=${COMPILER_PATH};${QT_BIN_DIR}" "${WINDEPLOYQT_EXECUTABLE}"
			--dry-run
			${WINDEPLOYQT_PARAMS}
			--list mapping
			"${TARGET_FILE}"
			OUTPUT_VARIABLE DEPS
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		# Parse DEPS into a semicolon-separated list.
		separate_arguments(DEPENDENCIES WINDOWS_COMMAND ${DEPS})
		string(REPLACE "\\" "/" DEPENDENCIES "${DEPENDENCIES}")

		# Copy dependencies to 'hyperhdr/lib' or 'hyperhdr'
		while (DEPENDENCIES)
			list(GET DEPENDENCIES 0 src)
			list(GET DEPENDENCIES 1 dst)
			get_filename_component(dst ${dst} DIRECTORY)

			if (NOT "${dst}" STREQUAL "")
				install(
					FILES ${src}
					DESTINATION "lib/${dst}"
					COMPONENT "HyperHDR"
				)
			else()
				install(
					FILES ${src}
					DESTINATION "bin"
					COMPONENT "HyperHDR"
				)
			endif()

			list(REMOVE_AT DEPENDENCIES 0 1)
		endwhile()

		# Copy TurboJPEG Libs
		if (TURBOJPEG_FOUND)
			find_file(TurboJPEG_DLL
				NAMES "turbojpeg.dll" "jpeg62.dll"
				PATHS "${CMAKE_SOURCE_DIR}/resources/dll/jpeg"
				NO_DEFAULT_PATH
			)

			install(
				FILES ${TurboJPEG_DLL}
				DESTINATION "bin"
				COMPONENT "HyperHDR"
			)
		endif(TURBOJPEG_FOUND)
		
		# Copy usb Libs
		if (LIBUSB_1_LIBRARIES)
			find_file(USBLIB_DLL
				NAMES "libusb-1.0.dll"
				PATHS "${CMAKE_SOURCE_DIR}/resources/dll/libusb"
				NO_DEFAULT_PATH
				REQUIRED
			)

			install(
				FILES ${USBLIB_DLL}
				DESTINATION "bin"
				COMPONENT "HyperHDR"
			)
			
			find_file(HIDLIB_DLL
				NAMES "hidapi.dll"
				PATHS "${CMAKE_SOURCE_DIR}/resources/dll/libusb"
				NO_DEFAULT_PATH
				REQUIRED
			)

			install(
				FILES ${HIDLIB_DLL}
				DESTINATION "bin"
				COMPONENT "HyperHDR"				
			)
		endif(LIBUSB_1_LIBRARIES)

		# Create a qt.conf file in 'bin' to override hard-coded search paths in Qt plugins
		file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "[Paths]\nPlugins=../lib/\n")
		install(
			FILES "${CMAKE_BINARY_DIR}/qt.conf"
			DESTINATION "bin"
			COMPONENT "HyperHDR"
		)

		execute_process(
			COMMAND ${SEVENZIP_BIN} e ${PROJECT_SOURCE_DIR}/resources/lut/lut_lin_tables.tar.xz -o${CMAKE_CURRENT_BINARY_DIR} -aoa -y
			RESULT_VARIABLE STATUS
			OUTPUT_VARIABLE OUTPUT1			
		)
		if(STATUS AND NOT STATUS EQUAL 0)
		    message( FATAL_ERROR "LUT tar.xz Bad exit status")
		else()
		    message( STATUS "LUT tar.xz tar extracted")			
		endif()

		execute_process(
			COMMAND ${SEVENZIP_BIN} e ${CMAKE_CURRENT_BINARY_DIR}/lut_lin_tables.tar -o${CMAKE_CURRENT_BINARY_DIR} -aoa -y 
			RESULT_VARIABLE STATUS
			OUTPUT_VARIABLE OUTPUT1			
		)
		if(STATUS AND NOT STATUS EQUAL 0)
		    message( FATAL_ERROR "LUT tar Bad exit status")
		else()
		    message( STATUS "LUT tar extracted")			
		endif()

		install(
			FILES ${CMAKE_CURRENT_BINARY_DIR}/lut_lin_tables.3d
			DESTINATION "bin"
			COMPONENT "HyperHDR"
		)


		INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT "HyperHDR")

	else()
		# Run CMake after target was built
		add_custom_command(
			TARGET ${TARGET} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" "-DTARGET_FILE=$<TARGET_FILE:${TARGET}>"
			ARGS ${CMAKE_SOURCE_DIR}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			VERBATIM
		)
	endif()
endmacro()
