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

// match aspect ratio of the texture (2526x1787)
#define WIDTH 1272
#define HEIGHT 900
#define FRAMERATE 60
#define MAX_PACKETS 20
#define BRUSH_TEX_SIZE 256
#define BRUSH_RADIUS 100  // percent

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
    int x;
    int y;
    int size;
};

struct st_inkPoint {
    st_inkData inkData;
    int inkPointIndex;
};

const double timePerFrame = 1.0 / FRAMERATE;
bool shouldClearInk = false;
float inkMinSize = 5;
float inkMaxSize = 20;
int spacing = 1;

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

double module(double x, double y) {
    return std::sqrt(x * x + y * y);
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

    unsigned int mainProgram;
    if (!createAndLinkProgram(&mainProgram, shaders, 2)) {
        std::cout << "Failed to create program" << std::endl;
        glfwTerminate();
        return -1;
    }

    unsigned int bgProgram;
    if (!createAndLinkProgram(&bgProgram, shaders + 2, 2)) {
        std::cout << "Failed to create program" << std::endl;
        glDeleteProgram(mainProgram);
        glfwTerminate();
        return -1;
    }

    if (!LoadWintab()) {
        std::cout << "Failed to initialize WINTAB" << std::endl;
        glDeleteProgram(mainProgram);
        glDeleteProgram(bgProgram);
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
        std::cout << "Failed to initialize WINTAB context" << std::endl;
        UnloadWintab();
        glDeleteProgram(mainProgram);
        glDeleteProgram(bgProgram);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_BLEND);
    glClearColor(0, 0, 0, 0);

    unsigned int bgTexture, brushTexture, inkLayerTexture;
    glGenTextures(1, &bgTexture);
    glBindTexture(GL_TEXTURE_2D, bgTexture);
    // default values require mipmaps so we define these
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    int bgWidth, bgHeight, bgChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *bg_data = stbi_load("assets/img/04.png", &bgWidth, &bgHeight, &bgChannels, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bgWidth, bgHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, bg_data);
    stbi_image_free(bg_data);

    glGenTextures(1, &brushTexture);
    glBindTexture(GL_TEXTURE_2D, brushTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    unsigned char *brush_data = (unsigned char *) malloc(BRUSH_TEX_SIZE * BRUSH_TEX_SIZE);
    const int radius_squared = 1L * BRUSH_TEX_SIZE * BRUSH_RADIUS * BRUSH_TEX_SIZE * BRUSH_RADIUS / (2 * 100 * 2 * 100);
    for (int i = 0; i < BRUSH_TEX_SIZE; ++i) {
        for (int j = 0; j < BRUSH_TEX_SIZE; ++j) {
            const int x = i - BRUSH_TEX_SIZE / 2;
            const int y = j - BRUSH_TEX_SIZE / 2;
            const int dist_squared = x * x + y * y;
            const int val = 255 - dist_squared * 256 / radius_squared;
            brush_data[i + j * BRUSH_TEX_SIZE] = val < 0 ? 0 : val > 255 ? 255: val;
        }
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, BRUSH_TEX_SIZE, BRUSH_TEX_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, brush_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    free(brush_data);

    glGenTextures(1, &inkLayerTexture);
    glBindTexture(GL_TEXTURE_2D, inkLayerTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bgWidth, bgHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    unsigned int inkingFbo;
    glGenFramebuffers(1, &inkingFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, inkingFbo);

    // make sure the thing is empty
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, inkLayerTexture, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Inking FRAMEBUFFER not complete" << std::endl;
        UnloadWintab();
        glDeleteFramebuffers(1, &inkingFbo);
        glDeleteProgram(mainProgram);
        glDeleteProgram(bgProgram);
        glfwTerminate();
        return -1;
    }

    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(0, 3, GL_INT, 4 * sizeof(int), (void *) nullptr);
    glVertexAttribIPointer(1, 1, GL_INT, 4 * sizeof(int), (void *) (3 * sizeof(int)));

    unsigned int bgVao, bgVbo;
    glGenVertexArrays(1, &bgVao);
    glGenBuffers(1, &bgVbo);

    glBindVertexArray(bgVao);
    glBindBuffer(GL_ARRAY_BUFFER, bgVbo);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(0, 2, GL_INT, 4 * sizeof(int), (void *) nullptr);
    glVertexAttribIPointer(1, 2, GL_INT, 4 * sizeof(int), (void *) (2 * sizeof(int)));

    std::vector<st_inkPoint> inkPoints;
    bool stroking = false;
    double leftoverDistance = 0;
    st_inkData prev = {};

    double lastRender = 0;
    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();

        int window_x, window_y, window_w, window_h;
        glfwGetWindowPos(window, &window_x, &window_y);
        glfwGetWindowSize(window, &window_w, &window_h);

        const double windowAspectRatio = (double) window_w / window_h;
        const double bgAspectRatio = (double) bgWidth / bgHeight;

        int bg_x, bg_y, bg_w, bg_h;

        if (bgAspectRatio < windowAspectRatio) {
            bg_w = window_h * bgWidth / bgHeight;
            bg_h = window_h;
            bg_x = (window_w - bg_w) / 2;
            bg_y = 0;
        } else {
            bg_w = window_w;
            bg_h = window_w * bgHeight / bgWidth;
            bg_x = 0;
            bg_y = (window_h - bg_h) / 2;
        }

        double canvasScale = (double) bg_w / bgWidth;

        processInput(window);

        glBindFramebuffer(GL_FRAMEBUFFER, inkingFbo);
        glViewport(0, 0, bgWidth, bgHeight);

        if (shouldClearInk) {
            glClear(GL_COLOR_BUFFER_BIT);
            shouldClearInk = false;
        }

        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(mainProgram);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindTexture(GL_TEXTURE_2D, brushTexture);

        glUniform1i(0, window_x + bg_x);
        glUniform1i(1, window_y + bg_y);
        glUniform1i(2, bg_w);
        glUniform1i(3, bg_h);
        glUniform1i(4, (int) pressure.axMax);
        glUniform1f(5, inkMinSize);
        glUniform1f(6, inkMaxSize);
        glUniform1i(7, bg_x);
        glUniform1i(8, bg_y);

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

                if (!stroking) {
                    if (pkt.pkNormalPressure == 0) {
                        continue;
                    }

                    // first point
                    st_inkData ink = {pkt.pkX, pkt.pkY, (int) pkt.pkNormalPressure};
                    inkPoints.push_back({ink, 1});
                    inkPoints.push_back({ink, 2});
                    inkPoints.push_back({ink, 3});
                    inkPoints.push_back({ink, 1});
                    inkPoints.push_back({ink, 3});
                    inkPoints.push_back({ink, 4});

                    stroking = true;
                    leftoverDistance = 0;
                    prev = ink;
                    continue;
                }

                if (pkt.pkNormalPressure == 0) {
                    // finish stroke;
                    stroking = false;
                    continue;
                }

                st_inkData ink = {pkt.pkX, pkt.pkY, (int) pkt.pkNormalPressure};

                double dist = module(ink.x - prev.x, ink.y - prev.y);
                int count = 1;
                while (spacing * count <= dist + leftoverDistance) {
                    double scaling = (spacing * count - leftoverDistance) / dist;
                    // point = (prev_to_curr / dist) * spacing * count
                    st_inkData fillerInk = {
                            (int) (prev.x + (ink.x - prev.x) * scaling),
                            (int) (prev.y + (ink.y - prev.y) * scaling),
                            (int) (prev.size + (ink.size - prev.size) * scaling)
                    };
                    inkPoints.push_back({fillerInk, 1});
                    inkPoints.push_back({fillerInk, 2});
                    inkPoints.push_back({fillerInk, 3});
                    inkPoints.push_back({fillerInk, 1});
                    inkPoints.push_back({fillerInk, 3});
                    inkPoints.push_back({fillerInk, 4});

                    count++;
                }
                leftoverDistance += dist - spacing * (count - 1);
                prev = ink;
            }
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(int) * 4 * inkPoints.size(), inkPoints.data(),
                     GL_STATIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (int) inkPoints.size());
        inkPoints.clear();

        if (now >= lastRender + timePerFrame) {
            lastRender = now;

            int framebuffer_w, framebuffer_h;
            glfwGetFramebufferSize(window, &framebuffer_w, &framebuffer_h);

            const int bgVertices[] = {
                    bg_x, bg_y, 0, 1,
                    bg_x, bg_y + bg_h, 0, 0,
                    bg_x + bg_w, bg_y + bg_h, 1, 0,
                    bg_x, bg_y, 0, 1,
                    bg_x + bg_w, bg_y + bg_h, 1, 0,
                    bg_x + bg_w, bg_y, 1, 1
            };

            const int brushVertices[] = {
                    window_w - 200, 25, 0, 1,
                    window_w - 200, 200, 0, 0,
                    window_w - 25, 200, 1, 0,
                    window_w - 200, 25, 0, 1,
                    window_w - 25, 200, 1, 0,
                    window_w - 25, 25, 1, 1
            };

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, framebuffer_w, framebuffer_h);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(bgProgram);
            glBindVertexArray(bgVao);
            glBindBuffer(GL_ARRAY_BUFFER, bgVbo);

            glUniform1i(0, window_w);
            glUniform1i(1, window_h);

            glBindTexture(GL_TEXTURE_2D, bgTexture);
            glBufferData(GL_ARRAY_BUFFER, sizeof(bgVertices), bgVertices, GL_STATIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindTexture(GL_TEXTURE_2D, inkLayerTexture);
            glBufferData(GL_ARRAY_BUFFER, sizeof(bgVertices), bgVertices, GL_STATIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindTexture(GL_TEXTURE_2D, brushTexture);
            glBufferData(GL_ARRAY_BUFFER, sizeof(brushVertices), brushVertices, GL_STATIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glfwSwapBuffers(window);

            // std::cout << -lastRender + glfwGetTime() << std::endl;
        } else {
            // std::cout << "skip" << std::endl;
        }

        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &inkingFbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, &bgVao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &bgVbo);
    glDeleteProgram(mainProgram);
    glDeleteProgram(bgProgram);

    gpWTClose(hctx);
    UnloadWintab();
    glfwTerminate();

    return 0;
}
