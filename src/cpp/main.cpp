#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#define STB_IMAGE_IMPLEMENTATION

#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_TIME)
#define PACKETMODE PK_BUTTONS

#include "Utils.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <wacom-wintab/PKTDEF.H>
#include <stb_image.h>

#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>

#define WIDTH 1600
#define HEIGHT 920
#define FRAMERATE 120
#define MAX_PACKETS 20

struct st_shaderInfo {
    unsigned int type;
    const char *file;
} shaders[] = {
        {GL_VERTEX_SHADER,   "glsl/vertex.glsl"},
        {GL_FRAGMENT_SHADER, "glsl/fragment.glsl"},
        {GL_VERTEX_SHADER,   "glsl/backgroundVertex.glsl"},
        {GL_FRAGMENT_SHADER, "glsl/backgroundFragment.glsl"},
};

struct st_inkData {
    float x;
    float y;
    float r;
};

struct st_point {
    float x;
    float y;
};

struct st_triangle {
    st_point p1;
    st_point p2;
    st_point p3;
};

const double timePerFrame = 1.0 / FRAMERATE;
bool shouldClearInk = false;
int maxInkSize = 50;

int createShader(unsigned int *shader, unsigned int type, const char *file) {
    *shader = glCreateShader(type);

    std::ifstream t(file);
    std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    const char *c = str.c_str();

    glShaderSource(*shader, 1, &c, nullptr);
    glCompileShader(*shader);

    int success;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(*shader, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::" << type << "::COMPILATION_FAILED" << std::endl;
        std::cout << infoLog << std::endl;
    }
    return success;
}

int createAndLinkProgram(unsigned int *program, st_shaderInfo *shaderInfo, int shaderCount) {
    unsigned int shaderIds[shaderCount];
    int success, shaderStatus = createShader(shaderIds, shaderInfo[0].type, shaderInfo[0].file);
    for (int i = 1; shaderStatus && i < shaderCount; ++i) {
        shaderStatus = createShader(shaderIds + i, shaderInfo[i].type, shaderInfo[i].file);
    }

    if (shaderStatus) {
        *program = glCreateProgram();
        for (int i = 0; i < shaderCount; ++i) {
            glAttachShader(*program, shaderIds[i]);
        }
        glLinkProgram(*program);

        glGetProgramiv(*program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(*program, 512, nullptr, infoLog);
            std::cout << "ERROR::PROGRAM::LINKING_FAILED" << std::endl;
            std::cout << infoLog << std::endl;
        }
    }

    for (int i = 0; i < shaderCount; ++i) {
        glDetachShader(*program, shaderIds[i]);
        glDeleteShader(shaderIds[i]);
    }

    return shaderStatus && success;
}

void errorCallback(int error, const char *description) {
    std::cout << "Code: " << error << std::endl;
    std::cout << description << std::endl;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        shouldClearInk = true;
    }
}

st_point canvasPointFromPacketCoords(
        int raw_x, int raw_y, int window_x, int window_y, int window_w, int window_h, float scale
) {
    float x_canvas = ((float) (raw_x - window_x) - (float) window_w / 2) * scale;
    float y_canvas = -((float) (raw_y - window_y) - (float) window_h / 2) * scale;

    return {x_canvas, y_canvas};
}

float inkRadiusFromPressure(float pressure, float maxPressure, float maxSize) {
    return pressure * maxSize / maxPressure;
}

int main() {
    glfwSetErrorCallback(errorCallback);

    if (!glfwInit()) {
        std::cout << "GLFW initialization failed" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Blue Archive Notes", nullptr, nullptr);
    if (!window) {
        std::cout << "GLFW window creation failed" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "GLVersion: " << GLVersion.major << "." << GLVersion.minor << std::endl;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int mainProgram;
    if (!createAndLinkProgram(&mainProgram, shaders, 2)) {
        std::cout << "Failed to create program" << std::endl;
        glfwTerminate();
        return -1;
    }

    unsigned int bgProgram;
    if (!createAndLinkProgram(&bgProgram, shaders + 2, 2)) {
        std::cout << "Failed to create program" << std::endl;
        glfwTerminate();
        return -1;
    }

    if (!LoadWintab()) {
        std::cout << "Failed to initialize WINTAB" << std::endl;
        glfwTerminate();
        return -1;
    }

    LOGCONTEXT lcMine = {0};
    AXIS tabletX = {0};
    AXIS tabletY = {0};
    AXIS pressure = {0};
    HCTX hctx = nullptr;
    if (gpWTInfoA(WTI_DEFSYSCTX, 0, &lcMine) > 0) {
        UINT result;
        gpWTInfoA(WTI_DEVICES, DVC_HARDWARE, &result);
        bool displayTablet = result & HWC_INTEGRATED;

        gpWTInfoA(WTI_DEVICES, DVC_PKTRATE, &result);
        std::cout << "pktrate: " << result << std::endl;

        char name[1024];
        gpWTInfoA(WTI_DEVICES + -1, DVC_NAME, name);
        std::cout << "name: " << name << std::endl;

        std::cout << "type: " << (displayTablet ? "display (integrated)" : "opaque") << std::endl;

        lcMine.lcPktData = PACKETDATA;
        lcMine.lcOptions |= CXO_MESSAGES;
        lcMine.lcOptions |= CXO_SYSTEM;  // move system cursor
        lcMine.lcPktMode = PACKETMODE;
        lcMine.lcMoveMask = PACKETDATA;
        lcMine.lcBtnUpMask = lcMine.lcBtnDnMask;

        // Set the entire tablet as active
        UINT wWTInfoRetVal = gpWTInfoA(WTI_DEVICES, DVC_X, &tabletX);
        if (wWTInfoRetVal != sizeof(AXIS)) {
            std::cout << "This context should not be opened. ?????" << std::endl;
        } else {
            gpWTInfoA(WTI_DEVICES, DVC_Y, &tabletY);
            gpWTInfoA(WTI_DEVICES, DVC_NPRESSURE, &pressure);
            std::cout << "x: " << tabletX.axMin << ", " << tabletX.axMax << std::endl;
            std::cout << "y: " << tabletY.axMin << ", " << tabletY.axMax << std::endl;
            std::cout << "pressure: " << pressure.axMin << ", " << pressure.axMax << std::endl;

            // In Wintab, the tablet origin is lower left. Move origin to upper left so that it coincides with screen origin.
            lcMine.lcOutExtY = -lcMine.lcOutExtY;

            hctx = gpWTOpenA(glfwGetWin32Window(window), &lcMine, true);
        }
    }

    if (!hctx) {
        UnloadWintab();
    }

    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *) nullptr);

    unsigned int bg_vao, bg_vbo, bg_texture;
    glGenVertexArrays(1, &bg_vao);
    glGenBuffers(1, &bg_vbo);
    glGenTextures(1, &bg_texture);

    glBindVertexArray(bg_vao);
    glBindBuffer(GL_ARRAY_BUFFER, bg_vbo);
    glBindTexture(GL_TEXTURE_2D, bg_texture);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));

    const float bg_vertices[] = {
            -1, 1, 0, 1,
            -1, -1, 0, 0,
            1, -1, 1, 0,
            -1, 1, 0, 1,
            1, -1, 1, 0,
            1, 1, 1, 1
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(bg_vertices), bg_vertices, GL_STATIC_DRAW);

    int bgWidth, bgHeight, bgChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load("assets/img/04.png", &bgWidth, &bgHeight, &bgChannels, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bgWidth, bgHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    std::vector<st_triangle> triangles;
    std::vector<st_inkData> inkData;
    bool strokeOngoing = false;
    bool strokeStart = true;

    double lastRender = 0;
    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();

        processInput(window);

        if (shouldClearInk) {
            inkData.clear();
            triangles.clear();
            shouldClearInk = false;
        }

        PACKET packets[MAX_PACKETS];
        int numPackets = gpWTPacketsGet(hctx, MAX_PACKETS, (LPVOID) packets);
        if (numPackets > 0) {
            for (int i = 0; i < numPackets; i++) {
                PACKET pkt = packets[i];
                // std::cout << "Packet #" << i <<
                //           " - X: " << pkt.pkX <<
                //           "  Y: " << pkt.pkY <<
                //           "  pressure: " << pkt.pkNormalPressure <<
                //           "  time: " << pkt.pkTime <<
                //           std::endl;

                if (pkt.pkNormalPressure == 0) {
                    strokeOngoing = false;
                    strokeStart = true;
                    continue;
                }

                int window_x, window_y, window_w, window_h;
                glfwGetWindowPos(window, &window_x, &window_y);
                glfwGetWindowSize(window, &window_w, &window_h);

                st_point p = canvasPointFromPacketCoords(
                        pkt.pkX, pkt.pkY, window_x, window_y, window_w, window_h, 1
                );
                float inkRadius = inkRadiusFromPressure(pkt.pkNormalPressure, pressure.axMax, (float) maxInkSize);
                st_inkData ink = {p.x, p.y, inkRadius};

                if (strokeOngoing) {
                    st_inkData last = inkData.back();

                    // check distance between points
                    float x_dir = ink.x - last.x;
                    float y_dir = ink.y - last.y;
                    float d2 = x_dir * x_dir + y_dir * y_dir;
                    float d = std::sqrt(d2);
                    if (d <= inkRadius) {  // TODO I'm not sure I understand why the ink size goes here
                        continue;
                    }

                    // get perpendicular direction
                    x_dir /= d;
                    y_dir /= d;
                    float temp = x_dir;
                    x_dir = -y_dir;
                    y_dir = temp;

                    // get points for the triangles
                    st_point last_positive = {last.x + x_dir * last.r, last.y + y_dir * last.r};
                    st_point last_negative = {last.x - x_dir * last.r, last.y - y_dir * last.r};
                    if (strokeStart) {
                        // last_positive = {last.x + x_dir * last.r, last.y + y_dir * last.r};
                        // last_negative = {last.x - x_dir * last.r, last.y - y_dir * last.r};
                        strokeStart = false;
                    } else {
                        st_triangle lastTriangle = triangles.back();
                        float dx1 = last_positive.x - lastTriangle.p3.x;
                        float dy1 = last_positive.y - lastTriangle.p3.y;
                        float dd1 = dx1 * dx1 + dy1 * dy1;

                        float dx2 = last_positive.x - lastTriangle.p2.x;
                        float dy2 = last_positive.y - lastTriangle.p2.y;
                        float dd2 = dx2 * dx2 + dy2 * dy2;
                        if (dd1 < dd2) {
                            last_positive = lastTriangle.p3;
                            last_negative = lastTriangle.p2;
                        } else {
                            last_positive = lastTriangle.p2;
                            last_negative = lastTriangle.p3;
                        }
                    }
                    st_point ink_positive = {ink.x + x_dir * ink.r, ink.y + y_dir * ink.r};
                    st_point ink_negative = {ink.x - x_dir * ink.r, ink.y - y_dir * ink.r};

                    // add triangles to vector
                    triangles.push_back({last_positive, last_negative, ink_positive});
                    triangles.push_back({last_negative, ink_negative, ink_positive});
                }

                inkData.push_back(ink);
                strokeOngoing = true;
            }
        }

        if (now >= lastRender + timePerFrame) {
            lastRender = now;

            int window_w, window_h;
            glfwGetWindowSize(window, &window_w, &window_h);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(bgProgram);
            glBindVertexArray(bg_vao);
            glBindBuffer(GL_ARRAY_BUFFER, bg_vbo);
            glBindTexture(GL_TEXTURE_2D, bg_texture);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            glUseProgram(mainProgram);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);

            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * 3 * triangles.size(), triangles.data(),
                         GL_STATIC_DRAW);

            glUniform1i(1, window_w);
            glUniform1i(2, window_h);
            glUniform1f(3, 1);

            glDrawArrays(GL_TRIANGLES, 0, 3 * triangles.size());

            glfwSwapBuffers(window);
        }

        glfwPollEvents();
    }

    glDeleteProgram(mainProgram);
    glDeleteProgram(bgProgram);

    gpWTClose(hctx);
    UnloadWintab();
    glfwTerminate();

    return 0;
}
