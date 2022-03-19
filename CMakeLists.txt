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
        lib/wacom-wintab/MSGPACK.H
        lib/wacom-wintab/PKTDEF.H
        lib/wacom-wintab/WINTAB.H
)
target_link_libraries(blue_archive_notes glfw opengl32)

add_custom_command(
        OUTPUT glsl/vertex.glsl glsl/fragment.glsl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/vertex.glsl glsl/vertex.glsl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/fragment.glsl glsl/fragment.glsl
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/vertex.glsl ${CMAKE_CURRENT_SOURCE_DIR}/src/glsl/fragment.glsl
)
add_custom_target(shaders DEPENDS glsl/vertex.glsl glsl/fragment.glsl)
add_dependencies(blue_archive_notes shaders)
