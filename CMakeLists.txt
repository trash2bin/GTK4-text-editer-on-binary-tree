cmake_minimum_required(VERSION 3.16)
project(BinaryTreeEditor LANGUAGES CXX)

# явный стандарт и отключение расширений
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# полезные переменные по умолчанию
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type specified, defaulting to Release")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

option(BUILD_TESTS "Build tests" ON)
option(ENABLE_SANITIZERS "Enable sanitizers in Debug builds" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
