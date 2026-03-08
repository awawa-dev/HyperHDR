macro(InstallerLinux TARGET)
    include(GNUInstallDirs)

    if (EXISTS ${TARGET_FILE})
        # Podstawowe pliki licencji
        install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${CMAKE_INSTALL_DATADIR}/hyperhdr" COMPONENT "HyperHDR")
        install(FILES "${PROJECT_SOURCE_DIR}/3RD_PARTY_LICENSES" DESTINATION "${CMAKE_INSTALL_DATADIR}/hyperhdr" COMPONENT "HyperHDR")

        # Binarka główna
        install(TARGETS hyperhdr DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT "HyperHDR")
        
        # Plik serwisu (jako szablony dla postinst)
        install(FILES ${CMAKE_SOURCE_DIR}/cmake/linux/service/hyperhdr@.service DESTINATION "/usr/lib/systemd/system" COMPONENT "HyperHDR")

        # Ikony i plik .desktop (Zgodne z FHS, np. /usr/share/applications i /usr/share/icons)
        install(FILES ${CMAKE_SOURCE_DIR}/cmake/linux/resources/hyperhdr-icon-32px.png 
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps" RENAME "hyperhdr.png" COMPONENT "HyperHDR")
        install(FILES ${CMAKE_SOURCE_DIR}/cmake/linux/resources/hyperhdr_128.png 
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps" RENAME "hyperhdr.png" COMPONENT "HyperHDR")
        install(FILES ${CMAKE_SOURCE_DIR}/cmake/linux/resources/hyperhdr.desktop 
                DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT "HyperHDR")

        # Web resources
        if (NOT USE_EMBEDDED_WEB_RESOURCES)
            if (NOT TARGET webserver-resources)
                message(FATAL_ERROR "Could not find compiled resources for the web server. Consider using USE_EMBEDDED_WEB_RESOURCES option" )
            endif()
            get_target_property(webserver-resources-path webserver-resources OUTPUT_FILE)
            install(FILES ${webserver-resources-path} DESTINATION "${CMAKE_INSTALL_DATADIR}/hyperhdr/web" COMPONENT "HyperHDR" )
        endif()

        # Optymalizacja: Pętla dla naszych wewnętrznych bibliotek, customizowanej wersji sqlite lub jesli ich nie ma w publicznym repo
        set(CUSTOM_INTERNAL_LIBS smart-x11 smart-pipewire utils-image utils-zstd systray-widget qmqtt sqlite3 )
        foreach(custom_lib ${CUSTOM_INTERNAL_LIBS})
            if (TARGET ${custom_lib})
                get_target_property(TYPE ${custom_lib} TYPE)
                if (TYPE STREQUAL "SHARED_LIBRARY")
                    install(TARGETS ${custom_lib} DESTINATION "${CMAKE_INSTALL_LIBDIR}/hyperhdr" COMPONENT "HyperHDR")
                endif()
            endif()
        endforeach()

        # Pakowanie opcjonalnych zależności
        if (ENABLE_DEPENDENCY_PACKAGING)
            set(PREREQUISITE_LIBS "")

            # OpenSSL
            find_package(OpenSSL)
            if(OPENSSL_FOUND)
                foreach(openssl_lib ${OPENSSL_LIBRARIES})
                    include(GetPrerequisites)
                    gp_append_unique(PREREQUISITE_LIBS ${openssl_lib})
                    get_filename_component(file_canonical ${openssl_lib} REALPATH)
                    gp_append_unique(PREREQUISITE_LIBS ${file_canonical})

                    get_filename_component(rootLib ${openssl_lib} NAME)
                    get_filename_component(rootPath ${openssl_lib} DIRECTORY)
                    file(GLOB resultSSL "${rootPath}/${rootLib}*")
                    foreach(targetlib ${resultSSL})
                        gp_append_unique(PREREQUISITE_LIBS ${targetlib})
                    endforeach()
                endforeach()
            else()
                message( WARNING "OpenSSL NOT found (https instance will not work)")
            endif()

            # Detect the Qt5 plugin directory
            if ( Qt5Core_FOUND )            
                get_target_property(QT_QMAKE_EXECUTABLE ${Qt5Core_QMAKE_EXECUTABLE} IMPORTED_LOCATION)
                execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS OUTPUT_VARIABLE QT_PLUGINS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)        
            elseif ( TARGET Qt${QT_VERSION_MAJOR}::qmake )
                get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
                execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS OUTPUT_VARIABLE QT_PLUGINS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
            endif()

            # CEC lib
            if (ENABLE_CEC)
                find_library(XRANDR_LIBRARY NAMES Xrandr libXrandr libXrandr.so.2)
                if (XRANDR_LIBRARY)
                    get_filename_component(resolvedXrandr ${XRANDR_LIBRARY} ABSOLUTE)
                    list (APPEND cecFiles ${resolvedXrandr})
                endif()

                foreach(resolved_file_in ${CEC_LIBRARIES})
                    unset(LIBCEC CACHE)
                    find_library(LIBCEC NAMES ${resolved_file_in})
                    if (LIBCEC)
                        get_filename_component(resolvedCec ${LIBCEC} ABSOLUTE)
                        list (APPEND cecFiles ${resolvedCec})
                    endif()
                endforeach()            

                foreach(cecFile ${cecFiles})
                    FILE(GLOB foundCec "${cecFile}*")
                    foreach(installCec ${foundCec})
                        include(GetPrerequisites)
                        gp_append_unique(PREREQUISITE_LIBS ${installCec})
                    endforeach()                
                endforeach()
            endif()
                    
            # Install CODE dla zależności
            install(CODE "set(TARGET_FILE \"${TARGET_FILE}\")" COMPONENT "HyperHDR")
            install(CODE "set(PREREQUISITE_LIBS \"${PREREQUISITE_LIBS}\")" COMPONENT "HyperHDR")
            install(CODE "set(QT_PLUGINS_DIR \"${QT_PLUGINS_DIR}\")" COMPONENT "HyperHDR")
            install(CODE "set(DEST_DIR \"${CMAKE_INSTALL_LIBDIR}/hyperhdr/external\")" COMPONENT "HyperHDR")

            install(CODE [[
                set(SYSTEM_LIBS_SKIP
                    "libc" "libglib-2" "libsystemd0" "libdl" "libexpat" "libfontconfig" "libgcc_s"
                    "libm" "libpthread" "librt" "libstdc++" "libudev" "libz.so" "libxrender1"
                    "libxi6" "libxext6" "libx11-xcb1" "libsm" "libice6" "libdrm2" "libxkbcommon0"
                    "libwacom2" "libmtdev1" "libinput10" "libgudev-1.0-0" "libffi6" "libevdev2"
                    "libuuid1" "libselinux1" "libmount1" "libblkid1" "libwayland" "libxcb-icccm4"
                    "libxcb-image0" "libxcb-keysyms1" "libxcb-randr0" "libxcb-render-util0"
                    "libxcb-render0" "libxcb-shape0" "libxcb-shm0" "libxcb-sync1" "libxcb-util0"
                    "libxcb-xfixes0" "libxcb-xkb1" "libxkbcommon-x11-0" "ld-" "libasound"
                    "libblkid" "libffi" "libgio-2" "libgmodule-2" "libgobject-2" "libidn2"
                    "libnghttp" "libsystemd" "libpsl" "libunistring" "libssh" "libselinux"
                    "libevent-2" "libldap" "libutils" "libsqlite3" "libqmqtt"
                )

                include(GetPrerequisites)        
                if (NOT CMAKE_CROSSCOMPILING)
                    file(GET_RUNTIME_DEPENDENCIES RESOLVED_DEPENDENCIES_VAR DEPENDENCIES EXECUTABLES ${TARGET_FILE})

                    file(GET_RUNTIME_DEPENDENCIES RESOLVED_DEPENDENCIES_VAR SYS_DEPENDENCIES EXECUTABLES $<TARGET_FILE:systray-widget>)
                    foreach(systrayLib ${SYS_DEPENDENCIES})
                        string(FIND ${systrayLib} "libayatana" _sysindex)
                        string(FIND ${systrayLib} "libdbusmenu" _sysDBusindex)
                        if (${_sysindex} GREATER -1 OR ${_sysDBusindex} GREATER -1)
                            list(APPEND DEPENDENCIES ${systrayLib})
                        endif()
                    endforeach()                        
                endif()

                # Kopiowanie pluginów QT do lib/hyperhdr/external/plugins
                foreach(PLUGIN "tls")
                    if(EXISTS ${QT_PLUGINS_DIR}/${PLUGIN})
                        file(GLOB files "${QT_PLUGINS_DIR}/${PLUGIN}/*openssl*")
                        foreach(file ${files})
                            file(GET_RUNTIME_DEPENDENCIES RESOLVED_DEPENDENCIES_VAR QT_DEPENDENCIES EXECUTABLES ${file})
                            list(APPEND DEPENDENCIES ${QT_DEPENDENCIES})
                            file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/${DEST_DIR}/plugins/${PLUGIN}" TYPE SHARED_LIBRARY FILES ${file})
                            message("Installing QT plugin: ${file} to ${CMAKE_INSTALL_PREFIX}/${DEST_DIR}/plugins/${PLUGIN}")
                        endforeach()
                    endif()
                endforeach()

                foreach(DEPENDENCY ${DEPENDENCIES})
                    get_filename_component(resolved ${DEPENDENCY} NAME_WE)
                    
                    set(SKIP_LIB FALSE)
                    foreach(myitem ${SYSTEM_LIBS_SKIP})
                        string(FIND ${resolved} ${myitem} _index)
                        if (${_index} EQUAL 0)
                            set(SKIP_LIB TRUE)
                            break()                                    
                        endif()
                    endforeach()
                            
                    if (SKIP_LIB)
                        continue()
                    else()
                        gp_resolve_item("${TARGET_FILE}" "${DEPENDENCY}" "" "" resolved_file)
                        get_filename_component(resolved_file ${resolved_file} ABSOLUTE)
                        gp_append_unique(PREREQUISITE_LIBS ${resolved_file})
                        get_filename_component(file_canonical ${resolved_file} REALPATH)
                        gp_append_unique(PREREQUISITE_LIBS ${file_canonical})
                    endif()
                endforeach()        

                # Kopiowanie i ew. patchowanie RPATH z zachowaniem nowych ścieżek FHS
                foreach(PREREQUISITE_LIB IN LISTS PREREQUISITE_LIBS)
                    # 1. Instalacja pliku bezpośrednio do katalogu docelowego
                    file(INSTALL FILES "${PREREQUISITE_LIB}" DESTINATION "${CMAKE_INSTALL_PREFIX}/${DEST_DIR}" TYPE SHARED_LIBRARY)
                    message("Installing: ${PREREQUISITE_LIB} to ${CMAKE_INSTALL_PREFIX}/${DEST_DIR}")
                    # 2. Sprawdzenie, czy plik nie jest dowiązaniem i czy to libproxy / libpxbackend
                    if(NOT IS_SYMLINK "${PREREQUISITE_LIB}" AND "${PREREQUISITE_LIB}" MATCHES "libproxy|libpxbackend")
                        get_filename_component(FILENAME "${PREREQUISITE_LIB}" NAME)
                        set(INSTALLED_FILE "${CMAKE_INSTALL_PREFIX}/${DEST_DIR}/${FILENAME}")
                        message("Patching RPATH: ${INSTALLED_FILE}")
                        execute_process(COMMAND chrpath -d "${INSTALLED_FILE}" OUTPUT_VARIABLE outputResult)
                    endif()
                endforeach()     
            ]] COMPONENT "HyperHDR")
        endif()
    else()
        add_custom_command(
            TARGET ${TARGET} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" "-DTARGET_FILE=$<TARGET_FILE:${TARGET}>"
            ARGS ${CMAKE_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            VERBATIM
        )
    endif()
endmacro()
