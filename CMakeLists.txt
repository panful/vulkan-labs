cmake_minimum_required (VERSION 3.25)

project("Learning_Vulkan" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/assets" PROJECT_ASSETS_DIR)

option(ENABLE_CLANG_TIDY "Enable clang-tidy during compilation" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR "ENABLE_CLANG_TIDY is ON, but clang-tidy was not found in PATH.")
    endif()
endif()

add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W3 /Zc:__cplusplus /utf-8 /EHsc)
else()
    target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

find_package(Vulkan)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found")
else()
    message("------------------------------------------------------")
    message("Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
    message(" Vulkan_INCLUDE_DIR: ${Vulkan_INCLUDE_DIR}")
    message("     Vulkan_LIBRARY: ${Vulkan_LIBRARY}")
    message("   Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
    message("------------------------------------------------------")
endif()

add_subdirectory(third_party)
add_subdirectory(src)
