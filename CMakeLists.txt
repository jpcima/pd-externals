cmake_minimum_required(VERSION 3.6)

project(pd-externals VERSION 0.1)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(GNUWarnings)
include(PdExternal)

enable_gnu_warning("all")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories(
  "${PROJECT_SOURCE_DIR}/src")

include_directories(
  "${PROJECT_SOURCE_DIR}/thirdparty/GSL/include")
include_directories(
  "${PROJECT_SOURCE_DIR}/thirdparty/jsl/include")

################################################################################
add_library(blepvco-common STATIC
  src/blepvco/blepvco.cc
  src/blepvco/minblep_tables.cc)

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

################################################################################
add_pd_external(delayA_tilde src/stk/delayA~.cc)
add_pd_external(nlcubic_tilde src/stk/nlcubic~.cc)

################################################################################
add_pd_external(dcremove_tilde src/swh/dcremove~.cc)
