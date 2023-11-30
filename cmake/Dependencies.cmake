macro(DeployApple TARGET)
	if(EXISTS ${TARGET_FILE})
		execute_process(
			COMMAND bash -c "cd ${CMAKE_CURRENT_BINARY_DIR} && tar -xzf ${PROJECT_SOURCE_DIR}/resources/lut/lut_lin_tables.tar.xz"
			RESULT_VARIABLE STATUS
			OUTPUT_VARIABLE OUTPUT1			
		)			
		if(STATUS AND NOT STATUS EQUAL 0)
			message( FATAL_ERROR "LUT tar.xz Bad exit status (xz-tools installed?)")
		else()
			message( STATUS "LUT tar.xz tar extracted")			
		endif()

		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lut_lin_tables.3d" DESTINATION "hyperhdr.app/Contents/lut" COMPONENT "HyperHDR")			
		install(FILES "${PROJECT_SOURCE_DIR}/cmake/osxbundle/Hyperhdr.icns" DESTINATION "hyperhdr.app/Contents/Resources" COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "hyperhdr.app/Contents/Resources" COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION "hyperhdr.app/Contents/Resources" COMPONENT "HyperHDR")

		if ( Qt5Core_FOUND )			
			get_target_property(MYQT_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE} IMPORTED_LOCATION)		
		else()
			SET (MYQT_QMAKE_EXECUTABLE "${_qt_import_prefix}/../../../bin/qmake")
		endif()

		execute_process(
			COMMAND ${MYQT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
			OUTPUT_VARIABLE MYQT_PLUGINS_DIR
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		install(CODE "set(MYQT_PLUGINS_DIR \"${MYQT_PLUGINS_DIR}\")"     COMPONENT "HyperHDR")
		install(CODE "set(MY_DEPENDENCY_PATHS \"${TARGET_FILE}\")"       COMPONENT "HyperHDR")
		install(CODE "set(MY_SYSTEM_LIBS_SKIP \"${SYSTEM_LIBS_SKIP}\")"  COMPONENT "HyperHDR")
		install(CODE [[
				#OpenSSL
				if(EXISTS "/usr/local/opt/openssl@1.1/lib" AND IS_DIRECTORY "/usr/local/opt/openssl@1.1/lib")
					message( STATUS "Including OpenSSL libraries")
					file(GLOB filesSSL "/usr/local/opt/openssl@1.1/lib/*")
					foreach(openssl_lib ${filesSSL})
						string(FIND ${openssl_lib} "dylib" _indexSSL)
						if (${_indexSSL} GREATER -1)
							file(INSTALL
								DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/Frameworks"
								TYPE SHARED_LIBRARY
								FILES "${openssl_lib}"
							)
						else()
							file(INSTALL
								DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib"
								TYPE SHARED_LIBRARY
								FILES "${openssl_lib}"
							)
						endif()
					endforeach()
				else()
					message( WARNING "OpenSSL NOT found (https instance will not work)")
				endif()
				
				file(GET_RUNTIME_DEPENDENCIES
					EXECUTABLES ${MY_DEPENDENCY_PATHS}
					RESOLVED_DEPENDENCIES_VAR _r_deps
					UNRESOLVED_DEPENDENCIES_VAR _u_deps
				)
				  
				foreach(_file ${_r_deps})										
					string(FIND ${_file} "dylib" _index)
					if (${_index} GREATER -1)
						file(INSTALL
							DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/Frameworks"
							TYPE SHARED_LIBRARY
							FILES "${_file}"
						)
					else()
						file(INSTALL
							DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib"
							TYPE SHARED_LIBRARY
							FILES "${_file}"
						)
					endif()
				endforeach()
				
				if (NOT Qt5Core_FOUND AND EXISTS "/usr/local/lib/libbrotlicommon.1.dylib")
					file(INSTALL
						DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib"
						TYPE SHARED_LIBRARY
						FOLLOW_SYMLINK_CHAIN
						FILES "/usr/local/lib/libbrotlicommon.1.dylib")
				endif()

				if (EXISTS "/usr/local/lib/libsharpyuv.0.dylib")
					file(INSTALL
						DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib"
						TYPE SHARED_LIBRARY
						FOLLOW_SYMLINK_CHAIN
						FILES "/usr/local/lib/libsharpyuv.0.dylib")
				endif()					
				  
				list(LENGTH _u_deps _u_length)
				if("${_u_length}" GREATER 0)
					message(WARNING "Unresolved dependencies detected!")
				endif()
				  
				foreach(PLUGIN "platforms" "sqldrivers" "imageformats")
				if(EXISTS ${MYQT_PLUGINS_DIR}/${PLUGIN})
					file(GLOB files "${MYQT_PLUGINS_DIR}/${PLUGIN}/*")
					foreach(file ${files})							
							file(GET_RUNTIME_DEPENDENCIES
							EXECUTABLES ${file}
							RESOLVED_DEPENDENCIES_VAR PLUGINS
							UNRESOLVED_DEPENDENCIES_VAR _u_deps				
							)

						foreach(DEPENDENCY ${PLUGINS})
								file(INSTALL
									DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib"
									TYPE SHARED_LIBRARY
									FILES ${DEPENDENCY}
								)									
						endforeach()
							
						get_filename_component(singleQtLib ${file} NAME)
						list(APPEND MYQT_PLUGINS "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/plugins/${PLUGIN}/${singleQtLib}")
						file(INSTALL
							DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/plugins/${PLUGIN}"
							TYPE SHARED_LIBRARY
							FILES ${file}
						)
							
					endforeach()
				endif()
			endforeach()
			
			include(BundleUtilities)							
			fixup_bundle("${CMAKE_INSTALL_PREFIX}/hyperhdr.app" "${MYQT_PLUGINS}" "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib")
				
			file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib")			
			file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/share")
		]] COMPONENT "HyperHDR")
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

macro(DeployUnix TARGET)
	if (EXISTS ${TARGET_FILE})
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
			"libxcb-xkb1"
			"libxkbcommon-x11-0"			
			"libssl1.1"
		)

		# Extract dependencies ignoring the system ones
		if (NOT CMAKE_CROSSCOMPILING)
			get_prerequisites(${TARGET_FILE} DEPENDENCIES 0 1 "" "")
		endif()

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
		
		# Copy SMARTX11 lib
		find_library(LIBSMARTX11
			NAMES "smartX11" "smartX11.so"
			PATHS "${CMAKE_BINARY_DIR}/lib"
			NO_DEFAULT_PATH
		)
		if (LIBSMARTX11)
			SET(resolved_file ${LIBSMARTX11})		
			get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
			gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
			message(STATUS "Adding smartX11: ${resolved_file}")		
			get_filename_component(file_canonical ${resolved_file} REALPATH)
			gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
			message(STATUS "Added smartX11(2): ${file_canonical}")
		endif()

		# Copy SMARTPIPEWIRE lib
		find_library(LIBSMARTPIPEWIRE
			NAMES "smartPipewire" "smartPipewire.so"
			PATHS "${CMAKE_BINARY_DIR}/lib"
			NO_DEFAULT_PATH
		)
		if (LIBSMARTPIPEWIRE)
			SET(resolved_file ${LIBSMARTPIPEWIRE})
			get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
			gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
			message(STATUS "Adding smartPipewire: ${resolved_file}")		
			get_filename_component(file_canonical ${resolved_file} REALPATH)
			gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
			message(STATUS "Added smartPipewire(2): ${file_canonical}")
		endif()		
		
		#OpenSSL
		find_package(OpenSSL)
		if(OPENSSL_FOUND)
			foreach(openssl_lib ${OPENSSL_LIBRARIES})
				gp_append_unique(PREREQUISITE_LIBS ${openssl_lib})
				get_filename_component(file_canonical ${openssl_lib} REALPATH)
				gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
			endforeach()
		else()
			message( WARNING "OpenSSL NOT found (https instance will not work)")
		endif()

		# Detect the Qt5 plugin directory, source: https://github.com/lxde/lxqt-qtplugin/blob/master/src/CMakeLists.txt
		if ( Qt5Core_FOUND )			
			get_target_property(QT_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE} IMPORTED_LOCATION)
			execute_process(
				COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
				OUTPUT_VARIABLE QT_PLUGINS_DIR
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)		
		elseif ( TARGET Qt${QT_VERSION_MAJOR}::qmake )
			get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
			execute_process(
				COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
				OUTPUT_VARIABLE QT_PLUGINS_DIR
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
		endif()

		message(STATUS "QT plugin path: ${QT_PLUGINS_DIR}")
		
		# Copy CEC lib
		if (CEC_FOUND)

			find_library(XRANDR_LIBRARY NAMES Xrandr libXrandr libXrandr.so.2)

			if (XRANDR_LIBRARY)
				SET(resolved_file ${XRANDR_LIBRARY})
				get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
				message(STATUS "Adding xrandr: ${resolved_file}")		
				get_filename_component(file_canonical ${resolved_file} REALPATH)
				gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
				message(STATUS "Added xrandr(2): ${file_canonical}")
			endif()

			foreach(resolved_file_in ${CEC_LIBRARIES})
				message(STATUS "Adding CEC: ${resolved_file_in}")
				unset(LIBCEC CACHE)
				find_library(LIBCEC NAMES ${resolved_file_in})
				if (LIBCEC)
					SET(resolved_file ${LIBCEC})
					get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
					gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
					message(STATUS "Adding CEC(1): ${resolved_file}")
					get_filename_component(file_canonical ${resolved_file} REALPATH)
					gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
					message(STATUS "Added CEC(2): ${file_canonical}")
					foreach(indexer RANGE 9)
						set(resolved_fileLink "${resolved_file}.${indexer}")
						if(EXISTS ${resolved_fileLink})
							get_filename_component(resolved_fileLink ${resolved_fileLink} ABSOLUTE)
							gp_append_unique(PREREQUISITE_LIBS ${resolved_fileLink})
							message(STATUS "Adding CEC(3): ${resolved_fileLink}")
						endif()
					endforeach()
				endif()
			endforeach()
		endif()
		
		if ( GLD )
			SET(resolved_file ${GLD})
			message(STATUS "Adding GLD: ${resolved_file}")
			get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
			gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
			message(STATUS "Added GLD: ${resolved_file}")
			set(resolved_file0 "${resolved_file}.0")
			if(EXISTS ${resolved_file0})
				message(STATUS "Adding GLD0: ${resolved_file0}")
				get_filename_component(resolved_file0 ${resolved_file0} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file0})
				message(STATUS "Added GLD0: ${resolved_file0}")
			endif()
			get_filename_component(file_canonical ${resolved_file} REALPATH)
			gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
			message(STATUS "Added GLD: ${file_canonical}")
		endif()

		
		find_library(LIB_XCB
			NAMES libxcb libxcb.so
		)

		if(LIB_XCB)
			message(STATUS "libXCB found ${LIB_XCB}")
			SET(resolved_file ${LIB_XCB})
			message(STATUS "Adding libXCB: ${resolved_file}")
			get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
			gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
			message(STATUS "Added libXCB: ${resolved_file}")
			set(resolved_file1 "${resolved_file}.1")
			if(EXISTS ${resolved_file1})
				message(STATUS "Adding libXCB1: ${resolved_file1}")
				get_filename_component(resolved_file1 ${resolved_file1} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file1})
				message(STATUS "Added libXCB1: ${resolved_file1}")
			endif()
			get_filename_component(file_canonical ${resolved_file} REALPATH)
			gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
			message(STATUS "Added: ${file_canonical}")
		else()
			message(STATUS "libXCB not found")
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
			set(resolved_file0 "${resolved_file}.0")
			if(EXISTS ${resolved_file0})
				message(STATUS "Adding LIB_GLX0: ${resolved_file0}")
				get_filename_component(resolved_file0 ${resolved_file0} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file0})
				message(STATUS "Added LIB_GLX0: ${resolved_file0}")
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
						if (NOT CMAKE_CROSSCOMPILING)
							get_prerequisites(${file} PLUGINS 0 1 "" "")
						endif()

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
		install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "share/hyperhdr" COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION "share/hyperhdr" COMPONENT "HyperHDR")
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

		# Collect the runtime libraries
		get_filename_component(COMPILER_PATH "${CMAKE_CXX_COMPILER}" DIRECTORY)
		if (Qt_VERSION EQUAL 5)
			set(WINDEPLOYQT_PARAMS --no-angle --no-opengl-sw)
		else()
			set(WINDEPLOYQT_PARAMS --no-opengl-sw)
		endif()

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
		if (ENABLE_MF)
			find_file(TurboJPEG_DLL
				NAMES "turbojpeg.dll" "jpeg62.dll"
				PATHS "${TURBOJPEG_LIBRARY_DIRS}"
				NO_DEFAULT_PATH
				REQUIRED
			)
					
			if(NOT CMAKE_GITHUB_ACTION)
				get_filename_component(JPEG_RUNTIME_TARGET ${TARGET_FILE} DIRECTORY)
				execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TurboJPEG_DLL} ${JPEG_RUNTIME_TARGET})
			endif()

			install(
				FILES ${TurboJPEG_DLL}
				DESTINATION "bin"
				COMPONENT "HyperHDR"
			)
		endif()

		# Copy MQTT Libs
		if (ENABLE_MQTT)
			set (MQTT_TARGET_LIB_FOLDER ${LIBRARY_OUTPUT_PATH}/${CMAKE_BUILD_TYPE})
			message(${MQTT_TARGET_LIB_FOLDER})
			find_file(MQTT_DLL
				NAMES "qmqtt.dll"
				PATHS "${MQTT_TARGET_LIB_FOLDER}"
				NO_DEFAULT_PATH
				REQUIRED
			)
			message(${MQTT_DLL})
			if(NOT CMAKE_GITHUB_ACTION)
				get_filename_component(MQTT_RUNTIME_TARGET ${TARGET_FILE} DIRECTORY)
				execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MQTT_DLL} ${MQTT_RUNTIME_TARGET})
			endif()

			install(
				FILES ${MQTT_DLL}
				DESTINATION "bin"
				COMPONENT "HyperHDR"
			)
		endif()


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
		    message( FATAL_ERROR "LUT tar.xz Bad exit status: ${STATUS} ${OUTPUT1}")
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


		find_package(OpenSSL QUIET)
		
		find_file(OPENSSL_SSL
			NAMES libssl-1_1-x64.dll libssl-1_1.dll libssl ssleay32.dll ssl.dll
			PATHS "C:/Program Files/OpenSSL" "C:/Program Files/OpenSSL-Win64" ${_OPENSSL_ROOT_PATHS}
			PATH_SUFFIXES bin
		)

		find_file(OPENSSL_CRYPTO
			NAMES libcrypto-1_1-x64.dll libcrypto-1_1.dll libcrypto libeay32.dll crypto.dll
			PATHS "C:/Program Files/OpenSSL" "C:/Program Files/OpenSSL-Win64" ${_OPENSSL_ROOT_PATHS}
			PATH_SUFFIXES bin
		)
		
		if(OPENSSL_SSL AND OPENSSL_CRYPTO)
			message( STATUS "OpenSSL found: ${OPENSSL_SSL} ${OPENSSL_CRYPTO}")
			install(
				FILES ${OPENSSL_SSL} ${OPENSSL_CRYPTO}
				DESTINATION "bin"
				COMPONENT "HyperHDR"
			)
		else()
			message( WARNING "OpenSSL NOT found (HyperHDR's https instance will not work)")
		endif()

		INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT "HyperHDR")
		
		install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION bin COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION bin COMPONENT "HyperHDR")

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
