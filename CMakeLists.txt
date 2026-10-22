cmake_minimum_required(VERSION 3.20)
project(discretization VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# scikit-build-core sets SKBUILD when building the Python package.
if(SKBUILD)
  set(_default_python ON)
  set(_default_tests OFF)
else()
  set(_default_python OFF)
  set(_default_tests ON)
endif()
option(DISCRETIZATION_BUILD_TESTS "Build the C++ unit tests" ${_default_tests})
option(DISCRETIZATION_BUILD_PYTHON "Build the Python extension module" ${_default_python})

# ---- Core library ----
add_library(discretization STATIC cpp/src/discretization.cpp)
target_include_directories(discretization PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include)
set_target_properties(discretization PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(discretization PRIVATE /W4)
else()
  target_compile_options(discretization PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- C++ tests (GoogleTest) ----
if(DISCRETIZATION_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(discretization_tests tests_cpp/test_discretization.cpp)
  target_link_libraries(discretization_tests PRIVATE discretization GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(discretization_tests)
endif()

# ---- Python extension (pybind11) ----
if(DISCRETIZATION_BUILD_PYTHON)
  find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
  find_package(pybind11 CONFIG REQUIRED)
  pybind11_add_module(_core python/src/bindings.cpp)
  target_link_libraries(_core PRIVATE discretization)
  install(TARGETS _core DESTINATION discretization)
endif()
