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

            # IMGUI
            file(GLOB IMGUI_SRC ${PROJECT_SOURCE_DIR}/3rdparty/imgui/src/*)

            # target
            add_executable(${target_name} ${subdir_sources} ${IMGUI_SRC})

            # 3rdparty
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/glm/include)
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/GLFW/include)
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb_image)
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/tiny_obj)
            target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/imgui/include)
            
            target_link_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/GLFW/lib/$<IF:$<CONFIG:Debug>,Debug,Release>/${CMAKE_HOST_SYSTEM_NAME})

            # vulkan
            target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
            target_link_libraries(${target_name} PRIVATE glfw3 ${Vulkan_LIBRARIES})

            install(TARGETS ${target_name} RUNTIME DESTINATION .)

        endforeach(subdir ${subdirectories})
    endif()
endfunction(BuildTarget path)