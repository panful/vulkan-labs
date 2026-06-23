include(${PROJECT_SOURCE_DIR}/cmake/build_target.cmake)

add_subdirectory(framework)

BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/01_vulkan_tutorial)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/02_advance)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/03_compute_shader)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/04_headless)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/05_geometry_shader)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/06_extensions)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/07_vulkan_hpp)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/08_showcases)
BuildTarget(${CMAKE_CURRENT_SOURCE_DIR}/09_memory_allocation)

add_subdirectory(10_scaffold)
