MACRO (CHECK_GLIBC_VERSION)
    EXECUTE_PROCESS (
        COMMAND ldd --version
	OUTPUT_VARIABLE GLIBC
	OUTPUT_STRIP_TRAILING_WHITESPACE)

	if ("${GLIBC}" MATCHES "^.+ ([0-9]+\\.[0-9]+)\n.+")
		set (GLIBC_VERSION ${CMAKE_MATCH_1})
	else()
		message (WARNING "Unknown glibc version: ${GLIBC_VERSION}")
	endif()
ENDMACRO (CHECK_GLIBC_VERSION)

# default packages to build
if(BUILD_ARCHIVES)
	message("Building archives")
	IF (APPLE)
		SET ( CPACK_GENERATOR "TGZ")
	ELSEIF (UNIX)
		SET ( CPACK_GENERATOR "TGZ")
	ELSEIF (WIN32)
		SET ( CPACK_GENERATOR "ZIP" "NSIS")
	ENDIF()
ELSE()
	message("Skipping archives")
	IF (WIN32)
		SET ( CPACK_GENERATOR "NSIS")
	ENDIF()
ENDIF()

# Determine packages by found generator executables

# Github Action enables it for packages
find_package(RpmBuilder)
IF(RPM_BUILDER_FOUND)
	message("CPACK: Found RPM builder")
	SET ( CPACK_GENERATOR ${CPACK_GENERATOR} "RPM")
ENDIF()

find_package(DebBuilder)
IF(DEB_BUILDER_FOUND)
	message("CPACK: Found DEB builder")
	SET ( CPACK_GENERATOR ${CPACK_GENERATOR} "DEB")
ENDIF()

# Overwrite CMAKE_SYSTEM_NAME for mac (visual)
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    if(${CMAKE_HOST_APPLE})
        set(CMAKE_SYSTEM_NAME "macOS")
    endif()
endif()

# Apply to all packages, some of these can be overwritten with generator specific content
# https://cmake.org/cmake/help/v3.5/module/CPack.html

SET ( CPACK_PACKAGE_NAME "HyperHDR" )
SET ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "HyperHDR is an open source ambient light implementation" )
SET ( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md" )
SET ( CPACK_PACKAGE_FILE_NAME "HyperHDR-${HYPERHDR_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

SET ( CPACK_PACKAGE_CONTACT "see_me_at@hyperhdr.eu")
SET ( CPACK_PACKAGE_VENDOR "HyperHDR")
SET ( CPACK_PACKAGE_EXECUTABLES "hyperhdr;HyperHDR" )
SET ( CPACK_PACKAGE_INSTALL_DIRECTORY "HyperHDR" )
SET ( CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/cmake/linux/resources/hyperhdr-icon-32px.png")

SET ( CPACK_PACKAGE_VERSION_MAJOR "${HYPERHDR_VERSION_MAJOR}")
SET ( CPACK_PACKAGE_VERSION_MINOR "${HYPERHDR_VERSION_MINOR}")
SET ( CPACK_PACKAGE_VERSION_PATCH "${HYPERHDR_VERSION_PATCH}")

if(USE_STANDARD_INSTALLER_NAME AND UNIX AND NOT APPLE)
	string(REPLACE "." ";" HYPERHDR_VERSION_LIST ${HYPERHDR_VERSION})
	list(LENGTH HYPERHDR_VERSION_LIST HYPERHDR_VERSION_LIST_LEN)
	if (HYPERHDR_VERSION_LIST_LEN EQUAL 4)
		list(GET HYPERHDR_VERSION_LIST 2 H_ELEM_2)
		list(GET HYPERHDR_VERSION_LIST 3 H_ELEM_3)
		string(REPLACE "alpha" "~${DEBIAN_NAME_TAG}~alpha" H_ELEM_3a "${H_ELEM_3}")
		string(REPLACE "beta" "~${DEBIAN_NAME_TAG}~beta" H_ELEM_3b "${H_ELEM_3a}")
		string(CONCAT CPACK_PACKAGE_VERSION_PATCH "${H_ELEM_2}" "." "${H_ELEM_3b}")
		string(FIND "${CPACK_PACKAGE_VERSION_PATCH}" "${DEBIAN_NAME_TAG}" foundTag)
		if (foundTag EQUAL -1)
			string(CONCAT CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH}" "~${DEBIAN_NAME_TAG}")
		endif()
		SET ( CPACK_PACKAGE_FILE_NAME "HyperHDR-${HYPERHDR_VERSION_MAJOR}.${HYPERHDR_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-${CMAKE_SYSTEM_PROCESSOR}")
		message("Package name: ${CPACK_PACKAGE_FILE_NAME}" )
	endif()	
endif()

if ( APPLE )
	SET ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osx/LICENSE" )
	SET ( CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osx/background.png" )
	SET ( CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osx/autorun.scpt" )
ELSE()
	SET ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" )
ENDIF()

SET ( CPACK_PACKAGE_EXECUTABLES "hyperhdr;HyperHDR" )
SET ( CPACK_CREATE_DESKTOP_LINKS "hyperhdr;HyperHDR" )
SET ( CPACK_ARCHIVE_THREADS 0 )

# Define the install prefix path for cpack
IF ( UNIX )
	#SET ( CPACK_PACKAGING_INSTALL_PREFIX "share/hyperhdr")
ENDIF()

# Specific CPack Package Generators
# https://cmake.org/Wiki/CMake:CPackPackageGenerators
# .deb files for apt
SET ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/debian/preinst;${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/debian/prerm" )
SET ( CPACK_DEBIAN_PACKAGE_DEPENDS "xz-utils, libglib2.0-0" )

SET ( CPACK_DEBIAN_PACKAGE_SUGGESTS "libx11-6" )
if ( ENABLE_SYSTRAY )
	string(CONCAT CPACK_DEBIAN_PACKAGE_SUGGESTS "${CPACK_DEBIAN_PACKAGE_SUGGESTS}, libgtk-3-0" )	
endif()

SET ( CPACK_DEBIAN_PACKAGE_SECTION "Miscellaneous" )

# .rpm for rpm
SET ( CPACK_RPM_PACKAGE_RELEASE 1)
SET ( CPACK_RPM_PACKAGE_LICENSE "MIT")
SET ( CPACK_RPM_PACKAGE_GROUP "Applications")
SET ( CPACK_RPM_PACKAGE_REQUIRES "xz" )
SET ( CPACK_RPM_PACKAGE_AUTOREQPROV 0 )
SET ( CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/rpm/preinst" )
SET ( CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/rpm/postinst" )
SET ( CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/rpm/prerm" )
SET ( CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none" )
if ( ENABLE_SYSTRAY )
	SET( CPACK_RPM_PACKAGE_SUGGESTS "gtk3")
endif()

# glibc
if ( UNIX AND NOT APPLE )
	CHECK_GLIBC_VERSION()
	if ( GLIBC_VERSION )
		MESSAGE ("Glibc version: ${GLIBC_VERSION}")
		string(CONCAT CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libc6 (>=${GLIBC_VERSION})" )
		string(CONCAT CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES}, glibc >= ${GLIBC_VERSION}" )
	endif()

	message("DEB deps: ${CPACK_DEBIAN_PACKAGE_DEPENDS}")
	message("RPM deps: ${CPACK_RPM_PACKAGE_REQUIRES}")
endif()

#if(BUILD_ARCHIVES)
	SET ( CPACK_DEBIAN_COMPRESSION_TYPE "xz" )
	SET ( CPACK_RPM_COMPRESSION_TYPE "xz" )
#endif()

# OSX dmg generator
if ( APPLE )
	SET ( CPACK_GENERATOR "DragNDrop")
	SET ( CPACK_DMG_FORMAT "UDBZ" )
	
	unset(CPACK_PACKAGE_ICON)
	set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osx/Hyperhdr.icns")
	
	set_target_properties(hyperhdr PROPERTIES
	  MACOSX_BUNDLE TRUE
	  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/osx/Info.plist
	  XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
	  XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
	  MACOSX_BUNDLE_COPYRIGHT "awawa-dev"
	  MACOSX_BUNDLE_BUNDLE_VERSION ${HYPERHDR_VERSION}
	  MACOSX_BUNDLE_ICON_FILE "Hyperhdr.icns"
	)
endif()

# Windows NSIS
# Use custom script based on cpack nsis template
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/windows/template ${CMAKE_MODULE_PATH})
# Some path transformations
if(WIN32)
	file(TO_NATIVE_PATH ${CPACK_PACKAGE_ICON} CPACK_PACKAGE_ICON)
	STRING(REGEX REPLACE "\\\\" "\\\\\\\\" CPACK_PACKAGE_ICON ${CPACK_PACKAGE_ICON})
endif()
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows/installer.ico" NSIS_HYP_ICO)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows/hyperhdr-logo.bmp" NSIS_HYP_LOGO_HORI)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows/hyperhdr-logo-vert.bmp" NSIS_HYP_LOGO_VERT)
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" NSIS_HYP_ICO "${NSIS_HYP_ICO}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" NSIS_HYP_LOGO_VERT "${NSIS_HYP_LOGO_VERT}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" NSIS_HYP_LOGO_HORI "${NSIS_HYP_LOGO_HORI}")

SET ( CPACK_NSIS_MODIFY_PATH ON )
if(NOT BUILD_ARCHIVES)
	SET ( CPACK_NSIS_COMPRESSOR "" )
else()
	SET ( CPACK_NSIS_COMPRESSOR "/SOLID lzma" )
endif()
SET ( CPACK_NSIS_MUI_ICON ${NSIS_HYP_ICO})
SET ( CPACK_NSIS_MUI_UNIICON ${NSIS_HYP_ICO})
SET ( CPACK_NSIS_MUI_HEADERIMAGE ${NSIS_HYP_LOGO_HORI} )
SET ( CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP ${NSIS_HYP_LOGO_VERT})
SET ( CPACK_NSIS_DISPLAY_NAME "HyperHDR Ambient Light")
SET ( CPACK_NSIS_PACKAGE_NAME "HyperHDR" )
SET ( CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\hyperhdr.exe")
SET ( CPACK_NSIS_HELP_LINK "http://www.hyperhdr.eu/")
SET ( CPACK_NSIS_URL_INFO_ABOUT "https://github.com/awawa-dev/HyperHDR")
SET ( CPACK_NSIS_MUI_FINISHPAGE_RUN "hyperhdr.exe")
SET ( CPACK_NSIS_BRANDING_TEXT "HyperHDR-${HYPERHDR_VERSION}")
# custom nsis plugin directory
SET ( CPACK_NSIS_EXTRA_DEFS "!addplugindir ${CMAKE_SOURCE_DIR}/cmake/windows/plugins")
# additional hyperhdr startmenu link, won't be created if the user disables startmenu links
SET ( CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\HyperHDR (Console).lnk' '$INSTDIR\\\\bin\\\\hyperhdr.exe' '-d -c'")
SET ( CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$MUI_TEMP\\\\HyperHDR (Console).lnk'")




# define the install components
# See also https://gitlab.kitware.com/cmake/community/-/wikis/doc/cpack/Component-Install-With-CPack
# and https://cmake.org/cmake/help/latest/module/CPackComponent.html
SET ( CPACK_COMPONENTS_GROUPING "ALL_COMPONENTS_IN_ONE")
# Components base
SET ( CPACK_COMPONENTS_ALL "HyperHDR" )

SET ( CPACK_ARCHIVE_COMPONENT_INSTALL ON )
SET ( CPACK_DEB_COMPONENT_INSTALL ON )
SET ( CPACK_RPM_COMPONENT_INSTALL ON )

SET ( CPACK_STRIP_FILES ON )

# no code after following line!
INCLUDE ( CPack )

cpack_add_install_type(Full DISPLAY_NAME "Full")
cpack_add_install_type(Min DISPLAY_NAME "Minimal")
cpack_add_component_group(Runtime EXPANDED DESCRIPTION "HyperHdr runtime")

# Components base
cpack_add_component(HyperHDR
	DISPLAY_NAME "HyperHDR"
	DESCRIPTION "HyperHDR runtime"
	INSTALL_TYPES Full Min
	GROUP Runtime
	REQUIRED
)

