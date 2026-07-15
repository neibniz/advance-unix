include_guard(GLOBAL)

function(add_unix_experiment target)
  set(options LINUX_ONLY NO_TEST)
  set(one_value_args)
  set(multi_value_args SOURCES LIBRARIES)
  cmake_parse_arguments(LAB
    "${options}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGN}
  )

  if(LAB_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "add_unix_experiment(${target}): unknown arguments: ${LAB_UNPARSED_ARGUMENTS}"
    )
  endif()

  if(NOT LAB_SOURCES)
    message(FATAL_ERROR
      "add_unix_experiment(${target}) requires at least one source file"
    )
  endif()

  if(LAB_LINUX_ONLY AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Skipping Linux-only experiment: ${target}")
    return()
  endif()

  add_executable(${target} ${LAB_SOURCES})
  target_compile_features(${target} PRIVATE c_std_17)
  set_target_properties(${target} PROPERTIES
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS YES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
  )

  target_compile_definitions(${target} PRIVATE _FILE_OFFSET_BITS=64)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${target} PRIVATE _GNU_SOURCE)
  else()
    target_compile_definitions(${target} PRIVATE _POSIX_C_SOURCE=200809L)
  endif()

  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wconversion>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wshadow>
  )

  if(ADVANCE_UNIX_ENABLE_SANITIZERS AND
     CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()

  if(LAB_LIBRARIES)
    target_link_libraries(${target} PRIVATE ${LAB_LIBRARIES})
  endif()

  if(BUILD_TESTING AND NOT LAB_NO_TEST)
    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(${target} PROPERTIES
      LABELS "unix-api"
      TIMEOUT 15
    )
  endif()

  install(TARGETS ${target}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
endfunction()
