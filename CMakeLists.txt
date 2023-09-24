cmake_minimum_required(VERSION 3.10)
project(VulkanTriangle)
add_executable(${PROJECT_NAME} src/main.c)
target_link_libraries(${PROJECT_NAME} glfw vulkan)
