cmake_minimum_required(VERSION 3.16)
project(blue_archive_notes)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/glfw)

include_directories(lib)

add_executable(
        blue_archive_notes
        src/cpp/main.cpp
        src/cpp/Utils.h
        src/cpp/Utils.cpp
        lib/glad/glad.h
        lib/glad/glad.c
)
target_link_libraries(blue_archive_notes glfw opengl32)

add_custom_command(
        OUTPUT glsl/vertex.glsl glsl/fragment.glsl glsl/backgroundVertex.glsl glsl/backgroundFragment.glsl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/vertex.glsl glsl/vertex.glsl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/fragment.glsl glsl/fragment.glsl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/backgroundVertex.glsl glsl/backgroundVertex.glsl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/backgroundFragment.glsl glsl/backgroundFragment.glsl
        DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/vertex.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/fragment.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/backgroundVertex.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/backgroundFragment.glsl
)
add_custom_target(shaders DEPENDS glsl/vertex.glsl glsl/fragment.glsl glsl/backgroundVertex.glsl glsl/backgroundFragment.glsl)
add_dependencies(blue_archive_notes shaders)

add_custom_command(
        OUTPUT assets/img/04.png
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/img/04.png assets/img/04.png
        DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/img/04.png
)
add_custom_target(assets DEPENDS assets/img/04.png)
add_dependencies(blue_archive_notes assets)
