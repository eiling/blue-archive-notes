#version 430 core
layout (location = 0) in vec3 inkPoint;
layout (location = 1) in int index;

layout (location = 0) uniform int canvas_w;
layout (location = 1) uniform int canvas_h;

layout (location = 2) uniform int maxPressure;
layout (location = 3) uniform float inkMinSize;
layout (location = 4) uniform float inkMaxSize;

out vec2 uv;

void main() {
    int canvasHalfWidth = canvas_w / 2;
    int canvasHalfHeight = canvas_h / 2;
    float offset_x = inkPoint.x - canvasHalfWidth;
    float offset_y = inkPoint.y - canvasHalfHeight;

    float halfInkSize = (inkMinSize + inkPoint.z * (inkMaxSize - inkMinSize) / maxPressure) / 2;
    float x = offset_x;
    float y = -offset_y;
    if (index == 1) {
        x = (x - halfInkSize) / canvasHalfWidth;
        y = (y + halfInkSize) / canvasHalfHeight;
        uv = vec2(0, 1);
    } else if (index == 2) {
        x = (x - halfInkSize) / canvasHalfWidth;
        y = (y - halfInkSize) / canvasHalfHeight;
        uv = vec2(0, 0);
    } else if (index == 3) {
        x = (x + halfInkSize) / canvasHalfWidth;
        y = (y - halfInkSize) / canvasHalfHeight;
        uv = vec2(1, 0);
    } else {
        x = (x + halfInkSize) / canvasHalfWidth;
        y = (y + halfInkSize) / canvasHalfHeight;
        uv = vec2(1, 1);
    }

    gl_Position = vec4(x, y, 0.0, 1.0);
}
