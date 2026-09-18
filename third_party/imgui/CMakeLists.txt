cmake_minimum_required(VERSION 3.12)

project(IMGUI_LIB_GENERATOR LANGUAGES C)

set(target_name imgui)
file(GLOB IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(${target_name} STATIC ${IMGUI_SOURCES})
target_include_directories(${target_name}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${Vulkan_INCLUDE_DIR}
)
target_link_libraries(${target_name} PUBLIC glfw)
