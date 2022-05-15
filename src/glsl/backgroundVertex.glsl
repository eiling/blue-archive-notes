#version 430 core
layout (location = 0) in ivec2 pos;
layout (location = 1) in ivec2 vertex_uv;

layout (location = 0) uniform int window_w;
layout (location = 1) uniform int window_h;

out vec2 uv;

void main() {
    int windowHalfWidth = window_w / 2;
    int windowHalfHeight = window_h / 2;
    int offset_x = pos.x - windowHalfWidth;
    int offset_y = pos.y - windowHalfHeight;
    float x = offset_x;
    float y = -offset_y;
    x = x / windowHalfWidth;
    y = y / windowHalfHeight;

    gl_Position = vec4(x, y, 0.0, 1.0);
    uv = vec2(vertex_uv.x, vertex_uv.y);
}
