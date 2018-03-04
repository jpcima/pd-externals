
include(CMakeParseArguments)

set(DEKEN_PACKAGE_DESTDIR
  "${CMAKE_SOURCE_DIR}/deken-packages")

macro(add_deken_package NAME VERSION)
  set(_package_dir "${CMAKE_BINARY_DIR}/deken/${NAME}")
  set(_cmds)
  set(_deps)

  cmake_parse_arguments(_arg
    "" "" "TARGETS;FILES;DIRS" ${ARGN})

  if(NOT "${_arg_UNPARSED_ARGUMENTS}" STREQUAL "")
    message(FATAL_ERROR "invalid arguments")
  endif()

  list(APPEND _cmds
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_package_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_package_dir}")

  foreach(_target ${_arg_TARGETS})
    list(APPEND _cmds
      COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${_target}>" "${_package_dir}")
    list(APPEND _deps
      "${_target}")
  endforeach()
  foreach(_file ${_arg_FILES})
    list(APPEND _cmds
      COMMAND "${CMAKE_COMMAND}" -E copy "${_file}" "${_package_dir}")
  endforeach()
  foreach(_dir ${_arg_DIRS})
    get_filename_component(_dirname "${_dir}" NAME)
    list(APPEND _cmds
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_dir}" "${_package_dir}/${_dirname}")
    unset(_dirname)
  endforeach()

  add_custom_target("${NAME}-deken-contents"
    DEPENDS ${_deps}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ${_cmds})

  add_custom_target("${NAME}-deken-destdir"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEKEN_PACKAGE_DESTDIR}")

  add_custom_target("${NAME}"
    DEPENDS "${NAME}-deken-contents" "${NAME}-deken-destdir"
    WORKING_DIRECTORY "${DEKEN_PACKAGE_DESTDIR}"
    COMMAND deken package --version "${VERSION}" "${_package_dir}"
    USES_TERMINAL)

  unset(_package_dir)
  unset(_cmds)
  unset(_deps)
  unset(_arg_TARGETS)
  unset(_arg_FILES)
  unset(_arg_DIRS)
  unset(_arg_UNPARSED_ARGUMENTS)
endmacro()
