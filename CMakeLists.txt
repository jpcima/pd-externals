cmake_minimum_required(VERSION 3.6)

project(pd-externals VERSION 0.1 LANGUAGES C CXX)

set(deken_PACKAGE_NAME "jpc")
set(deken_PACKAGE_VERSION "${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(GNUWarnings)
include(FindPkgConfigLibrary)
include(PdExternal)

enable_gnu_warning("all")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(OpenMP)
if(OpenMP_FOUND)
  add_compile_options("${OpenMP_C_FLAGS}")  # only for compiling, not linking
endif()

include_directories(
  "${PROJECT_SOURCE_DIR}/src")

include_directories(
  "${PROJECT_SOURCE_DIR}/thirdparty/GSL/include")
include_directories(
  "${PROJECT_SOURCE_DIR}/thirdparty/jsl/include")

if(PDEX_DOUBLE)
  pkg_check_library(jpc-fftw fftw3)
else()
  pkg_check_library(jpc-fftw fftw3f)
endif()

if(NOT jpc-fftw_FOUND)
  message(STATUS "FFTW not found, some externals will not be built")
endif()

################################################################################
add_library(blepvco-common STATIC
  src/blepvco/blepvco.cc
  src/blepvco/minblep_tables.cc)
target_include_directories(blepvco-common
  PUBLIC ${PD_INCLUDE_DIRS})

add_pd_external(bleprect_tilde src/blepvco/bleprect~.cc)
target_link_libraries(bleprect_tilde blepvco-common)

add_pd_external(blepsaw_tilde src/blepvco/blepsaw~.cc)
target_link_libraries(blepsaw_tilde blepvco-common)

add_pd_external(bleptri_tilde src/blepvco/bleptri~.cc)
target_link_libraries(bleptri_tilde blepvco-common)

################################################################################
add_pd_external(tri_tilde src/jpc/tri~.cc)
add_pd_external(sincos src/jpc/sincos.cc)
add_pd_external(lfos_tilde src/jpc/lfos~.cc)

################################################################################
add_pd_external(bbd_tilde src/dafx/bbd~.cc)
add_pd_external(limit_tilde src/dafx/limit~.cc)
if(jpc-fftw_FOUND)
  add_pd_external(robot_tilde src/dafx/robot~.cc)
  target_link_libraries(robot_tilde jpc-fftw)
endif()

################################################################################
add_pd_external(delayA_tilde src/stk/delayA~.cc)
add_pd_external(nlcubic_tilde src/stk/nlcubic~.cc)

################################################################################
add_pd_external(dcremove_tilde src/swh/dcremove~.cc)

################################################################################
set(deken_PACKAGE_BASEDIR
  "${deken_PACKAGE_NAME}")
set(deken_BINARY_PACKAGE_DIR
  "${CMAKE_BINARY_DIR}/deken-package/bin/${deken_PACKAGE_BASEDIR}")
set(deken_SOURCE_PACKAGE_DIR
  "${CMAKE_BINARY_DIR}/deken-package/src/${deken_PACKAGE_BASEDIR}")
add_custom_target(deken-package-contents
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${deken_BINARY_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${deken_BINARY_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy "*${PD_EXTERNAL_SUFFIX}" "${deken_BINARY_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy help-*.pd "${deken_BINARY_PACKAGE_DIR}")
add_custom_target(deken-source-package-contents
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${deken_SOURCE_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${deken_SOURCE_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy help-*.pd "${deken_SOURCE_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy CMakeLists.txt "${deken_SOURCE_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy README.md LICENSE.md "${deken_SOURCE_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory src "${deken_SOURCE_PACKAGE_DIR}/src"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory thirdparty "${deken_SOURCE_PACKAGE_DIR}/thirdparty"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory cmake "${deken_SOURCE_PACKAGE_DIR}/thirdparty")
add_custom_target(deken-binary-package
  DEPENDS deken-package-contents
  WORKING_DIRECTORY "${deken_BINARY_PACKAGE_DIR}/.."
  COMMAND deken package --version "${deken_PACKAGE_VERSION}" "${deken_PACKAGE_BASEDIR}"
  USES_TERMINAL)
add_custom_target(deken-source-package
  DEPENDS deken-source-package-contents
  WORKING_DIRECTORY "${deken_SOURCE_PACKAGE_DIR}/.."
  COMMAND deken package --version "${deken_PACKAGE_VERSION}" "${deken_PACKAGE_BASEDIR}"
  USES_TERMINAL)
add_custom_target(deken-package
  DEPENDS deken-binary-package deken-source-package)
