function(BuildTarget path)
    get_filename_component(var_name ${path} NAME)
    string(FIND ${var_name} "_" underscore_pos)

    if (underscore_pos GREATER 0)
        string(SUBSTRING ${var_name} 0 ${underscore_pos} number)
        file(GLOB subdirectories ${path}/*)

        foreach(subdir ${subdirectories})
            get_filename_component(tar_name ${subdir} NAME)
            file(GLOB_RECURSE subdir_sources ${subdir}/*.cpp)
            file(GLOB_RECURSE subdir_headers ${subdir}/*.h)
            set(target_name "${number}_${tar_name}")

            # target
            add_executable(${target_name} ${subdir_sources} ${subdir_headers})

            # shaders
            set(shader_outputs)
            string(FIND ${tar_name} "_" subdir_underscore_pos)
            if(subdir_underscore_pos GREATER 0)
                string(SUBSTRING ${tar_name} 0 ${subdir_underscore_pos} subdir_number)
                set(shader_prefix "${number}_${subdir_number}_")
                file(GLOB subdir_shaders ${subdir}/shaders/*.vert ${subdir}/shaders/*.frag ${subdir}/shaders/*.comp ${subdir}/shaders/*.geom)

                foreach(shader ${subdir_shaders})
                    get_filename_component(shader_name ${shader} NAME)
                    string(REPLACE "." "_" shader_spv_name ${shader_name})
                    set(shader_spv "${PROJECT_SHADER_DIR}/${shader_prefix}${shader_spv_name}.spv")

                    add_custom_command(
                        OUTPUT ${shader_spv}
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SHADER_DIR}"
                        COMMAND ${GLSLC_EXE} ${shader} -o ${shader_spv}
                        DEPENDS ${shader}
                        VERBATIM
                    )
                    list(APPEND shader_outputs ${shader_spv})
                endforeach()
            endif()

            foreach(source ${subdir_sources})
                file(READ ${source} source_content)
                string(REGEX MATCHALL "PROJECT_SHADER_DIR[ \t\r\n]*\"[^\"]+\\.spv\"" shader_refs "${source_content}")

                foreach(shader_ref ${shader_refs})
                    string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" shader_spv_name "${shader_ref}")
                    list(APPEND shader_outputs "${PROJECT_SHADER_DIR}/${shader_spv_name}")
                endforeach()
            endforeach()

            if(shader_outputs)
                list(REMOVE_DUPLICATES shader_outputs)
                add_custom_target(${target_name}_shaders DEPENDS ${shader_outputs})
                add_dependencies(${target_name} ${target_name}_shaders)
            endif()

            # 3rdparty
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
            target_link_libraries(${target_name} PRIVATE glfw imgui project_warnings)

            target_compile_definitions(${target_name} PRIVATE "PROJECT_ASSETS_DIR=\"${PROJECT_ASSETS_DIR}/\"")
            target_compile_definitions(${target_name} PRIVATE "PROJECT_SHADER_DIR=\"${PROJECT_SHADER_DIR}/\"")
            if(ENABLE_CLANG_TIDY)
                set_target_properties(${target_name} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
            endif()

            # vulkan
            target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
            target_link_libraries(${target_name} PRIVATE ${Vulkan_LIBRARIES})

        endforeach(subdir ${subdirectories})
    endif()
endfunction(BuildTarget path)
