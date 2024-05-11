cmake_minimum_required (VERSION 3.12)

project ("VulkanProject")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install)

if(MSVC)
    # 解决MSVC C4819警告
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /Zc:__cplusplus /utf-8 /EHsc")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/target)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/target)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/target)
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

set(3rdPartyArchiveOutputDir ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(3rdPartyHeadersOutputDir ${CMAKE_CURRENT_SOURCE_DIR}/includes)

include(${PROJECT_SOURCE_DIR}/cmake/copy_headers.cmake)
CopyHeaders()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/glfw)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_dependencies(imgui glfw)

include(${PROJECT_SOURCE_DIR}/cmake/build_target.cmake)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/sources/01_VulkanTutorial)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/sources/02_advance)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/sources/03_computeShader)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/sources/04_headless)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/sources/05_geometryShader)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/sources/06_extensions)
