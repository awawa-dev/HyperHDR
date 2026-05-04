macro(InstallerWindows TARGET)
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

		# Copy utils-zstd
		if (USE_SHARED_LIBS AND TARGET utils-zstd)
			install(CODE [[ file(INSTALL FILES $<TARGET_FILE:utils-zstd> DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY) ]] COMPONENT "HyperHDR")
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

		INSTALL(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT "HyperHDR")
		
		install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION bin COMPONENT "HyperHDR")
		install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION bin COMPONENT "HyperHDR")

		find_package(OpenSSL)
		
		get_filename_component(OPENSSL_BASE_DIR "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
		message("OpenSSL default path: ${OPENSSL_BASE_DIR}")

		find_file(OPENSSL_SSL
			NAMES libssl-3-x64.dll libssl-1_1-x64.dll libssl-1_1.dll
			HINTS "${OPENSSL_BASE_DIR}"
			PATHS "C:/Program Files/OpenSSL" "C:/Program Files/OpenSSL-Win64"
			PATH_SUFFIXES bin
		)

		find_file(OPENSSL_CRYPTO
			NAMES libcrypto-3-x64.dll libcrypto-1_1-x64.dll libcrypto-1_1.dll
			HINTS "${OPENSSL_BASE_DIR}"
			PATHS "C:/Program Files/OpenSSL" "C:/Program Files/OpenSSL-Win64"
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
