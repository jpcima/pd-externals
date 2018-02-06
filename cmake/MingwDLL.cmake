#
# Mingw DLL utilities
#

macro(mingw_link_static_system_libs TARGET)
  if(CMAKE_SYSTEM_NAME MATCHES Windows AND
      (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang))
      set_property(TARGET "${TARGET}" APPEND_STRING PROPERTY
        LINK_FLAGS " -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,-Bdynamic,--no-whole-archive")
  endif()
endmacro()
