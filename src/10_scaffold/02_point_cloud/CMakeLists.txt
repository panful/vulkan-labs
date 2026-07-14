set(executable_name "10_02_point_cloud")
set(shader_target_name "10_02_point_cloud_shaders")

add_executable(${executable_name}
    bounding_box.cpp
    bounding_box.h
    main.cpp
    point_cloud_vertex.cpp
    point_cloud_vertex.h
    potree_lod_selector.cpp
    potree_lod_selector.h
    potree_point_cloud.cpp
    potree_point_cloud.h
    potree_vertex_decoder.cpp
    potree_vertex_decoder.h
)

target_link_libraries(${executable_name}
    PRIVATE
        lvk_framework
        lvk_camera
        lvk_imgui
        project_warnings
)

target_include_directories(${executable_name}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party
)

set(shaders
    shaders/base.vert
    shaders/base.frag
)

set(shader_outputs)
foreach(shader ${shaders})
    get_filename_component(shader_name "${shader}" NAME)
    string(REPLACE "." "_" shader_spv_name "${shader_name}")
    set(shader_output "${PROJECT_SHADER_DIR}/10_02_${shader_spv_name}.spv")
    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SHADER_DIR}"
        COMMAND ${GLSLC_EXE} "${CMAKE_CURRENT_SOURCE_DIR}/${shader}" -o "${shader_output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
        VERBATIM
    )
    list(APPEND shader_outputs "${shader_output}")
endforeach()

add_custom_target(${shader_target_name} DEPENDS ${shader_outputs})
add_dependencies(${executable_name} ${shader_target_name})

target_compile_definitions(${executable_name}
    PRIVATE
        "PROJECT_SHADER_DIR=\"${PROJECT_SHADER_DIR}/\""
)

if(ENABLE_CLANG_TIDY)
    set_target_properties(${executable_name} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()
