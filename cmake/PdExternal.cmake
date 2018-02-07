#
# Puredata externals
#

include(MingwDLL)

option(PDEX_DOUBLE "Build for double precision" OFF)

if(NOT PD_WINDOWS_PROGRAM_DIR AND
    CMAKE_SYSTEM_NAME MATCHES "Windows" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
  if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/dist/pd.msw/pd")
    set(PD_WINDOWS_PROGRAM_DIR "${PROJECT_SOURCE_DIR}/dist/pd.msw/pd")
  endif()
endif()

if(NOT PD_WINDOWS_PROGRAM_DIR AND
    CMAKE_SYSTEM_NAME MATCHES "Windows" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
  set(PD_BINARY_ZIP "pd-0.48-0.msw.zip")
  set(PD_BINARY_URL "http://msp.ucsd.edu/Software/${PD_BINARY_ZIP}")
  set(PD_BINARY_DEST "${PROJECT_SOURCE_DIR}/dist/${PD_BINARY_ZIP}")

  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/dist/${PD_BINARY_ZIP}")
    message(STATUS "Downloading ${PD_BINARY_ZIP}")
    file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/dist")
    file(DOWNLOAD "${PD_BINARY_URL}" "${PD_BINARY_DEST}.part")
    file(RENAME "${PD_BINARY_DEST}.part" "${PD_BINARY_DEST}")
  endif()

  find_program(UNZIP_PROGRAM unzip)
  if (NOT UNZIP_PROGRAM)
    message(FATAL_ERROR "unzip not found, cannot extract the archive")
  endif()

  message(STATUS "Extracting ${PD_BINARY_ZIP}")
  execute_process(
    COMMAND "${UNZIP_PROGRAM}" -q -d pd.msw "${PD_BINARY_DEST}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/dist")
  set(PD_WINDOWS_PROGRAM_DIR "${PROJECT_SOURCE_DIR}/dist/pd.msw/pd")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  if(NOT PD_WINDOWS_PROGRAM_DIR)
    message(FATAL_ERROR "Please set PD_WINDOWS_PROGRAM_DIR on the Windows platform")
  endif()
  set(PD_INCLUDE_DIR "${PD_WINDOWS_PROGRAM_DIR}/src")
  set(PD_LIBRARIES "${PD_WINDOWS_PROGRAM_DIR}/bin/pd.lib")
else()
  find_path(PD_INCLUDE_DIR "m_pd.h")
  set(PD_LIBRARIES)
endif()

if(NOT PD_INCLUDE_DIR OR NOT EXISTS "${PD_INCLUDE_DIR}/m_pd.h")
  message(FATAL_ERROR "Cannot find the Puredata headers")
endif()

message(STATUS "Found Puredata headers: ${PD_INCLUDE_DIR}")

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
    PRIVATE "${PD_INCLUDE_DIR}")
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
  mingw_link_static_system_libs("${TARGET}")
  unset(_name)
endmacro()
