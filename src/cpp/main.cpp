#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32

#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_TIME)
#define PACKETMODE PK_BUTTONS

#include "Utils.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <wacom-wintab/PKTDEF.H>

#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>

#define WIDTH 800
#define HEIGHT 600
#define FRAMERATE 120
#define MAX_PACKETS 20

struct st_shaderInfo {
    unsigned int type;
    const char *file;
} shaders[] = {
        {GL_VERTEX_SHADER,   "glsl/vertex.glsl"},
        {GL_FRAGMENT_SHADER, "glsl/fragment.glsl"},
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

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void errorCallback(int error, const char *description) {
    std::cout << "Code: " << error << std::endl;
    std::cout << description << std::endl;
}

st_point
canvasPointFromPacketCoords(int raw_x, int raw_y, int window_x, int window_y, int window_w, int window_h, float scale) {
    float x_canvas = ((float) (raw_x - window_x) - (float) window_w / 2) * scale;
    float y_canvas = -((float) (raw_y - window_y) - (float) window_h / 2) * scale;

    return {x_canvas, y_canvas};
}

float inkRadiusFromPressure(float pressure, float maxPressure, float maxSize) {
    return pressure * maxSize / maxPressure;
}

st_point toOGL(st_point p) {
    double glx = p.x / ((double) WIDTH / 2);
    double gly = p.y / ((double) HEIGHT / 2);
    return {static_cast<float>(glx), static_cast<float>(gly)};
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
        lcMine.lcOptions |= CXO_SYSTEM;    // move system cursor
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

    std::vector<st_triangle> triangles;

    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *) nullptr);

    std::vector<st_inkData> inkData;
    bool strokeOngoing = false;
    bool strokeStart = true;

    double lastRender = 0;
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        const double now = glfwGetTime();

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
                float inkRadius = inkRadiusFromPressure(pkt.pkNormalPressure, pressure.axMax, 10);
                st_inkData ink = {p.x, p.y, inkRadius};

                if (strokeOngoing) {
                    st_inkData last = inkData.back();

                    // check distance between points
                    float x_dir = ink.x - last.x;
                    float y_dir = ink.y - last.y;
                    float d2 = x_dir * x_dir + y_dir * y_dir;
                    float d = std::sqrt(d2);
                    std::cout << "distance: " << d << std::endl;
                    if (d <= 5) {  // TODO does this value depend on the scale?
                        continue;
                    }

                    // get perpendicular direction
                    x_dir /= d;
                    y_dir /= d;
                    float temp = x_dir;
                    x_dir = -y_dir;
                    y_dir = temp;

                    // get points for the triangles
                    st_point last_positive;
                    st_point last_negative;
                    if (strokeStart) {
                        last_positive = {last.x + x_dir * last.r, last.y + y_dir * last.r};
                        last_negative = {last.x - x_dir * last.r, last.y - y_dir * last.r};
                        strokeStart = false;
                    } else {
                        st_triangle lastTriangle = triangles.back();
                        last_positive = lastTriangle.p3;
                        last_negative = lastTriangle.p2;
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

            glUniform1i(1, window_w);
            glUniform1i(2, window_h);
            glUniform1f(3, 1);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * 3 * triangles.size(), triangles.data(),
                         GL_STATIC_DRAW);

            glUseProgram(mainProgram);
            glBindVertexArray(vao);

            glDrawArrays(GL_TRIANGLES, 0, 3 * triangles.size());

            glfwSwapBuffers(window);
        }

        glfwPollEvents();
    }

    glDeleteProgram(mainProgram);

    gpWTClose(hctx);
    UnloadWintab();
    glfwTerminate();

    return 0;
}
