#version 430 core
layout (location = 0) in vec2 pos;
layout (location = 1) in float index;

layout (location = 0) uniform int window_w;
layout (location = 1) uniform int window_h;
layout (location = 2) uniform float scale;

out vec2 uv;

void main() {
    float x = pos.x * 2 / (window_w * scale);
    float y = pos.y * 2 / (window_h * scale);
    gl_Position = vec4(x, y, 0.0, 1.0);
    if (index == 1) {
        uv = vec2(0, 1);
    } else if (index == 2) {
        uv = vec2(0, 0);
    } else if (index == 3) {
        uv = vec2(1, 0);
    } else {
        uv = vec2(1, 1);
    }
}
