add_library(lvk_framework STATIC)

target_sources(lvk_framework
    PUBLIC
        include/lvk/application_desc.h
        include/lvk/frame_context.h
        include/lvk/glfw_input_adapter.h
        include/lvk/glfw_window.h
        include/lvk/input_state.h
        include/lvk/shader_loader.h
        include/lvk/swap_chain.h
        include/lvk/vk_utils.h
        include/lvk/vulkan_context.h
        include/lvk/vulkan_sample.h
    PRIVATE
        src/glfw_input_adapter.cpp
        src/glfw_window.cpp
        src/shader_loader.cpp
        src/swap_chain.cpp
        src/vk_utils.cpp
        src/vulkan_context.cpp
        src/vulkan_sample.cpp
)

target_include_directories(lvk_framework
    PUBLIC
        include
        ${Vulkan_INCLUDE_DIR}
)

target_link_libraries(lvk_framework
    PUBLIC
        glfw
        project_warnings
        ${Vulkan_LIBRARIES}
)

add_library(lvk_camera STATIC)

target_sources(lvk_camera
    PUBLIC
        include/lvk/camera/camera.h
        include/lvk/camera/camera_types.h
        include/lvk/camera/fps_camera_controller.h
        include/lvk/camera/orbit_camera_controller.h
    PRIVATE
        src/camera/camera.cpp
        src/camera/fps_camera_controller.cpp
        src/camera/orbit_camera_controller.cpp
)

target_include_directories(lvk_camera
    PUBLIC
        include
        ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(lvk_camera
    PUBLIC
        lvk_framework
        project_warnings
)

add_library(lvk_imgui STATIC)

target_sources(lvk_imgui
    PUBLIC
        include/lvk/imgui_layer.h
    PRIVATE
        src/imgui_layer.cpp
)

target_include_directories(lvk_imgui
    PUBLIC
        include
        ${Vulkan_INCLUDE_DIR}
)

target_link_libraries(lvk_imgui
    PUBLIC
        lvk_framework
        imgui
        project_warnings
)

if(ENABLE_CLANG_TIDY)
    set_target_properties(lvk_framework PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    set_target_properties(lvk_camera PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    set_target_properties(lvk_imgui PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()
