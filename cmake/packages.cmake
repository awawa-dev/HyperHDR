# cmake file for generating distribution packages
#SET(HYPERHDR_REPO_BUILD ON)

# - Check glibc version
# CHECK_GLIBC_VERSION()
# Source: https://gist.github.com/likema/f5c04dad837d2f5068ae7a8860c180e7#file-glibc-cmake
# 
# Once done this will define
#
#   GLIBC_VERSION - glibc version
#
MACRO (CHECK_GLIBC_VERSION)
    EXECUTE_PROCESS (
        COMMAND ${CMAKE_C_COMPILER} -print-file-name=libc.so.6
	OUTPUT_VARIABLE GLIBC
	OUTPUT_STRIP_TRAILING_WHITESPACE)

    GET_FILENAME_COMPONENT (GLIBC ${GLIBC} REALPATH)
    GET_FILENAME_COMPONENT (GLIBC_VERSION ${GLIBC} NAME)
    STRING (REPLACE "libc-" "" GLIBC_VERSION ${GLIBC_VERSION})
    STRING (REPLACE ".so" "" GLIBC_VERSION ${GLIBC_VERSION})
    if (NOT GLIBC_VERSION MATCHES "^[0-9.]+$")
		MESSAGE (WARNING "Unknown glibc version: ${GLIBC_VERSION}")
		UNSET (GLIBC_VERSION)
	endif()
ENDMACRO (CHECK_GLIBC_VERSION)

# default packages to build
if(NOT DO_NOT_BUILD_ARCHIVES)
	IF (APPLE)
		SET ( CPACK_GENERATOR "TGZ")
	ELSEIF (UNIX)
		SET ( CPACK_GENERATOR "TGZ")
	ELSEIF (WIN32)
		SET ( CPACK_GENERATOR "ZIP" "NSIS")
	ENDIF()
ELSE()
	IF (WIN32)
		SET ( CPACK_GENERATOR "NSIS")
	ENDIF()
ENDIF()

# Determine packages by found generator executables

# Github Action enables it for packages
find_package(RpmBuilder)
IF(RPM_BUILDER_FOUND)
	message(STATUS "CPACK: Found RPM builder")
	SET ( CPACK_GENERATOR "RPM")
ENDIF()

find_package(DebBuilder)
IF(DEB_BUILDER_FOUND)
	message(STATUS "CPACK: Found DEB builder")
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
SET ( CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/resources/icons/hyperhdr-icon-32px.png")

SET ( CPACK_PACKAGE_VERSION_MAJOR "${HYPERHDR_VERSION_MAJOR}")
SET ( CPACK_PACKAGE_VERSION_MINOR "${HYPERHDR_VERSION_MINOR}")
SET ( CPACK_PACKAGE_VERSION_PATCH "${HYPERHDR_VERSION_PATCH}")

# Github Action enables it for packages
if(HYPERHDR_REPO_BUILD)
	string(REPLACE "." ";" HYPERHDR_VERSION_LIST ${HYPERHDR_VERSION})
	list(LENGTH HYPERHDR_VERSION_LIST HYPERHDR_VERSION_LIST_LEN)
	if (HYPERHDR_VERSION_LIST_LEN EQUAL 4)
		list(GET HYPERHDR_VERSION_LIST 2 H_ELEM_2)
		list(GET HYPERHDR_VERSION_LIST 3 H_ELEM_3)
		string(REPLACE "alfa" "~${DEBIAN_NAME_TAG}~alfa" H_ELEM_3a "${H_ELEM_3}")
		string(REPLACE "beta" "~${DEBIAN_NAME_TAG}~beta" H_ELEM_3b "${H_ELEM_3a}")
		string(CONCAT CPACK_PACKAGE_VERSION_PATCH "${H_ELEM_2}" "." "${H_ELEM_3b}")
		string(FIND "${CPACK_PACKAGE_VERSION_PATCH}" "${DEBIAN_NAME_TAG}" foundTag)
		if (foundTag EQUAL -1)
			string(CONCAT CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH}" "~${DEBIAN_NAME_TAG}")
		endif()
		SET ( CPACK_PACKAGE_FILE_NAME "HyperHDR-${HYPERHDR_VERSION_MAJOR}.${HYPERHDR_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-${CMAKE_SYSTEM_PROCESSOR}")
		message( warning "Package name: ${CPACK_PACKAGE_FILE_NAME}" )
	endif()	
endif()

if ( APPLE )
	SET ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osxbundle/LICENSE" )
	SET ( CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osxbundle/background.png" )
	SET ( CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osxbundle/autorun.scpt" )
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
SET ( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/cmake/debian/preinst;${CMAKE_CURRENT_SOURCE_DIR}/cmake/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/cmake/debian/prerm" )
SET ( CPACK_DEBIAN_PACKAGE_DEPENDS "xz-utils, libglib2.0-0" )
SET ( CPACK_DEBIAN_PACKAGE_RECOMMENDS "shared-mime-info" )
if ( UNIX AND NOT APPLE )
	CHECK_GLIBC_VERSION()
	if ( GLIBC_VERSION )
		MESSAGE (STATUS "Glibc version: ${GLIBC_VERSION}")
		string(CONCAT CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}" ", libc6 (>=${GLIBC_VERSION})" )
	endif()
endif()
if ( ENABLE_CEC )
	string(CONCAT CPACK_DEBIAN_PACKAGE_RECOMMENDS "${CPACK_DEBIAN_PACKAGE_RECOMMENDS}" ", libp8-platform-dev" )	
endif()
SET ( CPACK_DEBIAN_PACKAGE_SUGGESTS "libx11-6" )
SET ( CPACK_DEBIAN_PACKAGE_SECTION "Miscellaneous" )
SET ( CPACK_DEBIAN_COMPRESSION_TYPE "xz" )

# .rpm for rpm
# https://cmake.org/cmake/help/v3.5/module/CPackRPM.html
SET ( CPACK_RPM_PACKAGE_RELEASE 1)
SET ( CPACK_RPM_PACKAGE_LICENSE "MIT")
SET ( CPACK_RPM_PACKAGE_GROUP "Applications")
SET ( CPACK_RPM_PACKAGE_REQUIRES "xz-utils" )
SET ( CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm/preinst" )
SET ( CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm/postinst" )
SET ( CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm/prerm" )
SET ( CPACK_RPM_COMPRESSION_TYPE "xz" )

# OSX dmg generator
if ( APPLE )
	SET ( CPACK_GENERATOR "DragNDrop")
	SET ( CPACK_DMG_FORMAT "UDBZ" )
	
	unset(CPACK_PACKAGE_ICON)
	set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osxbundle/Hyperhdr.icns")
	
	set_target_properties(hyperhdr PROPERTIES
	  MACOSX_BUNDLE TRUE
	  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/osxbundle/Info.plist
	  XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
	  XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
	  MACOSX_BUNDLE_COPYRIGHT "awawa-dev"
	  MACOSX_BUNDLE_BUNDLE_VERSION ${HYPERHDR_VERSION}
	  MACOSX_BUNDLE_ICON_FILE "Hyperhdr.icns"
	)
endif()

# Windows NSIS
# Use custom script based on cpack nsis template
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/nsis/template ${CMAKE_MODULE_PATH})
# Some path transformations
if(WIN32)
	file(TO_NATIVE_PATH ${CPACK_PACKAGE_ICON} CPACK_PACKAGE_ICON)
	STRING(REGEX REPLACE "\\\\" "\\\\\\\\" CPACK_PACKAGE_ICON ${CPACK_PACKAGE_ICON})
endif()
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nsis/installer.ico" NSIS_HYP_ICO)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nsis/hyperhdr-logo.bmp" NSIS_HYP_LOGO_HORI)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nsis/hyperhdr-logo-vert.bmp" NSIS_HYP_LOGO_VERT)
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" NSIS_HYP_ICO "${NSIS_HYP_ICO}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" NSIS_HYP_LOGO_VERT "${NSIS_HYP_LOGO_VERT}")
STRING(REGEX REPLACE "\\\\" "\\\\\\\\" NSIS_HYP_LOGO_HORI "${NSIS_HYP_LOGO_HORI}")

SET ( CPACK_NSIS_MODIFY_PATH ON )
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
SET ( CPACK_NSIS_EXTRA_DEFS "!addplugindir ${CMAKE_SOURCE_DIR}/cmake/nsis/plugins")
# additional hyperhdr startmenu link, won't be created if the user disables startmenu links
SET ( CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\HyperHDR (Console).lnk' '$INSTDIR\\\\bin\\\\hyperhdr.exe' '-d -c'")
SET ( CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$MUI_TEMP\\\\HyperHDR (Console).lnk'")




# define the install components
# See also https://gitlab.kitware.com/cmake/community/-/wikis/doc/cpack/Component-Install-With-CPack
# and https://cmake.org/cmake/help/latest/module/CPackComponent.html
SET ( CPACK_COMPONENTS_GROUPING "ALL_COMPONENTS_IN_ONE")
# Components base
if (NOT APPLE)
	SET ( CPACK_COMPONENTS_ALL "HyperHDR" "HyperHDR_remote" )
else()
	SET ( CPACK_COMPONENTS_ALL "HyperHDR" )
endif()

SET ( CPACK_ARCHIVE_COMPONENT_INSTALL ON )
SET ( CPACK_DEB_COMPONENT_INSTALL ON )
SET ( CPACK_RPM_COMPONENT_INSTALL ON )

SET ( CPACK_STRIP_FILES ON )

# no code after following line!
INCLUDE ( CPack )

cpack_add_install_type(Full DISPLAY_NAME "Full")
cpack_add_install_type(Min DISPLAY_NAME "Minimal")
cpack_add_component_group(Runtime EXPANDED DESCRIPTION "HyperHdr runtime and HyperHdr_remote commandline tool")
cpack_add_component_group(Screencapture EXPANDED DESCRIPTION "Standalone Screencapture commandline programs")
# Components base
cpack_add_component(HyperHDR
	DISPLAY_NAME "HyperHDR"
	DESCRIPTION "HyperHDR runtime"
	INSTALL_TYPES Full Min
	GROUP Runtime
	REQUIRED
)

if (NOT APPLE)
	cpack_add_component(HyperHDR_remote
		DISPLAY_NAME "HyperHdr Remote"
		DESCRIPTION "HyperHdr remote cli tool"
		INSTALL_TYPES Full
		GROUP Runtime
		DEPENDS HyperHDR
	)
endif()

