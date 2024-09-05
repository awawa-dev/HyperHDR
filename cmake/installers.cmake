macro(DeployApple TARGET)
	if(EXISTS ${TARGET_FILE})
		install ( TARGETS hyperhdr DESTINATION "share/.." COMPONENT "HyperHDR" )

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
		install(FILES "${PROJECT_SOURCE_DIR}/cmake/osx/Hyperhdr.icns" DESTINATION "hyperhdr.app/Contents/Resources" COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "hyperhdr.app/Contents/Resources" COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION "hyperhdr.app/Contents/Resources" COMPONENT "HyperHDR")

		# Copy QMQTT
		if (USE_SHARED_LIBS AND TARGET qmqtt)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:qmqtt> DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
			install(CODE [[ file(INSTALL FILES $<TARGET_SONAME_FILE:qmqtt> DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy SQLITE3
		if (USE_SHARED_LIBS AND TARGET sqlite3)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:sqlite3> DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy utils-xz
		if (USE_SHARED_LIBS AND TARGET utils-xz)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-xz> DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy utils-image
		if (USE_SHARED_LIBS AND TARGET utils-image)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-image> DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

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
		install(CODE "set(SCOPE_Qt_VERSION ${Qt_VERSION})"               COMPONENT "HyperHDR")
		install(CODE "set(SCOPE_CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})" COMPONENT "HyperHDR")
		install(CODE [[
				execute_process(
					COMMAND brew --prefix openssl@3
					RESULT_VARIABLE BREW_OPENSSL3
					OUTPUT_VARIABLE BREW_OPENSSL3_PATH
					OUTPUT_STRIP_TRAILING_WHITESPACE
				)
				if (BREW_OPENSSL3 EQUAL 0 AND EXISTS "${BREW_OPENSSL3_PATH}/lib")
					set(BREW_OPENSSL3_LIB "${BREW_OPENSSL3_PATH}/lib")
					message("Found OpenSSL@3 at ${BREW_OPENSSL3_LIB}")
					file(GLOB filesSSL3 "${BREW_OPENSSL3_LIB}/*")
					if (NOT (SCOPE_Qt_VERSION EQUAL 5))
						list (APPEND filesSSL ${filesSSL3})
					else()
						message("Skipping OpenSSL@3 for Qt5")
					endif()
				endif()

				execute_process(
					COMMAND brew --prefix openssl@1.1
					RESULT_VARIABLE BREW_OPENSSL1
					OUTPUT_VARIABLE BREW_OPENSSL1_PATH
					OUTPUT_STRIP_TRAILING_WHITESPACE
				)
				if (BREW_OPENSSL1 EQUAL 0 AND EXISTS "${BREW_OPENSSL1_PATH}/lib")
					set(BREW_OPENSSL1_LIB "${BREW_OPENSSL1_PATH}/lib")
					message("Found OpenSSL@1.1 at ${BREW_OPENSSL1_LIB}")
					file(GLOB filesSSL1 "${BREW_OPENSSL1_LIB}/*")
					list (APPEND filesSSL ${filesSSL1})
				endif()

				#OpenSSL
				if(filesSSL)
					list( REMOVE_DUPLICATES filesSSL)
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
				  
				list(LENGTH _u_deps _u_length)
				if("${_u_length}" GREATER 0)
					message(WARNING "Unresolved dependencies detected!")
				endif()

				foreach(PLUGIN "tls")
					if(EXISTS ${MYQT_PLUGINS_DIR}/${PLUGIN})
						file(GLOB files "${MYQT_PLUGINS_DIR}/${PLUGIN}/*openssl*")
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

			message( "Detected architecture: '${SCOPE_CMAKE_SYSTEM_PROCESSOR}'")
			if(SCOPE_CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
				cmake_policy(PUSH)
					cmake_policy(SET CMP0009 NEW)
					message( "Re-signing bundle's components...")
					file(GLOB_RECURSE libSignFramework LIST_DIRECTORIES false "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/Frameworks/*")
					list(APPEND libSignFramework "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/MacOS/hyperhdr")
					foreach(_fileToSign ${libSignFramework})
						string(FIND ${_fileToSign} ".framework/Resources" isResources)
						if (${isResources} EQUAL -1)
							execute_process(COMMAND bash -c "codesign --force -s - ${_fileToSign}" RESULT_VARIABLE CODESIGN_VERIFY)
							if(NOT CODESIGN_VERIFY EQUAL 0)
								message(WARNING "Failed to repair the component signature: signing failed for ${_fileToSign}")
							endif()
						endif()			
					endforeach()
					message( "Perform final verification...")
					execute_process(COMMAND bash -c "codesign --verify --deep -vvvv ${CMAKE_INSTALL_PREFIX}/hyperhdr.app" RESULT_VARIABLE CODESIGN_VERIFY)
					if(NOT CODESIGN_VERIFY EQUAL 0)
						message(WARNING "Failed to repair the bundle signature: verification failed")
					endif()
				cmake_policy(POP)
			endif()
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
		include(GetPrerequisites)

		# Install LUT		
		install(FILES "${PROJECT_SOURCE_DIR}/resources/lut/lut_lin_tables.tar.xz" DESTINATION "share/hyperhdr/lut" COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "share/hyperhdr" COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION "share/hyperhdr" COMPONENT "HyperHDR")

		# HyperHDR bin & res
		install ( TARGETS hyperhdr DESTINATION "share/hyperhdr/bin" COMPONENT "HyperHDR" )
		install ( DIRECTORY ${CMAKE_SOURCE_DIR}/cmake/linux/service DESTINATION "share/hyperhdr/" COMPONENT "HyperHDR" )
		install ( FILES ${CMAKE_SOURCE_DIR}/cmake/linux/resources/hyperhdr-icon-32px.png DESTINATION "share/hyperhdr/icons" COMPONENT "HyperHDR" )
		install ( FILES ${CMAKE_SOURCE_DIR}/cmake/linux/resources/hyperhdr_128.png DESTINATION "share/hyperhdr/desktop" COMPONENT "HyperHDR" )
		install ( FILES ${CMAKE_SOURCE_DIR}/cmake/linux/resources/hyperhdr.desktop DESTINATION "share/hyperhdr/desktop" COMPONENT "HyperHDR" )


		# Our and custom libs
		set(PREREQUISITE_LIBS "")

		# Web resources
		if (NOT USE_EMBEDDED_WEB_RESOURCES)
			if (NOT TARGET webserver-resources)
				message(FATAL_ERROR "Could not find compiled resources for the web server. Consider using USE_EMBEDDED_WEB_RESOURCES option" )
			endif()
			get_target_property(webserver-resources-path webserver-resources OUTPUT_FILE)
			install(FILES ${webserver-resources-path} DESTINATION "share/hyperhdr/lib" COMPONENT "HyperHDR" )
		endif()

		# Copy SMART-X11 lib
		if (TARGET smart-x11)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:smart-x11> DESTINATION "${CMAKE_INSTALL_PREFIX}/share/hyperhdr/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy SMART-PIPEWIRE lib
		if (TARGET smart-pipewire)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:smart-pipewire> DESTINATION "${CMAKE_INSTALL_PREFIX}/share/hyperhdr/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy UTILS-IMAGE lib
		if (TARGET utils-image)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-image> DESTINATION "${CMAKE_INSTALL_PREFIX}/share/hyperhdr/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy UTILS-XZ lib
		if (TARGET utils-image)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-xz> DESTINATION "${CMAKE_INSTALL_PREFIX}/share/hyperhdr/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()


		#OpenSSL
		find_package(OpenSSL)
		if(OPENSSL_FOUND)
			foreach(openssl_lib ${OPENSSL_LIBRARIES})
				message(STATUS "Adding OpenSSL(1): ${openssl_lib}")
				gp_append_unique(PREREQUISITE_LIBS ${openssl_lib})
				get_filename_component(file_canonical ${openssl_lib} REALPATH)
				message(STATUS "Adding OpenSSL(2): ${file_canonical}")
				gp_append_unique(PREREQUISITE_LIBS ${file_canonical})

				get_filename_component(rootLib ${openssl_lib} NAME)
				get_filename_component(rootPath ${openssl_lib} DIRECTORY)
				file(GLOB resultSSL "${rootPath}/${rootLib}*")
				foreach(targetlib ${resultSSL})
					message("=>${targetlib}")
					gp_append_unique(PREREQUISITE_LIBS ${targetlib})
				endforeach()
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
		if (ENABLE_CEC)
			# xrandr
			find_library(XRANDR_LIBRARY NAMES Xrandr libXrandr libXrandr.so.2)
			if (XRANDR_LIBRARY)
				get_filename_component(resolvedXrandr ${XRANDR_LIBRARY} ABSOLUTE)
				list (APPEND cecFiles ${resolvedXrandr})
			endif()

			# cec core
			foreach(resolved_file_in ${CEC_LIBRARIES})
				message(STATUS "Adding CEC: ${resolved_file_in}")
				unset(LIBCEC CACHE)
				find_library(LIBCEC NAMES ${resolved_file_in})
				if (LIBCEC)
					get_filename_component(resolvedCec ${LIBCEC} ABSOLUTE)
					list (APPEND cecFiles ${resolvedCec})
				endif()
			endforeach()			

			# install cec
			foreach(cecFile ${cecFiles})
				FILE(GLOB foundCec "${cecFile}*")
				foreach(installCec ${foundCec})
					message(STATUS "Adding CEC: ${installCec}")
					gp_append_unique(PREREQUISITE_LIBS ${installCec})
				endforeach()				
			endforeach()
		endif()
				
		# Create a qt.conf file in 'share/hyperhdr/bin' to override hard-coded search paths in Qt plugins
		file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "[Paths]\nPlugins=../lib/plugins/\n")
		install(
			FILES "${CMAKE_BINARY_DIR}/qt.conf"
			DESTINATION "share/hyperhdr/bin"
			COMPONENT "HyperHDR"
		)

		# symlink
		install( CODE "EXECUTE_PROCESS(COMMAND ln -sf \"../share/hyperhdr/bin/hyperhdr\" \"${CMAKE_BINARY_DIR}/symlink_hyperhdr\" )" COMPONENT "HyperHDR" )
		install( FILES ${CMAKE_BINARY_DIR}/symlink_hyperhdr DESTINATION "bin" RENAME hyperhdr COMPONENT "HyperHDR" )
		install( CODE "FILE (REMOVE ${CMAKE_BINARY_DIR}/symlink_hyperhdr )" COMPONENT "HyperHDR" )

		# install CODE 	
		install(CODE "set(TARGET_FILE \"${TARGET_FILE}\")"					COMPONENT "HyperHDR")
		install(CODE "set(PREREQUISITE_LIBS \"${PREREQUISITE_LIBS}\")"		COMPONENT "HyperHDR")
		install(CODE "set(QT_PLUGINS_DIR \"${QT_PLUGINS_DIR}\")"			COMPONENT "HyperHDR")

		install(CODE [[

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
			"libz"
			"libxrender1"
			"libxi6"
			"libxext6"
			"libx11-xcb1"
			"libsm"
			"libice6"
			"libdrm2"
			"libxkbcommon0"
			"libwacom2"
			"libmtdev1"
			"libinput10"
			"libgudev-1.0-0"
			"libffi6"
			"libevdev2"
			"libuuid1"
			"libselinux1"
			"libmount1"
			"libblkid1"
			"libwayland"
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
			"ld-"
			"libasound"
			"libblkid"
			"libbrotlicommon"
			"libbrotlidec"
			"libffi"
			"libgio-2"
			"libgmodule-2"
			"libgobject-2"
			"libidn2"
			"libnghttp"
			"libsystemd"
			"libpsl"
			"libunistring"
			"libssh"
			"libselinux"
			"libevent-2"
			"libldap"
			"libutils"
		)

		#message(STATUS "Collecting Dependencies for target file: ${TARGET_FILE}")
		include(GetPrerequisites)		
		# Extract dependencies ignoring the system ones
		if (NOT CMAKE_CROSSCOMPILING)
			# get HyperHDR deps
			file(GET_RUNTIME_DEPENDENCIES
				RESOLVED_DEPENDENCIES_VAR DEPENDENCIES
				EXECUTABLES ${TARGET_FILE}
			)

			# get Systray deps (always present even if it's a dummy implementation)
			file(INSTALL FILES $<TARGET_FILE:systray-widget> DESTINATION "${CMAKE_INSTALL_PREFIX}/share/hyperhdr/lib" TYPE SHARED_LIBRARY)
			file(GET_RUNTIME_DEPENDENCIES
				RESOLVED_DEPENDENCIES_VAR SYS_DEPENDENCIES
				EXECUTABLES $<TARGET_FILE:systray-widget>)
			foreach(systrayLib ${SYS_DEPENDENCIES})
				string(FIND ${systrayLib} "libayatana" _sysindex)
				if (${_sysindex} GREATER -1)
					list(APPEND DEPENDENCIES ${systrayLib})
				endif()
				string(FIND ${systrayLib} "libdbusmenu" _sysDBusindex)
				if (${_sysDBusindex} GREATER -1)
					list(APPEND DEPENDENCIES ${systrayLib})
				endif()
			endforeach()						
		endif()

		# Copy Qt plugins to 'share/hyperhdr/lib/plugins'
		foreach(PLUGIN "tls")
			#message(WARNING "Collecting Dependencies for QT plugin folder: ${PLUGIN}")
			if(EXISTS ${QT_PLUGINS_DIR}/${PLUGIN})
				file(GLOB files "${QT_PLUGINS_DIR}/${PLUGIN}/*openssl*")
				foreach(file ${files})
					file(GET_RUNTIME_DEPENDENCIES
						RESOLVED_DEPENDENCIES_VAR QT_DEPENDENCIES
						EXECUTABLES ${file}
					)
					#message(WARNING "${file} => ${DEPENDENCIES} <= ${QT_DEPENDENCIES}")
					list(APPEND DEPENDENCIES ${QT_DEPENDENCIES})

					file(INSTALL
						DESTINATION "${CMAKE_INSTALL_PREFIX}/share/hyperhdr/lib/plugins/${PLUGIN}"
						TYPE SHARED_LIBRARY
						FILES ${file}
					)
						
				endforeach()
			endif()
		endforeach()

		# Append symlink and non-symlink dependencies to the list		
		foreach(DEPENDENCY ${DEPENDENCIES})
			get_filename_component(resolved ${DEPENDENCY} NAME_WE)
			
			foreach(myitem ${SYSTEM_LIBS_SKIP})
				string(FIND ${resolved} ${myitem} _index)
				if (${_index} EQUAL 0)
					break()									
				endif()
			endforeach()
					
			if (${_index} EQUAL 0)
				continue() # Skip system libraries
			else()
				gp_resolve_item("${TARGET_FILE}" "${DEPENDENCY}" "" "" resolved_file)
				get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
				gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
				get_filename_component(file_canonical ${resolved_file} REALPATH)
				gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
				#message("Basic check added: ${resolved_file} (${resolved})")
			endif()
		endforeach()		

		# Copy dependencies to 'share/hyperhdr/lib/external'
		foreach(PREREQUISITE_LIB ${PREREQUISITE_LIBS})
			set(FILE_TO_INSTALL ${PREREQUISITE_LIB})
			string(FIND ${PREREQUISITE_LIB} "libproxy" libproxyindex)
			string(FIND ${PREREQUISITE_LIB} "libpxbackend" libpxbackendindex)
			if((NOT IS_SYMLINK ${PREREQUISITE_LIB}) AND (${libproxyindex} GREATER -1 OR ${libpxbackendindex} GREATER -1))				
				get_filename_component(pathingFilename ${PREREQUISITE_LIB} NAME)
				set(FILE_TO_INSTALL "${CMAKE_BINARY_DIR}/${pathingFilename}")
				message("Patching RPATH: ${FILE_TO_INSTALL}")
				file(COPY_FILE ${PREREQUISITE_LIB} ${FILE_TO_INSTALL} )				
				execute_process (
					COMMAND bash -c "chrpath -d ${FILE_TO_INSTALL}"
					OUTPUT_VARIABLE outputResult
				)
			endif()
			message("Installing: " ${FILE_TO_INSTALL})
			file(
				INSTALL
				FILES ${FILE_TO_INSTALL}
				DESTINATION "${CMAKE_INSTALL_PREFIX}/share/hyperhdr/lib/external"
				TYPE SHARED_LIBRARY
			)
		endforeach()		
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

macro(DeployWindows TARGET)
	if(EXISTS ${TARGET_FILE})

		install ( TARGETS hyperhdr DESTINATION "bin" COMPONENT "HyperHDR" )
		include( InstallRequiredSystemLibraries )

		message("Collecting Dependencies for target file: ${TARGET_FILE}")
		
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
		install(FILES ${TurboJPEG_INSTALL_LIB} DESTINATION "bin" COMPONENT "HyperHDR" )

		# Web resources
		if (NOT USE_EMBEDDED_WEB_RESOURCES)
			if (NOT TARGET webserver-resources)
				message(FATAL_ERROR "Could not find compiled resources for the web server. Consider using USE_EMBEDDED_WEB_RESOURCES option" )
			endif()
			get_target_property(webserver-resources-path webserver-resources OUTPUT_FILE)
			install(FILES ${webserver-resources-path} DESTINATION "lib" COMPONENT "HyperHDR" )
		endif()

		# Copy QMQTT
		if (USE_SHARED_LIBS AND TARGET qmqtt)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:qmqtt> DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy SQLITE3
		if (USE_SHARED_LIBS AND TARGET sqlite3)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:sqlite3> DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy utils-xz
		if (USE_SHARED_LIBS AND TARGET utils-xz)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-xz> DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Copy utils-image
		if (USE_SHARED_LIBS AND TARGET utils-image)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-image> DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
		endif()

		# Create a qt.conf file in 'bin' to override hard-coded search paths in Qt plugins
		file(WRITE "${CMAKE_BINARY_DIR}/qt.conf" "[Paths]\nPlugins=../lib/\n")
		install(
			FILES "${CMAKE_BINARY_DIR}/qt.conf"
			DESTINATION "bin"
			COMPONENT "HyperHDR"
		)		

		install(
			FILES ${PROJECT_SOURCE_DIR}/resources/lut/lut_lin_tables.tar.xz
			DESTINATION "bin"
			COMPONENT "HyperHDR"
		)

		INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT "HyperHDR")
		
		install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION bin COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION bin COMPONENT "HyperHDR")

		find_package(OpenSSL QUIET)

		find_file(OPENSSL_SSL
			NAMES libssl-3-x64.dll libssl-1_1-x64.dll libssl-1_1.dll libssl ssleay32.dll ssl.dll
			PATHS "C:/Program Files/OpenSSL" "C:/Program Files/OpenSSL-Win64" ${_OPENSSL_ROOT_PATHS}
			PATH_SUFFIXES bin
		)

		find_file(OPENSSL_CRYPTO
			NAMES libcrypto-3-x64.dll libcrypto-1_1-x64.dll libcrypto-1_1.dll libcrypto libeay32.dll crypto.dll
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
			message( WARNING "OpenSSL NOT found. HyperHDR's https instance and Philips Hue devices will not work.")
		endif()

	else()
		if (Qt_VERSION EQUAL 5)
			get_target_property(QT_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE} IMPORTED_LOCATION)
			get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
			find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
		else()
			get_filename_component(My_Qt6Core_EXECUTABLE_DIR ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS} ABSOLUTE)
			find_program(WINDEPLOYQT_EXECUTABLE windeployqt PATHS "${My_Qt6Core_EXECUTABLE_DIR}" NO_DEFAULT_PATH)
			if (NOT WINDEPLOYQT_EXECUTABLE)
				find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
			endif()
		endif()
		
		if (WINDEPLOYQT_EXECUTABLE AND (NOT CMAKE_GITHUB_ACTION))
			set(WINDEPLOYQT_PARAMS_RUNTIME --verbose 0 --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler)
			message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE} PATH_HINT:${My_Qt6Core_EXECUTABLE_DIR}${QT_BIN_DIR}")
			add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${WINDEPLOYQT_EXECUTABLE} ${WINDEPLOYQT_PARAMS_RUNTIME} "$<TARGET_FILE:${TARGET}>")
			add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TurboJPEG_INSTALL_LIB} $<TARGET_FILE_DIR:${TARGET}>)
		endif()

		add_custom_command(
			TARGET ${TARGET} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" "-DTARGET_FILE=$<TARGET_FILE:${TARGET}>"
			ARGS ${CMAKE_SOURCE_DIR}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			VERBATIM
		)
	endif()
endmacro()
