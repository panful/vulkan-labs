add_executable(10_01_interactive_cube)

target_sources(10_01_interactive_cube
    PRIVATE
        01_interactive_cube/main.cpp
)

target_link_libraries(10_01_interactive_cube
    PRIVATE
        lvk_framework
        lvk_camera
        lvk_imgui
)

set(interactive_cube_shader_dir "${PROJECT_SHADER_DIR}/10_01_interactive_cube")
set(interactive_cube_shaders
    01_interactive_cube/shaders/base.vert
    01_interactive_cube/shaders/base.frag
)

set(interactive_cube_shader_outputs)
foreach(shader ${interactive_cube_shaders})
    get_filename_component(shader_name "${shader}" NAME)
    set(shader_output "${interactive_cube_shader_dir}/${shader_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${interactive_cube_shader_dir}"
        COMMAND ${GLSLC_EXE} "${CMAKE_CURRENT_SOURCE_DIR}/${shader}" -o "${shader_output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
        VERBATIM
    )
    list(APPEND interactive_cube_shader_outputs "${shader_output}")
endforeach()

add_custom_target(10_01_interactive_cube_shaders DEPENDS ${interactive_cube_shader_outputs})
add_dependencies(10_01_interactive_cube 10_01_interactive_cube_shaders)

file(TO_CMAKE_PATH "${interactive_cube_shader_dir}" interactive_cube_shader_dir_define)
target_compile_definitions(10_01_interactive_cube
    PRIVATE
        "LVK_SHADER_DIR=\"${interactive_cube_shader_dir_define}/\""
)

if(ENABLE_CLANG_TIDY)
    set_target_properties(10_01_interactive_cube PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()
