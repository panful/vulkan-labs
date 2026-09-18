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

            # 3rdparty
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
            target_link_libraries(${target_name} PRIVATE glfw imgui project_warnings)
            target_compile_definitions(${target_name} PRIVATE "PROJECT_ASSETS_DIR=\"${PROJECT_ASSETS_DIR}/\"")
            if(ENABLE_CLANG_TIDY)
                set_target_properties(${target_name} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
            endif()

            # vulkan
            target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
            target_link_libraries(${target_name} PRIVATE ${Vulkan_LIBRARIES})

        endforeach(subdir ${subdirectories})
    endif()
endfunction(BuildTarget path)
