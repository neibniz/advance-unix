include(CheckCSourceCompiles)
check_c_source_compiles([[
  #include <features.h>
  #ifndef __GLIBC__
  #error "glibc required"
  #endif
  int main(void) { return 0; }
]] ADVANCE_UNIX_HAVE_GLIBC)

if(ADVANCE_UNIX_HAVE_GLIBC)
  add_unix_experiment(lab_dynamic_loading
    SOURCES main.c
    LIBRARIES ${CMAKE_DL_LIBS}
    LINUX_ONLY
  )
else()
  message(STATUS "Skipping glibc-specific experiment: lab_dynamic_loading")
endif()
