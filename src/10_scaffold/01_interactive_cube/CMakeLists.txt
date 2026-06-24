add_executable(10_01_interactive_cube)

target_sources(10_01_interactive_cube
    PRIVATE
        main.cpp
)

target_link_libraries(10_01_interactive_cube
    PRIVATE
        lvk_framework
        lvk_camera
        lvk_imgui
        project_warnings
)

set(interactive_cube_shaders
    shaders/base.vert
    shaders/base.frag
)

set(interactive_cube_shader_outputs)
foreach(shader ${interactive_cube_shaders})
    get_filename_component(shader_name "${shader}" NAME)
    string(REPLACE "." "_" shader_spv_name "${shader_name}")
    set(shader_output "${PROJECT_SHADER_DIR}/10_01_${shader_spv_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SHADER_DIR}"
        COMMAND ${GLSLC_EXE} "${CMAKE_CURRENT_SOURCE_DIR}/${shader}" -o "${shader_output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
        VERBATIM
    )
    list(APPEND interactive_cube_shader_outputs "${shader_output}")
endforeach()

add_custom_target(10_01_interactive_cube_shaders DEPENDS ${interactive_cube_shader_outputs})
add_dependencies(10_01_interactive_cube 10_01_interactive_cube_shaders)

target_compile_definitions(10_01_interactive_cube
    PRIVATE
        "PROJECT_SHADER_DIR=\"${PROJECT_SHADER_DIR}/\""
)

if(ENABLE_CLANG_TIDY)
    set_target_properties(10_01_interactive_cube PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()
