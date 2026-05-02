cmake_minimum_required(VERSION 3.23)

project(sdl3_hpp VERSION 0.1.0 LANGUAGES CXX)

add_library(sdl3_hpp INTERFACE)
add_library(sdl3_hpp::sdl3_hpp ALIAS sdl3_hpp)

target_compile_features(sdl3_hpp INTERFACE cxx_std_23)
target_include_directories(
    sdl3_hpp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
target_link_libraries(sdl3_hpp INTERFACE SDL3::SDL3)

option(SDL3_HPP_BUILD_TESTS "Build sdl3.hpp tests" ON)

include(CTest)

if(BUILD_TESTING AND SDL3_HPP_BUILD_TESTS)
    add_executable(sdl3_hpp_tests tests/sdl3_hpp_tests.cpp)
    target_link_libraries(sdl3_hpp_tests PRIVATE sdl3_hpp)
    target_compile_features(sdl3_hpp_tests PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(sdl3_hpp_tests PRIVATE /W4)
    else()
        target_compile_options(sdl3_hpp_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME sdl3_hpp_tests COMMAND sdl3_hpp_tests)
endif()
