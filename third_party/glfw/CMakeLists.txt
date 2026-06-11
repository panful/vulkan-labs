cmake_minimum_required(VERSION 3.4...3.20 FATAL_ERROR)

project(GLFW_LIB_GENERATOR VERSION 3.4.0 LANGUAGES C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${3rdPartyArchiveOutputDir})

include(CMakeDependentOption)
cmake_dependent_option(GLFW_BUILD_WIN32 "Build support for Win32" ON "WIN32" OFF)
cmake_dependent_option(GLFW_BUILD_X11 "Build support for X11" ON "UNIX;NOT APPLE" OFF)

set(target_name glfw)

add_library(glfw STATIC
                 ${CMAKE_CURRENT_SOURCE_DIR}/include/GLFW/glfw3.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/include/GLFW/glfw3native.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/internal.h 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/platform.h 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/mappings.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/context.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/init.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/input.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/monitor.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/platform.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/window.c
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/egl_context.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/osmesa_context.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/null_platform.h 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/null_joystick.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/null_init.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/null_monitor.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/null_window.c 
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/null_joystick.c)

if (WIN32)
    target_sources(glfw PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_time.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_thread.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_module.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_time.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_thread.c
    )
else()
    target_sources(glfw PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/posix_time.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/posix_thread.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/posix_module.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/posix_time.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/posix_thread.c
    )
endif()

if (GLFW_BUILD_WIN32)
    target_compile_definitions(glfw PRIVATE _GLFW_WIN32)
    target_sources(glfw PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_platform.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_joystick.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_init.c      
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_joystick.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_monitor.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_window.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/wgl_context.c
    )
endif()

if (GLFW_BUILD_X11)
    target_compile_definitions(glfw PRIVATE _GLFW_X11)
    target_sources(glfw PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x11_platform.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/xkb_unicode.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x11_init.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x11_monitor.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x11_window.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/xkb_unicode.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glx_context.c
    )
endif()

if (GLFW_BUILD_X11 OR GLFW_BUILD_WAYLAND)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(glfw PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/src/linux_joystick.h 
            ${CMAKE_CURRENT_SOURCE_DIR}/src/linux_joystick.c
        )
    endif()
    target_sources(glfw PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/posix_poll.h 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/posix_poll.c
    )
endif()

set_target_properties(glfw PROPERTIES
                      OUTPUT_NAME ${target_name}
                      C_STANDARD 99
                      C_EXTENSIONS OFF)

find_package(Threads REQUIRED)
target_link_libraries(glfw PRIVATE Threads::Threads)

if (GLFW_BUILD_X11)
    find_package(X11 REQUIRED)
    target_include_directories(glfw PRIVATE "${X11_X11_INCLUDE_PATH}")

    # Check for XRandR (modern resolution switching and gamma control)
    if (NOT X11_Xrandr_INCLUDE_PATH)
        message(FATAL_ERROR "RandR headers not found; install libxrandr development package")
    endif()
    target_include_directories(glfw PRIVATE "${X11_Xrandr_INCLUDE_PATH}")

    # Check for Xinerama (legacy multi-monitor support)
    if (NOT X11_Xinerama_INCLUDE_PATH)
        message(FATAL_ERROR "Xinerama headers not found; install libxinerama development package")
    endif()
    target_include_directories(glfw PRIVATE "${X11_Xinerama_INCLUDE_PATH}")

    # Check for Xkb (X keyboard extension)
    if (NOT X11_Xkb_INCLUDE_PATH)
        message(FATAL_ERROR "XKB headers not found; install X11 development package")
    endif()
    target_include_directories(glfw PRIVATE "${X11_Xkb_INCLUDE_PATH}")

    # Check for Xcursor (cursor creation from RGBA images)
    if (NOT X11_Xcursor_INCLUDE_PATH)
        message(FATAL_ERROR "Xcursor headers not found; install libxcursor development package")
    endif()
    target_include_directories(glfw PRIVATE "${X11_Xcursor_INCLUDE_PATH}")

    # Check for XInput (modern HID input)
    if (NOT X11_Xi_INCLUDE_PATH)
        message(FATAL_ERROR "XInput headers not found; install libxi development package")
    endif()
    target_include_directories(glfw PRIVATE "${X11_Xi_INCLUDE_PATH}")

    # Check for X Shape (custom window input shape)
    if (NOT X11_Xshape_INCLUDE_PATH)
        message(FATAL_ERROR "X Shape headers not found; install libxext development package")
    endif()
    target_include_directories(glfw PRIVATE "${X11_Xshape_INCLUDE_PATH}")
endif()

# Make GCC warn about declarations that VS 2010 and 2012 won't accept for all
# source files that VS will build (Clang ignores this because we set -std=c99)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/context.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/init.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/input.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/monitor.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/window.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/null_init.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/null_joystick.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/null_monitor.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/null_window.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_init.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_joystick.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_module.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_monitor.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_time.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_thread.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32_window.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/wgl_context.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/egl_context.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/osmesa_context.c 
    PROPERTIES
        COMPILE_FLAGS -Wdeclaration-after-statement
    )
endif()

# Enable a reasonable set of warnings
# NOTE: The order matters here, Clang-CL matches both MSVC and Clang
if (MSVC)
    target_compile_options(glfw PRIVATE "/W3")
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
        CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
        CMAKE_C_COMPILER_ID STREQUAL "AppleClang")

    target_compile_options(glfw PRIVATE "-Wall")
endif()

if (GLFW_BUILD_WIN32)
    target_compile_definitions(glfw PRIVATE UNICODE _UNICODE)
endif()

# Workaround for the MS CRT deprecating parts of the standard library
if (MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    target_compile_definitions(glfw PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(glfw PRIVATE _DEFAULT_SOURCE)
endif()

# 拷贝头文件
add_custom_command(TARGET ${target_name}
    POST_BUILD COMMAND ${CMAKE_COMMAND} -E
        copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${3rdPartyHeadersOutputDir}
)
