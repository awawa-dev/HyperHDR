macro(InstallerMacOS TARGET)
	if(EXISTS ${TARGET_FILE})
		cmake_policy(PUSH)
		cmake_policy(SET CMP0177 NEW)	
		install ( TARGETS hyperhdr DESTINATION "share/.." COMPONENT "HyperHDR" )
		cmake_policy(POP)		

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

		# Copy utils-zstd
		if (USE_SHARED_LIBS AND TARGET utils-zstd)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-zstd> DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
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
				  
				foreach(_libfile ${_r_deps})
				    if(_libfile MATCHES "\\.dylib$")
				        set(_dest "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/Frameworks")
				    else()
				        set(_dest "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib")
				    endif()
				    file(COPY "${_libfile}" DESTINATION "${_dest}" FOLLOW_SYMLINK_CHAIN)
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

							get_filename_component(real_file "${file}" REALPATH)
							get_filename_component(singleQtLib ${file} NAME)
							list(APPEND MYQT_PLUGINS "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/plugins/${PLUGIN}/${singleQtLib}")
							file(INSTALL
								DESTINATION "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/plugins/${PLUGIN}"
								TYPE SHARED_LIBRARY
								FILES "${real_file}"
							)

						endforeach()
					endif()
				endforeach()

			include(BundleUtilities)
			fixup_bundle("${CMAKE_INSTALL_PREFIX}/hyperhdr.app" "${MYQT_PLUGINS}" "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib;${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/Frameworks")
			fixup_bundle("${CMAKE_INSTALL_PREFIX}/hyperhdr.app" "" "")
				
			file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/lib")			
			file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/share")

			message( "Detected architecture: '${SCOPE_CMAKE_SYSTEM_PROCESSOR}'")
			if(SCOPE_CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
				cmake_policy(PUSH)
					cmake_policy(SET CMP0009 NEW)
					message( "Re-signing bundle's components...")
					file(GLOB_RECURSE libSignFramework LIST_DIRECTORIES false "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/Frameworks/*" "${CMAKE_INSTALL_PREFIX}/hyperhdr.app/Contents/plugins/tls/*")
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
					execute_process(COMMAND bash -c "codesign --verify --strict --verbose=4 ${CMAKE_INSTALL_PREFIX}/hyperhdr.app" RESULT_VARIABLE CODESIGN_VERIFY)
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
