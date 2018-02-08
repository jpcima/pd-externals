#
# Find libraries with pkg-config and create them as interface targets.
#
#
# Macro: PKG_CHECK_LIBRARY(TARGET ...)
#  Creates an interface target for the specified pkg-config library.
#  Arguments are identical to PKG_CHECK_MODULES, except the first one names the
#  resulting target.
#  Note: prefix your name to avoid conflicts. eg. "pkg-blas"
#

include(FindPkgConfig)

macro(pkg_check_library TARGET)
  pkg_check_modules(_pkg_${TARGET} ${ARGN})
  set(${TARGET}_FOUND "${_pkg_${TARGET}_FOUND}")
  if(NOT ${TARGET}_FOUND)
    set(${TARGET}_FOUND "${TARGET}-NOTFOUND")
  else()
    foreach(_lib ${_pkg_${TARGET}_LIBRARIES})
      if(_lib STREQUAL "${TARGET}")
        message(FATAL_ERROR "pkg_check_library: name conflict with library \"${TARGET}\"; please rename this target.")
      endif()
    endforeach()
    add_library(${TARGET} INTERFACE)
    link_directories(${_pkg_${TARGET}_LIBRARY_DIRS})
    target_include_directories(${TARGET} INTERFACE ${_pkg_${TARGET}_INCLUDE_DIRS})
    target_link_libraries(${TARGET} INTERFACE ${_pkg_${TARGET}_LIBRARIES})
  endif()
endmacro()
