#
# Puredata externals
#

include(MingwDLL)

option(PDEX_DOUBLE "Build for double precision" OFF)

set(PD_INCLUDE_BASEDIR "${CMAKE_SOURCE_DIR}/dist/pd/include")
set(PD_IMP_DEF "${CMAKE_SOURCE_DIR}/dist/pd/pd.def")
set(PD_LIBRARIES)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  find_program(PD_DLLTOOL dlltool)
  add_custom_command(
    OUTPUT "libpd.dll.a"
    COMMAND "${PD_DLLTOOL}" -l libpd.dll.a -d "${PD_IMP_DEF}"
    DEPENDS "${PD_IMP_DEF}")
  add_custom_target(pdex-implib-generated
    DEPENDS "libpd.dll.a")
  add_library(pdex-implib STATIC IMPORTED)
  add_dependencies(pdex-implib pdex-implib-generated)
  set_target_properties(pdex-implib PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/libpd.dll.a")
  list(APPEND PD_LIBRARIES
    pdex-implib)
endif()

if(NOT PD_INCLUDE_BASEDIR OR NOT EXISTS "${PD_INCLUDE_BASEDIR}/m_pd.h")
  message(FATAL_ERROR "Cannot find the Puredata headers")
endif()

message(STATUS "Found Puredata headers: ${PD_INCLUDE_BASEDIR}")

set(PD_INCLUDE_DIRS "${PD_INCLUDE_BASEDIR}")
if(IS_DIRECTORY "${PD_INCLUDE_BASEDIR}/pd")
  list(APPEND PD_INCLUDE_DIRS "${PD_INCLUDE_BASEDIR}/pd")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(PD_EXTERNAL_SUFFIX ".pd_linux")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(PD_EXTERNAL_SUFFIX ".pd_darwin")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(PD_EXTERNAL_SUFFIX ".dll")
else()
  message(FATAL_ERROR "Unrecognized platform")
endif()

macro(add_pd_external TARGET)
  if("${TARGET}" MATCHES "_tilde$")
    string(REGEX REPLACE "_tilde$" "~" _name "${TARGET}")
  else()
    set(_name "${TARGET}")
  endif()
  add_library("${TARGET}" MODULE ${ARGN})
  target_include_directories("${TARGET}"
    PRIVATE ${PD_INCLUDE_DIRS})
  target_compile_definitions("${TARGET}"
    PRIVATE "PD")
  if(PDEX_DOUBLE)
    target_compile_definitions("${TARGET}"
      PRIVATE "PD_FLOATSIZE=64")
  endif()
  target_link_libraries("${TARGET}"
    ${PD_LIBRARIES})
  set_target_properties("${TARGET}" PROPERTIES
    OUTPUT_NAME "${_name}"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}"
    PREFIX ""
    SUFFIX "${PD_EXTERNAL_SUFFIX}")
  if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set_target_properties("${TARGET}" PROPERTIES
      LINK_FLAGS "-Wl,-undefined,suppress,-flat_namespace")
  endif()
  mingw_link_static_system_libs("${TARGET}")
  unset(_name)
endmacro()
