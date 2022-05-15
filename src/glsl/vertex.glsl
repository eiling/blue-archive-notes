#version 430 core
layout (location = 0) in ivec3 inkPoint;
layout (location = 1) in int index;

layout (location = 0) uniform int window_x;
layout (location = 1) uniform int window_y;
layout (location = 2) uniform int window_w;
layout (location = 3) uniform int window_h;

layout (location = 4) uniform int maxPressure;
layout (location = 5) uniform float inkMinSize;
layout (location = 6) uniform float inkMaxSize;

out vec2 uv;

void main() {
    int windowHalfWidth = window_w / 2;
    int windowHalfHeight = window_h / 2;
    int offset_x = inkPoint.x - window_x - windowHalfWidth;
    int offset_y = inkPoint.y - window_y - windowHalfHeight;

    float halfInkSize = (inkMinSize + inkPoint.z * (inkMaxSize - inkMinSize) / maxPressure) / 2;
    float x = offset_x;
    float y = -offset_y;
    if (index == 1) {
        x = (x - halfInkSize) / windowHalfWidth;
        y = (y + halfInkSize) / windowHalfHeight;
        uv = vec2(0, 1);
    } else if (index == 2) {
        x = (x - halfInkSize) / windowHalfWidth;
        y = (y - halfInkSize) / windowHalfHeight;
        uv = vec2(0, 0);
    } else if (index == 3) {
        x = (x + halfInkSize) / windowHalfWidth;
        y = (y - halfInkSize) / windowHalfHeight;
        uv = vec2(1, 0);
    } else {
        x = (x + halfInkSize) / windowHalfWidth;
        y = (y + halfInkSize) / windowHalfHeight;
        uv = vec2(1, 1);
    }

    gl_Position = vec4(x, y, 0.0, 1.0);
}
