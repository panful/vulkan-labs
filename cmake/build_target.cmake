function(BuildTarget path)
    get_filename_component(var_name ${path} NAME)
    string(FIND ${var_name} "_" underscore_pos)

    if (underscore_pos GREATER 0)
        string(SUBSTRING ${var_name} 0 ${underscore_pos} number)
        file(GLOB subdirectories ${path}/*)

        foreach(subdir ${subdirectories})
            get_filename_component(tar_name ${subdir} NAME)
            file(GLOB_RECURSE subdir_sources ${subdir}/*.cpp)
            set(target_name "${number}_${tar_name}")

            # target
            add_executable(${target_name} ${subdir_sources})

            # 3rdparty
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/includes)
            target_link_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/libs)
            target_link_libraries(${target_name} PRIVATE glfw imgui)

            # vulkan
            target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
            target_link_libraries(${target_name} PRIVATE ${Vulkan_LIBRARIES})

            # 确保 imgui 已经成功构建
            add_dependencies(${target_name} imgui)

            install(TARGETS ${target_name} RUNTIME DESTINATION .)

        endforeach(subdir ${subdirectories})
    endif()
endfunction(BuildTarget path)
