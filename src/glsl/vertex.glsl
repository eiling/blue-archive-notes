#version 430 core
layout (location = 0) in vec2 pos;

layout (location = 1) uniform int window_w;
layout (location = 2) uniform int window_h;
layout (location = 3) uniform float scale;

void main() {
    float x = pos.x * 2 / (window_w * scale);
    float y = pos.y * 2 / (window_h * scale);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
