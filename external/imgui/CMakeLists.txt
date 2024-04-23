cmake_minimum_required(VERSION 3.12)

project(IMGUI_LIB_GENERATOR LANGUAGES C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${3rdPartyArchiveOutputDir})

set(target_name imgui)
file(GLOB IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(${target_name} STATIC ${IMGUI_SOURCES})
target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
target_include_directories(${target_name} PRIVATE ${3rdPartyHeadersOutputDir}) # GLFW

# 拷贝头文件
add_custom_command(TARGET ${target_name}
    POST_BUILD COMMAND ${CMAKE_COMMAND} -E
        copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${3rdPartyHeadersOutputDir}
)
