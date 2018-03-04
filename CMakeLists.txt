cmake_minimum_required(VERSION 3.6)

project(pd-externals VERSION 0.1 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(GNUWarnings)
include(FindPkgConfigLibrary)
include(PdExternal)
include(PdDeken)

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
add_deken_package(jpcex "${PROJECT_VERSION}"
  TARGETS
    bleprect_tilde blepsaw_tilde bleptri_tilde
    tri_tilde sincos lfos_tilde
    bbd_tilde limit_tilde robot_tilde
    delayA_tilde nlcubic_tilde
    dcremove_tilde
  FILES
    help-*.pd CMakeLists.txt README.md LICENSE.md
  DIRS
    src thirdparty cmake)
