set(target_name "06_loadingModels")

add_executable(${target_name} "main.cpp")

# 3rdparty
target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/includes)
target_link_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/libs/$<IF:$<CONFIG:Debug>,Debug,Release>/${CMAKE_HOST_SYSTEM_NAME})

# vulkan
target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
target_link_libraries(${target_name} PRIVATE glfw3 ${Vulkan_LIBRARIES})

install(TARGETS ${target_name} RUNTIME DESTINATION .)
