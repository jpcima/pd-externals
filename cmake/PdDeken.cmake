
include(CMakeParseArguments)

set(DEKEN_PACKAGE_DESTDIR
  "${CMAKE_SOURCE_DIR}/deken-packages")

macro(add_deken_package NAME VERSION)
  set(_package_dir "${CMAKE_BINARY_DIR}/deken/${NAME}")
  set(_cmds)
  set(_deps)

  cmake_parse_arguments(_arg
    "" "" "TARGETS;FILES;DIRS;EXCLUDE" ${ARGN})

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
    file(GLOB_RECURSE _alldirfiles
      LIST_DIRECTORIES FALSE RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
      "${_dir}/*")
    #
    set(_dirfiles)
    foreach(_dirfile ${_alldirfiles})
      set(_excluded FALSE)
      foreach(_exclregex ${_arg_EXCLUDE})
        if(_dirfile MATCHES "${_exclregex}")
          set(_excluded TRUE)
        endif()
      endforeach()
      if(NOT _excluded)
        list(APPEND _dirfiles "${_dirfile}")
      endif()
      unset(_excluded)
    endforeach()
    unset(_alldirfiles)
    #
    foreach(_dirfile ${_dirfiles})
      get_filename_component(_filedir "${_dirfile}" DIRECTORY)
      list(APPEND _cmds
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_package_dir}/${_filedir}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${_dirfile}" "${_package_dir}/${_filedir}/")
      unset(_filedir)
    endforeach()
    unset(_dirfiles)
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
  unset(_arg_EXCLUDE)
  unset(_arg_UNPARSED_ARGUMENTS)
endmacro()
