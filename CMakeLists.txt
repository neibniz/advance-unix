cmake_minimum_required(VERSION 3.25)

project(
  advance_unix
  VERSION 0.1.0
  DESCRIPTION "Small experiments for advanced Unix and Linux APIs"
  LANGUAGES C
)

include(CTest)
include(GNUInstallDirs)
find_package(Threads REQUIRED)

option(ADVANCE_UNIX_ENABLE_SANITIZERS
       "Build experiments with AddressSanitizer and UndefinedBehaviorSanitizer"
       OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AddUnixExperiment)

add_subdirectory(experiments)
