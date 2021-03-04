if(TagLib_FOUND)
  return()
endif()

if(USE_SYSTEM_TAGLIB)
  find_path(TagLib_INCLUDE_DIR NAMES taglib.h PATH_SUFFIXES taglib)
  find_library(TagLib_LIBRARY tag)
  mark_as_advanced(TagLib_INCLUDE_DIR TagLib_LIBRARY)
else()
  set(TagLib_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/taglib")

  if(NOT EXISTS "${TagLib_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "TagLib cannnot be found in ${TagLib_SOURCE_DIR}.\n"

      "Run the following command to fetch TagLib and build from source "
      "or set -DUSE_SYSTEM_TAGLIB=ON to use the system library:\n"

      "git submodule update --init vendor/taglib"
    )
  endif()

  set(BUILD_BINDINGS OFF CACHE BOOL "Build TagLib C bindings")

  set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
  add_subdirectory(${TagLib_SOURCE_DIR} EXCLUDE_FROM_ALL)

  mark_as_advanced(
    # TagLib options
    BUILD_SHARED_LIBS ENABLE_STATIC_RUNTIME ENABLE_CCACHE VISIBILITY_HIDDEN
    BUILD_TESTS BUILD_EXAMPLES BUILD_BINDINGS NO_ITUNES_HACKS

    # Cached variables
    BUILD_FRAMEWORK TRACE_IN_RELEASE EXEC_INSTALL_PREFIX LIB_SUFFIX
    BIN_INSTALL_DIR LIB_INSTALL_DIR INCLUDE_INSTALL_DIR FRAMEWORK_INSTALL_DIR
  )

  # TagLib does not export its CMake targets, so we must do it ourselves.
  set(TagLib_INCLUDE_DIR "${TagLib_SOURCE_DIR}/taglib/toolkit") # path to taglib.h

  # just for a prettier find_package_handle_standard_args's default "found" message
  set(TagLib_LIBRARY "${TagLib_SOURCE_DIR}")

  target_compile_definitions(tag INTERFACE TAGLIB_STATIC)

  get_target_property(TagLib_INCLUDE_DIR_INTERFACE tag INCLUDE_DIRECTORIES)
  target_include_directories(tag INTERFACE
    ${TagLib_SOURCE_DIR}
    ${TagLib_SOURCE_DIR}/taglib
    ${TagLib_INCLUDE_DIR_INTERFACE}
  )
endif()

# Detect TagLib's version
if(EXISTS "${TagLib_INCLUDE_DIR}/taglib.h")
  file(STRINGS "${TagLib_INCLUDE_DIR}/taglib.h" TagLib_H REGEX "^#define")

  foreach(segment MAJOR MINOR PATCH)
    set(regex "^.+TAGLIB_${segment}_VERSION ([0-9]+).+$")
    string(REGEX REPLACE "${regex}" "\\1" TagLib_VERSION_${segment} "${TagLib_H}")
  endforeach()

  set(TagLib_VERSION
    "${TagLib_VERSION_MAJOR}.${TagLib_VERSION_MINOR}.${TagLib_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TagLib
  REQUIRED_VARS TagLib_LIBRARY TagLib_INCLUDE_DIR
  VERSION_VAR   TagLib_VERSION
)

if(TagLib_SOURCE_DIR)
  add_library(TagLib::TagLib ALIAS tag)
else()
  add_library(TagLib::TagLib UNKNOWN IMPORTED)
  set_target_properties(TagLib::TagLib PROPERTIES
    IMPORTED_LOCATION             "${TagLib_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${TagLib_INCLUDE_DIR}"
  )
endif()
