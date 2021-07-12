cmake_minimum_required(VERSION 3.13)
project(sws LANGUAGES C CXX)

option(BUILD_SWS_PYTHON  "Generate sws_python(32|64).py (requires Perl)" ON)
option(USE_SYSTEM_TAGLIB "Link against the system-provided TagLib"       OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(WIN32)
    set(USER_CONFIG_DIR "$ENV{APPDATA}")
  elseif(APPLE)
    set(USER_CONFIG_DIR "~/Library/Application Support")
  else()
    set(USER_CONFIG_DIR "~/.config")
  endif()

  set(CMAKE_INSTALL_PREFIX "${USER_CONFIG_DIR}/REAPER" CACHE PATH
    "REAPER resource path where to install SWS" FORCE)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL "8")
  set(CPUBITS "64")
  set(REASCRIPT_PYTHON_FLAGS "-x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL "4")
  set(CPUBITS "32")
else()
  message(FATAL_ERROR "Unknown architecture where sizeof(void*) == ${CMAKE_SIZEOF_VOID_P}")
endif()

if(CMAKE_OSX_ARCHITECTURES)
  list(JOIN CMAKE_OSX_ARCHITECTURES "-" ARCH_NAME)
elseif(MSVC_CXX_ARCHITECTURE_ID)
  set(ARCH_NAME ${MSVC_CXX_ARCHITECTURE_ID})
else()
  set(ARCH_NAME ${CMAKE_SYSTEM_PROCESSOR})
endif()

STRING(TOLOWER "${ARCH_NAME}" ARCH_NAME)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/reaper_plugin_functions.h")
  message(FATAL_ERROR
    "reaper_plugin_functions.h cannot be found in "
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor.\n"

    "Generate it using the following REAPER action: "
    "[developer] Write C++ API functions header."
  )
endif()

set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME sws) # avoid an empty "Unspecified" component
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE        ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL     ON)

if(MSVC)
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)

  foreach(arg
    CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL
  )
    # Use the static runtime library
    string(REPLACE "/MD" "/MT" ${arg} "${${arg}}")

    # Embed debug information in the object files
    string(REGEX REPLACE "/Z[iI]" "/Z7" ${arg} "${${arg}}")
  endforeach()

  # Global compiler flags (all targets)
  add_compile_options(
    # Buffer security check
    /GS

    # Enhance optimized debugging
    $<$<NOT:$<CONFIG:Debug>>:/Zo>

    # Eliminate duplicate strings
    $<$<NOT:$<CONFIG:Debug>>:/GF>

    # Enable function-level linking
    $<$<NOT:$<CONFIG:Debug>>:/Gy>

    # Remove unreferenced COMDAT
    $<$<NOT:$<CONFIG:Debug>>:/Zc:inline>

    # Disable thread-safe initialization of local static variables for XP compatibility
    # https://web.archive.org/web/20151123133638/https://connect.microsoft.com/VisualStudio/feedback/details/1789709/visual-c-2015-runtime-broken-on-windows-server-2003-c-11-magic-statics
    /Zc:threadSafeInit-
  )
else()
  add_compile_options(-fstack-protector-strong -fdiagnostics-color)
endif()

find_package(PHP REQUIRED)
add_custom_command(
  OUTPUT  reascript_vararg.h
  COMMAND ${PHP_EXECUTABLE} reascript_vararg.php
    > ${CMAKE_CURRENT_BINARY_DIR}/reascript_vararg.h
  DEPENDS ReaScript.cpp reascript_vararg.php
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

if(BUILD_SWS_PYTHON)
  find_package(Perl)

  if(NOT Perl_FOUND)
    message(FATAL_ERROR
      "Perl is required for generating sws_python${CPUBITS}.py.\n"

      "You may disable this by passing -DBUILD_SWS_PYTHON=NO to cmake."
    )
  endif()

  set(REASCRIPT_PYTHON_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sws_python${CPUBITS}.py)
  add_custom_command(
    OUTPUT  ${REASCRIPT_PYTHON_OUTPUT}
    COMMAND ${PERL_EXECUTABLE} reascript_python.pl ${REASCRIPT_PYTHON_FLAGS}
      > ${REASCRIPT_PYTHON_OUTPUT}
    DEPENDS ReaScript.cpp reascript_python.pl reascript_helper.pl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  add_custom_target(sws_python ALL DEPENDS ${REASCRIPT_PYTHON_OUTPUT})
endif()

add_library(sws SHARED
  libebur128/ebur128.cpp
  Menus.cpp
  Prompt.cpp
  reaper/reaper.cpp
  ReaScript.cpp
  reascript_vararg.h # dynamically generated
  sws_about.cpp
  sws_extension.cpp
  sws_extension.rc
  sws_util.cpp
  sws_waitdlg.cpp
  sws_wnd.cpp
  Zoom.cpp
)

if(WIN32)
  target_sources(sws PRIVATE Utility/win32-utf8.cpp DragDrop.cpp)
else()
  target_sources(sws PRIVATE sws_util$<IF:$<BOOL:${APPLE}>,.mm,_generic.cpp>)
endif()

target_compile_features(sws PRIVATE cxx_std_11)

target_include_directories(sws PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor
  ${CMAKE_CURRENT_BINARY_DIR}
)

find_package(JNetLib REQUIRED)
find_package(LICE REQUIRED)
find_package(TagLib REQUIRED)
find_package(WDL REQUIRED)
target_link_libraries(sws JNetLib::JNetLib LICE::LICE TagLib::TagLib WDL::WDL)

if(USE_SYSTEM_TAGLIB)
  # maybe replace this with a proper install of taglib (eg. via vcpkg)?
  target_compile_definitions(sws PRIVATE USE_SYSTEM_TAGLIB)
endif()

find_package(SWELL)
if(SWELL_FOUND)
  target_link_libraries(sws SWELL::swell)

  # Transpile the Win32 resource file
  add_custom_command(
    OUTPUT  sws_extension.rc_mac_dlg sws_extension.rc_mac_menu
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_SOURCE_DIR}/sws_extension.rc ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${PHP_EXECUTABLE} ${SWELL_RESGEN} sws_extension.rc
    DEPENDS ${SWELL_RESGEN}
    MAIN_DEPENDENCY sws_extension.rc
  )
endif()

if(MSVC)
  target_compile_options(sws PRIVATE /EHsc /W3)
  target_link_libraries(sws Shlwapi)
  target_link_options(sws PRIVATE $<$<NOT:$<CONFIG:Debug>>:
    # Remove unreferenced code
    /OPT:REF

    # Remove duplicate sections
    /OPT:ICF

    # Use relative path to the PDB file to avoid exposing the full path
    /PDBALTPATH:%_PDB%
  >)

  # Use the precompiled header for all files
  set(PCH_INPUT  "stdafx.h")
  set(PCH_OUTPUT "stdafx.pch")
  target_compile_options(sws PRIVATE /Yu${PCH_INPUT} /Fp${PCH_OUTPUT})

  # ...except for these files
  set_source_files_properties(reaper/reaper.cpp PROPERTIES COMPILE_OPTIONS "/Y-")

  # Build the precompiled header
  set_source_files_properties(stdafx.cpp PROPERTIES
    COMPILE_OPTIONS "/Yc${PCH_INPUT};/Fp${PCH_OUTPUT}"
    OBJECT_OUTPUTS "${PCH_OUTPUT}"
  )
  add_library(pch OBJECT stdafx.cpp)
  target_include_directories(pch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${WDL_INCLUDE_DIR}
  )
  add_dependencies(sws pch)
  target_link_libraries(sws pch)
else()
  target_compile_options(sws PRIVATE
    -Wall
    -Wno-reorder -Wno-sign-compare -Wno-unused-function -Wno-unused-parameter

    # Ignore deprecated std::auto_ptr from Fingers and stat64 from WDL/dirscan.h
    -Wno-deprecated-declarations
  )

  # Better [s][n]printf diagnostics (GCC 7)
  check_cxx_compiler_flag(-Wformat-overflow HAVE_WFORMAT_OVERFLOW)
  if(HAVE_WFORMAT_OVERFLOW)
    target_compile_options(sws PRIVATE -Wformat-overflow=2 -Wformat-truncation=2)
  endif()

  # Ignore deprecated register keyword from WDL/MersenneTwister.h (Clang)
  check_cxx_compiler_flag(-Wdeprecated-register HAVE_WDEPRECATED_REGISTER)
  if(HAVE_WDEPRECATED_REGISTER)
    target_compile_options(sws PRIVATE -Wno-deprecated-register)
  endif()
endif()

set_target_properties(sws PROPERTIES
  PREFIX "" # disable the "lib" prefix
  OUTPUT_NAME "reaper_sws-${ARCH_NAME}"
)

set(REAPER_USER_PLUGINS "UserPlugins")
set(REAPER_SCRIPTS      "Scripts")

if(NO_INSTALL_PREFIX)
  # Used for .dmg generation to avoid getting directories
  # Maybe there's a better way?
  set(REAPER_USER_PLUGINS ".")
  set(REAPER_SCRIPTS      ".")
endif()

install(TARGETS sws
  COMPONENT sws
  RUNTIME DESTINATION "${REAPER_USER_PLUGINS}" # Windows .dll
  LIBRARY DESTINATION "${REAPER_USER_PLUGINS}" # macOS .dylib/Linux .so
)

if(BUILD_SWS_PYTHON)
  install(FILES sws_python.py "${REASCRIPT_PYTHON_OUTPUT}"
    COMPONENT   sws_python
    DESTINATION "${REAPER_SCRIPTS}"
  )
endif()

install(DIRECTORY "FingersExtras/Grooves"
  COMPONENT   grooves
  DESTINATION .
  EXCLUDE_FROM_ALL
)

add_subdirectory(Autorender)
add_subdirectory(Breeder)
add_subdirectory(cfillion)
add_subdirectory(Color)
add_subdirectory(Console)
add_subdirectory(Fingers)
add_subdirectory(Freeze)
add_subdirectory(IX)
add_subdirectory(MarkerActions)
add_subdirectory(MarkerList)
add_subdirectory(Misc)
add_subdirectory(nofish)
add_subdirectory(ObjectState)
add_subdirectory(Padre)
add_subdirectory(Projects)
add_subdirectory(Snapshots)
add_subdirectory(SnM)
add_subdirectory(snooks)
add_subdirectory(TrackList)
add_subdirectory(Utility)
add_subdirectory(Wol)
add_subdirectory(Xenakios)

# the langpack target must be included after all sources files are registered
add_subdirectory(BuildUtils)

set(SWS_VERSION_REGEX "^#define SWS_VERSION ([0-9]+),([0-9]+),([0-9]+),([0-9]+)$")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" SWS_VERSION_DEF REGEX
  "${SWS_VERSION_REGEX}")
foreach(pair "1;MAJOR" "2;MINOR" "3;PATCH" "4;TWEAK")
  list(GET pair 0 capture)
  list(GET pair 1 segment)
  string(REGEX REPLACE "${SWS_VERSION_REGEX}" "\\${capture}"
    SWS_VERSION_${segment} "${SWS_VERSION_DEF}")
endforeach()

find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(NOT "$ENV{APPVEYOR_PULL_REQUEST_HEAD_REPO_BRANCH}" STREQUAL "")
    set(GIT_BRANCH "$ENV{APPVEYOR_PULL_REQUEST_HEAD_REPO_BRANCH}")
  elseif(NOT "$ENV{APPVEYOR_REPO_BRANCH}" STREQUAL "$ENV{APPVEYOR_REPO_TAG_NAME}")
    set(GIT_BRANCH "$ENV{APPVEYOR_REPO_BRANCH}")
  elseif(NOT "$ENV{TRAVIS_PULL_REQUEST_BRANCH}" STREQUAL "")
    set(GIT_BRANCH "$ENV{TRAVIS_PULL_REQUEST_BRANCH}")
  elseif(NOT "$ENV{TRAVIS_BRANCH}" STREQUAL "$ENV{TRAVIS_TAG}")
    set(GIT_BRANCH "$ENV{TRAVIS_BRANCH}")
  else()
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
      OUTPUT_VARIABLE GIT_BRANCH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()

  if("${GIT_BRANCH}" STREQUAL "master" OR "${GIT_BRANCH}" STREQUAL "HEAD")
    unset(GIT_BRANCH)
  endif()

  # normalize the branch name to be appended to the package filename
  string(REGEX REPLACE "[^a-zA-Z0-9\\._-]" - GIT_BRANCH_FN "${GIT_BRANCH}")
  STRING(TOLOWER "${GIT_BRANCH_FN}" GIT_BRANCH_FN)
else()
  set(GIT_COMMIT "nogit")
endif()

set(CPACK_PACKAGE_NAME "sws")
list(APPEND CPACK_PACKAGE_VERSION
  ${SWS_VERSION_MAJOR} ${SWS_VERSION_MINOR} ${SWS_VERSION_PATCH}
  ${SWS_VERSION_TWEAK}
)
list(JOIN CPACK_PACKAGE_VERSION . CPACK_PACKAGE_VERSION)
list(APPEND CPACK_PACKAGE_FILE_NAME
  ${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION} ${CMAKE_SYSTEM_NAME}
  ${ARCH_NAME} ${GIT_BRANCH_FN} ${GIT_COMMIT}
)
list(JOIN CPACK_PACKAGE_FILE_NAME - CPACK_PACKAGE_FILE_NAME)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/setup/license.txt")

if(APPLE)
  set(CPACK_GENERATOR DragNDrop)
elseif(WIN32)
  set(CPACK_GENERATOR NSIS)
else()
  set(CPACK_GENERATOR TXZ)
endif()

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON) # include all components
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE) # ...into a single archive

set(CPACK_DMG_FORMAT "UDBZ") # bzip2-compressed
set(CPACK_DMG_DS_STORE_SETUP_SCRIPT
  "${CMAKE_CURRENT_SOURCE_DIR}/setup/macos/DS_Store.scpt")
set(CPACK_DMG_BACKGROUND_IMAGE
  "${CMAKE_CURRENT_SOURCE_DIR}/setup/macos/background.tiff")
set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/setup/win32") # NSIS script
set(CPACK_NSIS_PACKAGE_NAME "SWS Extension v${CPACK_PACKAGE_VERSION} ${ARCH_NAME}")
set(CPACK_NSIS_CPUBITS ${CPUBITS})

include(CPack)

configure_file(version.h.in version.h)
configure_file(version.rc2.in version.rc2)
