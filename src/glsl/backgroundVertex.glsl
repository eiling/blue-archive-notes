#version 430 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 vertex_uv;

out vec2 uv;

void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
    uv = vertex_uv;
}
