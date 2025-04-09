# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindTurboJPEG
--------

Find the Joint Photographic Experts Group (TurboJPEG) library (``libjpeg``)

Imported targets
^^^^^^^^^^^^^^^^

.. versionadded:: 3.12

This module defines the following :prop_tgt:`IMPORTED` targets:

``TurboJPEG::TurboJPEG``
  The JPEG library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``TurboJPEG_FOUND``
  If false, do not try to use JPEG.
``TurboJPEG_INCLUDE_DIRS``
  where to find jpeglib.h, etc.
``TurboJPEG_LIBRARIES``
  the libraries needed to use JPEG.
``TurboJPEG_VERSION``
  .. versionadded:: 3.12
    the version of the JPEG library found

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``TurboJPEG_INCLUDE_DIRS``
  where to find jpeglib.h, etc.
``TurboJPEG_LIBRARY_RELEASE``
  where to find the JPEG library (optimized).
``TurboJPEG_LIBRARY_DEBUG``
  where to find the JPEG library (debug).

.. versionadded:: 3.12
  Debug and Release variand are found separately.

Obsolete variables
^^^^^^^^^^^^^^^^^^

``TurboJPEG_INCLUDE_DIR``
  where to find jpeglib.h, etc. (same as TurboJPEG_INCLUDE_DIRS)
``TurboJPEG_LIBRARY``
  where to find the JPEG library.
#]=======================================================================]

if (NOT WIN32)
  find_path(TurboJPEG_INCLUDE_DIR NAMES turbojpeg.h)
else()
  cmake_host_system_information(RESULT libRegPaths QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/WOW6432Node" SUBKEYS)
  foreach(regPath ${libRegPaths})
    string(FIND ${regPath} "libjpeg-turbo" have_turbo)
    if (${have_turbo} GREATER -1)
      message("Found reg: ${regPath}")
      set(lib_turbo_registry ${regPath})
      break()
    endif()
  endforeach()
  find_path(TurboJPEG_INCLUDE_DIR NAMES turbojpeg.h PATHS "[HKLM/SOFTWARE/WOW6432Node/${lib_turbo_registry};Install_Dir]/include" "c:/libjpeg-turbo/include" "c:/libjpeg-turbo64/include")
  find_file(TurboJPEG_INSTALL_LIB NAMES turbojpeg.dll PATHS "[HKLM/SOFTWARE/WOW6432Node/${lib_turbo_registry};Install_Dir]/bin" "c:/libjpeg-turbo/bin" "c:/libjpeg-turbo64/bin" NO_DEFAULT_PATH)
  message("Install lib: ${TurboJPEG_INSTALL_LIB}")  
endif()

set(TurboJPEG_names ${TurboJPEG_NAMES} turbojpeg turbojpeg-static)
foreach(name ${TurboJPEG_names})
  list(APPEND TurboJPEG_names_debug "${name}d")
endforeach()

if(NOT TurboJPEG_LIBRARY)
  if (NOT WIN32)
    find_library(TurboJPEG_LIBRARY_RELEASE NAMES ${TurboJPEG_names} NAMES_PER_DIR)
    find_library(TurboJPEG_LIBRARY_DEBUG NAMES ${TurboJPEG_names_debug} NAMES_PER_DIR)
  else()
    find_library(TurboJPEG_LIBRARY_RELEASE PATHS "[HKLM/SOFTWARE/WOW6432Node/${lib_turbo_registry};Install_Dir]/lib/cmake" "c:/libjpeg-turbo/lib" "c:/libjpeg-turbo64/lib" NAMES ${TurboJPEG_names} NAMES_PER_DIR)
    find_library(TurboJPEG_LIBRARY_DEBUG PATHS "[HKLM/SOFTWARE/WOW6432Node/${lib_turbo_registry};Install_Dir]/lib/cmake" "c:/libjpeg-turbo/lib" "c:/libjpeg-turbo64/lib" NAMES ${TurboJPEG_names_debug} NAMES_PER_DIR)
  endif()
  include(SelectLibraryConfigurations)
  select_library_configurations(TurboJPEG)
  mark_as_advanced(TurboJPEG_LIBRARY_RELEASE TurboJPEG_LIBRARY_DEBUG)
endif()
unset(TurboJPEG_names)
unset(TurboJPEG_names_debug)

if(TurboJPEG_INCLUDE_DIR)
  file(GLOB _TurboJPEG_CONFIG_HEADERS_FEDORA "${TurboJPEG_INCLUDE_DIR}/jconfig*.h")
  file(GLOB _TurboJPEG_CONFIG_HEADERS_DEBIAN "${TurboJPEG_INCLUDE_DIR}/*/jconfig.h")
  set(_TurboJPEG_CONFIG_HEADERS
    "${TurboJPEG_INCLUDE_DIR}/jpeglib.h"
    ${_TurboJPEG_CONFIG_HEADERS_FEDORA}
    ${_TurboJPEG_CONFIG_HEADERS_DEBIAN})
  foreach (_TurboJPEG_CONFIG_HEADER IN LISTS _TurboJPEG_CONFIG_HEADERS)
    if (NOT EXISTS "${_TurboJPEG_CONFIG_HEADER}")
      continue ()
    endif ()
    file(STRINGS "${_TurboJPEG_CONFIG_HEADER}"
      TurboJPEG_lib_version REGEX "^#define[\t ]+TurboJPEG_LIB_VERSION[\t ]+.*")

    if (NOT TurboJPEG_lib_version)
      continue ()
    endif ()

    string(REGEX REPLACE "^#define[\t ]+TurboJPEG_LIB_VERSION[\t ]+([0-9]+).*"
      "\\1" TurboJPEG_VERSION "${TurboJPEG_lib_version}")
    break ()
  endforeach ()
  unset(TurboJPEG_lib_version)
  unset(_TurboJPEG_CONFIG_HEADER)
  unset(_TurboJPEG_CONFIG_HEADERS)
  unset(_TurboJPEG_CONFIG_HEADERS_FEDORA)
  unset(_TurboJPEG_CONFIG_HEADERS_DEBIAN)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TurboJPEG
  REQUIRED_VARS TurboJPEG_LIBRARY TurboJPEG_INCLUDE_DIR
  VERSION_VAR TurboJPEG_VERSION)

if(TurboJPEG_FOUND)
  if(NOT TurboJPEG_INSTALL_LIB)
    set(TurboJPEG_INSTALL_LIB ${TurboJPEG_LIBRARY})
  endif()
  set(TurboJPEG_LIBRARIES ${TurboJPEG_LIBRARY})
  set(TurboJPEG_INCLUDE_DIRS "${TurboJPEG_INCLUDE_DIR}")

  if(NOT TARGET TurboJPEG::TurboJPEG)
    add_library(TurboJPEG::TurboJPEG UNKNOWN IMPORTED)
    if(TurboJPEG_INCLUDE_DIRS)
      set_target_properties(TurboJPEG::TurboJPEG PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${TurboJPEG_INCLUDE_DIRS}")
    endif()
    if(EXISTS "${TurboJPEG_LIBRARY}")
      set_target_properties(TurboJPEG::TurboJPEG PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${TurboJPEG_LIBRARY}")
    endif()
    if(EXISTS "${TurboJPEG_LIBRARY_RELEASE}")
      set_property(TARGET TurboJPEG::TurboJPEG APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(TurboJPEG::TurboJPEG PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
        IMPORTED_LOCATION_RELEASE "${TurboJPEG_LIBRARY_RELEASE}")
    endif()
    if(EXISTS "${TurboJPEG_LIBRARY_DEBUG}")
      set_property(TARGET TurboJPEG::TurboJPEG APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(TurboJPEG::TurboJPEG PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
        IMPORTED_LOCATION_DEBUG "${TurboJPEG_LIBRARY_DEBUG}")
    endif()
  endif()
endif()

mark_as_advanced(TurboJPEG_LIBRARY TurboJPEG_INCLUDE_DIR)
