
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.hpp"
#include "camera.hpp"

// STB used for icon textures (fire/snow/ok)

#include "stb_image.h"

// ASSIMP Model loader
#include "model.hpp"

// ===================== HELPERS =====================
static float clampf(float x, float a, float b) { return (x < a) ? a : (x > b ? b : x); }

// ===================== CONSTANTS =====================
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// ===== SIMPLE STATE =====
bool  klimaOn = false;
int   targetTemp = 24;
float currentTemp = 31.0f;

// lid anim
float lidT = 0.0f;         // 0..1
const float LID_SPEED = 3.5f;

// ===== KLIMA (AC) TRANSFORM =====
const glm::vec3 AC_POS = glm::vec3(0.0f, 2.45f, -2.93f);
const glm::vec3 AC_SCALE = glm::vec3(1.20f, 0.45f, 0.35f);
const float AC_FRONT_Z = AC_POS.z + (AC_SCALE.z * 0.5f);
const float UI_EPS = 0.01f;

// ===== SCREENS (3) =====
const float screenW = 0.30f;
const float screenH = 0.12f;
const float screenY = 2.42f;
const float screenZ = AC_FRONT_Z + UI_EPS;

const float SCREEN_X_LEFT = -0.34f;
const float SCREEN_X_MID = 0.00f;
const float SCREEN_X_RIGHT = 0.34f;

// ===== ROOM =====
static constexpr float FLOOR_THICK = 0.1f;
static constexpr float FLOOR_Y = -FLOOR_THICK * 0.5f;
static constexpr float WALL_Z_BACK = -3.0f;
static constexpr float WALL_THICK = 0.1f;

// ===== BASIN GEOM (mesh-space) =====
static constexpr float BASIN_H = 0.35f;
static constexpr float BASIN_R_TOP = 0.55f;
static constexpr float BASIN_R_BOT = 0.35f;

// ===== BASIN PLACEMENT =====
const float basinScale = 0.85f;
const float BASIN_EPS = 0.002f;

const float BASIN_PICK_DISTANCE = 3.0f;
const float BASIN_PICK_RADIUS = 2.0f;

// ===== WATER FILL =====
float waterLevel = 0.0f;                 // 0..1
const float waterFillPerHit = 0.004f;    // per droplet impact

unsigned int waterVAO = 0, waterVBO = 0;
int waterVertexCount = 0;

// ===== 7-SEG DIGITS =====
static bool DIGITS[10][7] = {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}  // 9
};

// ===== DROPLETS =====
struct Droplet {
    glm::vec3 pos;
    glm::vec3 vel;
    bool alive;
};
std::vector<Droplet> droplets;

const int   DROPLET_COUNT = 60;
const float DROPLET_SIZE = 0.02f;
const float DROPLET_GRAVITY = 6.5f;
const float DROPLET_SPAWN_JIT = 0.12f;

// ===================== CAMERA =====================
Camera camera(glm::vec3(0.0f, 1.5f, 0.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ===================== NEW INTERACTION STATE =====================
static bool basinHeld = false;    
static bool basinFull = false;    
static glm::vec3 basinPosDefault(0.0f);
static glm::vec3 basinPos(0.0f);

static bool pendingSpace = false;
static bool spaceWasDown = false;

static bool mouseClicked = false;
static double clickX = 0.0, clickY = 0.0;

// ===================== FORWARD DECLS =====================
void drawCube();
void drawBasin();

// ===================== INPUT =====================
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        glfwGetCursorPos(window, &clickX, &clickY);
        mouseClicked = true;
    }
}

static bool eWasDown = false;
static bool upWasDown = false;
static bool downWasDown = false;

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);

    bool eDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    if (eDown && !eWasDown) {
        if (!basinHeld)
            klimaOn = !klimaOn;
    }
    eWasDown = eDown;

    bool upDown = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    if (upDown && !upWasDown && klimaOn) targetTemp = std::min(40, targetTemp + 1);
    upWasDown = upDown;

    bool dnDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
    if (dnDown && !downWasDown && klimaOn) targetTemp = std::max(-10, targetTemp - 1);
    downWasDown = dnDown;

    bool sp = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (sp && !spaceWasDown) pendingSpace = true;
    spaceWasDown = sp;
}

// ===================== OPENGL GEOMETRY =====================
unsigned int cubeVAO = 0, cubeVBO = 0;
unsigned int basinVAO = 0, basinVBO = 0;
int basinVertexCount = 0;
unsigned int quadVAO = 0, quadVBO = 0;

// ===================== TEXTURES (ICONS) =====================
unsigned int texFire = 0, texSnow = 0, texOk = 0;

static unsigned int loadTextureRGBA(const char* path)
{
    int w, h, comp;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path, &w, &h, &comp, 4);
    if (!data) {
        std::cout << "Failed to load texture: " << path << "\n";
        return 0;
    }

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
}

void initCube()
{
    static const float vertices[] = {
        // positions        // normals
       -0.5f,-0.5f,-0.5f,   0,0,-1,
        0.5f,-0.5f,-0.5f,   0,0,-1,
        0.5f, 0.5f,-0.5f,   0,0,-1,
        0.5f, 0.5f,-0.5f,   0,0,-1,
       -0.5f, 0.5f,-0.5f,   0,0,-1,
       -0.5f,-0.5f,-0.5f,   0,0,-1,

       -0.5f,-0.5f, 0.5f,   0,0,1,
        0.5f,-0.5f, 0.5f,   0,0,1,
        0.5f, 0.5f, 0.5f,   0,0,1,
        0.5f, 0.5f, 0.5f,   0,0,1,
       -0.5f, 0.5f, 0.5f,   0,0,1,
       -0.5f,-0.5f, 0.5f,   0,0,1,

       -0.5f, 0.5f, 0.5f,  -1,0,0,
       -0.5f, 0.5f,-0.5f,  -1,0,0,
       -0.5f,-0.5f,-0.5f,  -1,0,0,
       -0.5f,-0.5f,-0.5f,  -1,0,0,
       -0.5f,-0.5f, 0.5f,  -1,0,0,
       -0.5f, 0.5f, 0.5f,  -1,0,0,

        0.5f, 0.5f, 0.5f,   1,0,0,
        0.5f, 0.5f,-0.5f,   1,0,0,
        0.5f,-0.5f,-0.5f,   1,0,0,
        0.5f,-0.5f,-0.5f,   1,0,0,
        0.5f,-0.5f, 0.5f,   1,0,0,
        0.5f, 0.5f, 0.5f,   1,0,0,

       -0.5f,-0.5f,-0.5f,   0,-1,0,
        0.5f,-0.5f,-0.5f,   0,-1,0,
        0.5f,-0.5f, 0.5f,   0,-1,0,
        0.5f,-0.5f, 0.5f,   0,-1,0,
       -0.5f,-0.5f, 0.5f,   0,-1,0,
       -0.5f,-0.5f,-0.5f,   0,-1,0,

       -0.5f, 0.5f,-0.5f,   0,1,0,
        0.5f, 0.5f,-0.5f,   0,1,0,
        0.5f, 0.5f, 0.5f,   0,1,0,
        0.5f, 0.5f, 0.5f,   0,1,0,
       -0.5f, 0.5f, 0.5f,   0,1,0,
       -0.5f, 0.5f,-0.5f,   0,1,0
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

static void pushVertex(std::vector<float>& v, const glm::vec3& p, const glm::vec3& n)
{
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
}

void initBasin(int segments = 48)
{
    std::vector<float> verts;
    verts.reserve(segments * 6 * 6);

    float h = BASIN_H;
    float rTop = BASIN_R_TOP;
    float rBottom = BASIN_R_BOT;
    float yTop = +h * 0.5f;
    float yBottom = -h * 0.5f;

    for (int i = 0; i < segments; i++)
    {
        float a0 = (float)i / segments * 2.0f * 3.1415926f;
        float a1 = (float)(i + 1) / segments * 2.0f * 3.1415926f;

        glm::vec3 p0t = { std::cos(a0) * rTop,    yTop,    std::sin(a0) * rTop };
        glm::vec3 p1t = { std::cos(a1) * rTop,    yTop,    std::sin(a1) * rTop };
        glm::vec3 p0b = { std::cos(a0) * rBottom, yBottom, std::sin(a0) * rBottom };
        glm::vec3 p1b = { std::cos(a1) * rBottom, yBottom, std::sin(a1) * rBottom };

        glm::vec3 n0 = glm::normalize(glm::vec3(std::cos(a0), (rBottom - rTop) / h, std::sin(a0)));
        glm::vec3 n1 = glm::normalize(glm::vec3(std::cos(a1), (rBottom - rTop) / h, std::sin(a1)));

        pushVertex(verts, p0t, n0);
        pushVertex(verts, p0b, n0);
        pushVertex(verts, p1b, n1);

        pushVertex(verts, p0t, n0);
        pushVertex(verts, p1b, n1);
        pushVertex(verts, p1t, n1);
    }

    glm::vec3 center = { 0.0f, yBottom, 0.0f };
    glm::vec3 nDown = { 0.0f, -1.0f, 0.0f };

    for (int i = 0; i < segments; i++)
    {
        float a0 = (float)i / segments * 2.0f * 3.1415926f;
        float a1 = (float)(i + 1) / segments * 2.0f * 3.1415926f;

        glm::vec3 p0 = { std::cos(a0) * rBottom, yBottom, std::sin(a0) * rBottom };
        glm::vec3 p1 = { std::cos(a1) * rBottom, yBottom, std::sin(a1) * rBottom };

        pushVertex(verts, center, nDown);
        pushVertex(verts, p1, nDown);
        pushVertex(verts, p0, nDown);
    }

    basinVertexCount = (int)(verts.size() / 6);

    glGenVertexArrays(1, &basinVAO);
    glGenBuffers(1, &basinVBO);

    glBindVertexArray(basinVAO);
    glBindBuffer(GL_ARRAY_BUFFER, basinVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// ===== WATER MESH =====
static void initWaterMesh()
{
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);

    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 6 * 256, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

static void updateWaterMesh(int segments, float level, float basinX, float basinY, float basinZ)
{
    level = clampf(level, 0.0f, 1.0f);
    if (level <= 0.0f) { waterVertexCount = 0; return; }

    std::vector<float> v;
    v.reserve(segments * 12 * 6);

    float r0 = BASIN_R_BOT * basinScale * 0.85f;
    float r1 = (BASIN_R_BOT + level * (BASIN_R_TOP - BASIN_R_BOT)) * basinScale * 0.85f;

    float yBottomWorld = basinY + basinScale * (-BASIN_H * 0.5f) + 0.006f;
    float yTopWorld = yBottomWorld + (level * basinScale * BASIN_H);

    for (int i = 0; i < segments; i++)
    {
        float a0 = (float)i / segments * 2.0f * 3.1415926f;
        float a1 = (float)(i + 1) / segments * 2.0f * 3.1415926f;

        glm::vec3 p0t = { std::cos(a0) * r1, yTopWorld,    std::sin(a0) * r1 };
        glm::vec3 p1t = { std::cos(a1) * r1, yTopWorld,    std::sin(a1) * r1 };
        glm::vec3 p0b = { std::cos(a0) * r0, yBottomWorld, std::sin(a0) * r0 };
        glm::vec3 p1b = { std::cos(a1) * r0, yBottomWorld, std::sin(a1) * r0 };

        glm::vec3 n0 = glm::normalize(glm::vec3(std::cos(a0), 0.2f, std::sin(a0)));
        glm::vec3 n1 = glm::normalize(glm::vec3(std::cos(a1), 0.2f, std::sin(a1)));

        auto push = [&](const glm::vec3& p, const glm::vec3& n) {
            v.push_back(p.x + basinX);
            v.push_back(p.y);
            v.push_back(p.z + basinZ);
            v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
            };

        push(p0t, n0); push(p0b, n0); push(p1b, n1);
        push(p0t, n0); push(p1b, n1); push(p1t, n1);
    }

    glm::vec3 center = { basinX, yTopWorld, basinZ };
    glm::vec3 nUp = { 0.0f, 1.0f, 0.0f };

    for (int i = 0; i < segments; i++)
    {
        float a0 = (float)i / segments * 2.0f * 3.1415926f;
        float a1 = (float)(i + 1) / segments * 2.0f * 3.1415926f;

        glm::vec3 p0 = { basinX + std::cos(a0) * r1, yTopWorld, basinZ + std::sin(a0) * r1 };
        glm::vec3 p1 = { basinX + std::cos(a1) * r1, yTopWorld, basinZ + std::sin(a1) * r1 };

        v.push_back(center.x); v.push_back(center.y); v.push_back(center.z);
        v.push_back(nUp.x);    v.push_back(nUp.y);    v.push_back(nUp.z);

        v.push_back(p0.x); v.push_back(p0.y); v.push_back(p0.z);
        v.push_back(nUp.x); v.push_back(nUp.y); v.push_back(nUp.z);

        v.push_back(p1.x); v.push_back(p1.y); v.push_back(p1.z);
        v.push_back(nUp.x); v.push_back(nUp.y); v.push_back(nUp.z);
    }

    waterVertexCount = (int)(v.size() / 6);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
}

// ===== TEXTURED QUAD (icon) =====
void initTexturedQuad()
{
    static const float v[] = {
        -0.5f,-0.5f,  0.0f,0.0f,
         0.5f,-0.5f,  1.0f,0.0f,
         0.5f, 0.5f,  1.0f,1.0f,

         0.5f, 0.5f,  1.0f,1.0f,
        -0.5f, 0.5f,  0.0f,1.0f,
        -0.5f,-0.5f,  0.0f,0.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// ===== DRAW BASIC =====
void drawCube()
{
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void drawBasin()
{
    glBindVertexArray(basinVAO);
    glDrawArrays(GL_TRIANGLES, 0, basinVertexCount);
    glBindVertexArray(0);
}

// ===== LID =====
void drawKlimaLid(Shader& shader)
{
    const float lidH = 0.08f;
    const float lidHalf = lidH * 0.5f;

    const float acBottomY = AC_POS.y - (AC_SCALE.y * 0.5f);
    const float baseY = acBottomY + lidHalf;

    const float travelDown = 0.04f;
    const float y = baseY - travelDown * lidT;

    const float z = AC_FRONT_Z + 0.010f;

    shader.setVec3("uObjectColor", 0.45f, 0.45f, 0.45f);

    glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, y, z));
    M = glm::scale(M, glm::vec3(AC_SCALE.x * 0.95f, lidH, 0.02f));
    shader.setMat4("uM", M);
    drawCube();
}

// ===================== SCENE HELPERS =====================
void drawScreen3D(Shader& shader, float x)
{
    shader.setVec3("uObjectColor", 0.05f, 0.05f, 0.05f);
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, screenY, screenZ));
    m = glm::scale(m, glm::vec3(screenW, screenH, 0.02f));
    shader.setMat4("uM", m);
    drawCube();
}

static void drawSegment3D(Shader& shader, const glm::vec3& center, const glm::vec3& size, bool on)
{
    if (on) shader.setVec3("uObjectColor", 0.95f, 0.15f, 0.15f);
    else    shader.setVec3("uObjectColor", 0.20f, 0.02f, 0.02f);

    glm::mat4 M = glm::translate(glm::mat4(1.0f), center);
    M = glm::scale(M, size);
    shader.setMat4("uM", M);
    drawCube();
}

static void drawDigit3D(Shader& shader, int digit, const glm::vec3& screenCenter)
{
    if (digit < 0 || digit > 9) return;

    const float z = screenZ + 0.0135f;
    const float w = screenW * 0.40f;
    const float h = screenH * 0.75f;

    const float segT = screenH * 0.10f;
    const float segZ = 0.006f;

    const float yTop = +h * 0.50f;
    const float yMid = 0.0f;
    const float yBot = -h * 0.50f;

    const float xL = -w * 0.50f;
    const float xR = +w * 0.50f;

    const float yU = +h * 0.25f;
    const float yD = -h * 0.25f;

    glm::vec3 horiz = glm::vec3(w, segT, segZ);
    glm::vec3 vert = glm::vec3(segT, h * 0.50f, segZ);

    bool* s = DIGITS[digit];

    drawSegment3D(shader, glm::vec3(screenCenter.x, screenCenter.y + yTop, z), horiz, s[0]);
    drawSegment3D(shader, glm::vec3(screenCenter.x + xR, screenCenter.y + yU, z), vert, s[1]);
    drawSegment3D(shader, glm::vec3(screenCenter.x + xR, screenCenter.y + yD, z), vert, s[2]);
    drawSegment3D(shader, glm::vec3(screenCenter.x, screenCenter.y + yBot, z), horiz, s[3]);
    drawSegment3D(shader, glm::vec3(screenCenter.x + xL, screenCenter.y + yD, z), vert, s[4]);
    drawSegment3D(shader, glm::vec3(screenCenter.x + xL, screenCenter.y + yU, z), vert, s[5]);
    drawSegment3D(shader, glm::vec3(screenCenter.x, screenCenter.y + yMid, z), horiz, s[6]);
}

static void drawNumber2DLike3D(Shader& shader, int value, const glm::vec3& screenCenter)
{
    int v = value;
    bool neg = v < 0;
    v = std::abs(v);
    if (v > 99) v = 99;

    int d0 = v / 10;
    int d1 = v % 10;

    const float z = screenZ + 0.0135f;
    const float dx = screenW * 0.24f;

    if (neg)
    {
        shader.setVec3("uObjectColor", 0.95f, 0.15f, 0.15f);
        glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(screenCenter.x - dx * 1.70f, screenCenter.y, z));
        M = glm::scale(M, glm::vec3(screenW * 0.12f, screenH * 0.07f, 0.006f));
        shader.setMat4("uM", M);
        drawCube();
    }

    if (v >= 10)
    {
        drawDigit3D(shader, d0, glm::vec3(screenCenter.x - dx, screenCenter.y, screenCenter.z));
        drawDigit3D(shader, d1, glm::vec3(screenCenter.x + dx, screenCenter.y, screenCenter.z));
    }
    else
    {
        drawDigit3D(shader, d1, screenCenter);
    }
}

static void drawLampCircle(Shader& shader)
{
   
    if (klimaOn) shader.setVec3("uObjectColor", 1.0f, 0.03f, 0.03f);
    else         shader.setVec3("uObjectColor", 0.18f, 0.02f, 0.02f);

    glm::vec3 p(0.45f, 2.56f, AC_FRONT_Z + UI_EPS + 0.002f);

    const float R = 0.055f;   
    const float T = 0.010f;   
    const float Z = 0.010f;   

    
    const int N = 24;

    for (int i = 0; i < N; i++)
    {
        float a = (float)i / (float)N * 3.1415926f; 
        float c = std::cos(a);
        float s = std::sin(a);

        glm::mat4 M(1.0f);
        M = glm::translate(M, p);
        M = glm::rotate(M, a, glm::vec3(0, 0, 1));

        
        M = glm::scale(M, glm::vec3(2.0f * R, T, Z));

        shader.setMat4("uM", M);
        drawCube();
    }
}


unsigned int pickStatusTex()
{
    int cur = (int)std::round(currentTemp);
    if (!klimaOn) return texOk;
    if (targetTemp > cur) return texFire;
    if (targetTemp < cur) return texSnow;
    return texOk;
}

void drawStatusIcon(Shader& texShader, const glm::mat4& P, const glm::mat4& V)
{
    if (!klimaOn) return;
    unsigned int tex = pickStatusTex();
    if (!tex) return;

    glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(SCREEN_X_RIGHT, screenY, screenZ + 0.0135f));
    M = glm::scale(M, glm::vec3(screenW * 0.65f, screenH * 0.65f, 1.0f));

    texShader.use();
    texShader.setMat4("uP", P);
    texShader.setMat4("uV", V);
    texShader.setMat4("uM", M);
    texShader.setInt("uTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ===================== WATER + DROPLETS =====================
float basinWaterTopY(float basinY)
{
    float yBottomWorld = basinY + basinScale * (-BASIN_H * 0.5f);
    return yBottomWorld + (waterLevel * basinScale * BASIN_H);
}

glm::vec3 outletWorldPos()
{
    float acBottomY = AC_POS.y - (AC_SCALE.y * 0.5f);
    return glm::vec3(0.0f, acBottomY - 0.02f, AC_FRONT_Z + 0.02f);
}

static float rand01() { return (float)std::rand() / (float)RAND_MAX; }

void spawnDroplet(Droplet& d)
{
    glm::vec3 o = outletWorldPos();
    float jx = (rand01() * 2.0f - 1.0f) * DROPLET_SPAWN_JIT;
    float jz = (rand01() * 2.0f - 1.0f) * DROPLET_SPAWN_JIT;

    d.pos = o + glm::vec3(jx, 0.0f, jz);
    d.vel = glm::vec3(0.0f, -0.2f - rand01() * 0.3f, 0.0f);
    d.alive = true;
}

void initDroplets()
{
    droplets.resize(DROPLET_COUNT);
    for (auto& d : droplets) spawnDroplet(d);
}

void updateDroplets(float dt, float basinY, float basinZ)
{
    if (!klimaOn) return;

    float surfaceY = basinWaterTopY(basinY);
    glm::vec3 basinC(0.0f, basinY, basinZ);

    float captureR = basinScale * BASIN_R_TOP * 0.85f;

    for (auto& d : droplets)
    {
        if (!d.alive) { spawnDroplet(d); continue; }

        d.vel.y -= DROPLET_GRAVITY * dt;
        d.pos += d.vel * dt;

        glm::vec2 dxz(d.pos.x - basinC.x, d.pos.z - basinC.z);
        float dist2 = dxz.x * dxz.x + dxz.y * dxz.y;

        if (d.pos.y <= surfaceY && dist2 <= captureR * captureR)
        {
            waterLevel = std::min(1.0f, waterLevel + waterFillPerHit);
            spawnDroplet(d);
            continue;
        }

        if (d.pos.y < -1.0f) {
            spawnDroplet(d);
            continue;
        }
    }
}

void drawDroplets(Shader& shader)
{
    if (!klimaOn) return;

    shader.setVec3("uObjectColor", 0.75f, 0.90f, 1.0f);

    for (const auto& d : droplets)
    {
        glm::mat4 M = glm::translate(glm::mat4(1.0f), d.pos);
        M = glm::scale(M, glm::vec3(DROPLET_SIZE, DROPLET_SIZE * 1.4f, DROPLET_SIZE));
        shader.setMat4("uM", M);
        drawCube();
    }
}

// ===================== PICKING + ANGLE CHECK =====================
static glm::vec3 screenRayDir(double mx, double my, int w, int h, const glm::mat4& P, const glm::mat4& V)
{
    float x = (2.0f * (float)mx) / (float)w - 1.0f;
    float y = 1.0f - (2.0f * (float)my) / (float)h;

    glm::vec4 rayClip(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(P) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(V) * rayEye));
    return rayWorld;
}

static bool raySphereHitDist(
    const glm::vec3& ro,
    const glm::vec3& rd,
    const glm::vec3& c,
    float r,
    float& outT
) {
    glm::vec3 oc = ro - c;

    float b = glm::dot(oc, rd);
    float c2 = glm::dot(oc, oc) - r * r;
    float h = b * b - c2;
    if (h < 0.0f) return false;

    float s = std::sqrt(h);

    float t0 = -b - s;
    float t1 = -b + s;

    float t = (t0 > 0.0f) ? t0 : t1;
    if (t <= 0.0f) return false;

    outT = t;
    return true;
}

static float angleDegXZ(const glm::vec3& a, const glm::vec3& b)
{
    glm::vec2 aa(a.x, a.z);
    glm::vec2 bb(b.x, b.z);
    float la = glm::length(aa);
    float lb = glm::length(bb);
    if (la < 1e-6f || lb < 1e-6f) return 180.0f;
    aa /= la;
    bb /= lb;
    float d = glm::clamp(aa.x * bb.x + aa.y * bb.y, -1.0f, 1.0f);
    return glm::degrees(std::acos(d));
}

// ===================== MODEL DRAW HELPERS =====================
static void applyModelCommonUniforms(Shader& sh, const glm::mat4& P, const glm::mat4& V)
{
    sh.use();
    sh.setMat4("uP", P);
    sh.setMat4("uV", V);
    sh.setVec3("uLightPos", 2.0f, 4.0f, 2.0f);
    sh.setVec3("uViewPos", camera.Position);
    sh.setVec3("uLightColor", 1.0f, 1.0f, 1.0f);
}

static void drawToiletModel(Model& toilet, Shader& modelShader)
{
    glm::mat4 M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(0.0f, 0.0f, 2.25f));
    M = glm::rotate(M, glm::radians(180.0f), glm::vec3(0, 1, 0));
    M = glm::scale(M, glm::vec3(1.0f)); // tune

    modelShader.setMat4("uM", M);
    toilet.Draw(modelShader);
}

static void drawRemoteModel(Model& remoteM, Shader& modelShader)
{
   
    glm::vec3 viewOffset(
        +0.35f,   // вправо
        -0.45f,   // вниз
        -1.25f    // от камеры вперёд
    );

    // view и inverse view
    glm::mat4 V = camera.GetViewMatrix();
    glm::mat4 invV = glm::inverse(V);

    // модель в view-space
    glm::mat4 M(1.0f);
    M = glm::translate(M, viewOffset);

    
    M = glm::rotate(M, glm::radians(180.0f), glm::vec3(0, 1, 0));
    M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));


    // масштаб
    M = glm::scale(M, glm::vec3(0.05f));

    // переводим обратно в world-space
    M = invV * M;

    modelShader.setMat4("uM", M);
    remoteM.Draw(modelShader);
}



// ===================== MAIN =====================
int main()
{
    std::srand(1337);

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "3D Klima Scene", monitor, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    bool depthOn = false;
    bool cullOn = false;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glCullFace(GL_BACK);


    if (glewInit() != GLEW_OK) { std::cout << "GLEW failed\n"; return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader shader("basic.vert", "basic.frag");       // cubes
    Shader texShader("tex.vert", "tex.frag");        // icons
    Shader modelShader("model.vert", "model.frag");  // obj+mtl

    initCube();
    initBasin();
    initWaterMesh();
    initTexturedQuad();

    texFire = loadTextureRGBA("res/fire.png");
    texSnow = loadTextureRGBA("res/snow.png");
    texOk = loadTextureRGBA("res/ok.png");

    // Load Models
    Model toilet("res/Toilet/Toilet.obj");
    Model remoteM("res/RemoteController/remote_controller.obj");

    // basin placement
    const float basinBottomLocal = -BASIN_H * 0.5f;
    const float basinBottomWorldOffset = basinScale * basinBottomLocal;

    const float basinY = 0.0f - basinBottomWorldOffset + BASIN_EPS;
    const float basinRTopWorld = basinScale * BASIN_R_TOP;

    const float backWallFrontZ = WALL_Z_BACK + (WALL_THICK * 0.5f);
    const float basinZ = backWallFrontZ + basinRTopWorld + 0.10f;

    basinPosDefault = glm::vec3(0.0f, basinY, basinZ);
    basinPos = basinPosDefault;

    initDroplets();

    while (!glfwWindowShouldClose(window))
    {
        float t = (float)glfwGetTime();
        deltaTime = t - lastFrame;
        lastFrame = t;

        processInput(window);

        static bool key1Pressed = false;
        static bool key2Pressed = false;

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !key1Pressed)
        {
            key1Pressed = true;
            depthOn = !depthOn;

            if (depthOn) glEnable(GL_DEPTH_TEST);
            else         glDisable(GL_DEPTH_TEST);

            std::cout << "Depth test: " << (depthOn ? "ON" : "OFF") << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE)
        {
            key1Pressed = false;
        }

        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !key2Pressed)
        {
            key2Pressed = true;
            cullOn = !cullOn;

            if (cullOn) glEnable(GL_CULL_FACE);
            else        glDisable(GL_CULL_FACE);

            std::cout << "Back-face culling: " << (cullOn ? "ON" : "OFF") << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE)
        {
            key2Pressed = false;
        }


        // temp drift towards target when on
        if (klimaOn) {
            float speed = 1.0f;
            if (currentTemp < (float)targetTemp) currentTemp = std::min((float)targetTemp, currentTemp + speed * deltaTime);
            if (currentTemp > (float)targetTemp) currentTemp = std::max((float)targetTemp, currentTemp - speed * deltaTime);
        }

        // droplets fill
        updateDroplets(deltaTime, basinY, basinZ);

        // auto-off when full 
        if (waterLevel >= 1.0f) {
            waterLevel = 1.0f;
            klimaOn = false;
            basinFull = true;
        }

        // lid animation
        float targetLid = klimaOn ? 1.0f : 0.0f;
        float step = LID_SPEED * deltaTime;
        if (lidT < targetLid) lidT = std::min(targetLid, lidT + step);
        else                  lidT = std::max(targetLid, lidT - step);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        glm::mat4 P = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
        glm::mat4 V = camera.GetViewMatrix();

        // click: pick basin only when full and not held
        if (mouseClicked) {
            mouseClicked = false;

            if (basinFull && !basinHeld) {
                glm::vec3 ro = camera.Position;
                glm::vec3 rd = screenRayDir(clickX, clickY, width, height, P, V);

                glm::vec3 basinCenter = glm::vec3(0.0f, basinY, basinZ);

                float tHit = 0.0f;
                if (raySphereHitDist(ro, rd, basinCenter, BASIN_PICK_RADIUS, tHit) && tHit < BASIN_PICK_DISTANCE)
                {
                    basinHeld = true;
                }
            }
        }

        // if held -> place in front of camera
        if (basinHeld) {
            const float holdDist = 0.75f;
            const float holdDown = 0.35f;
            basinPos = camera.Position + camera.Front * holdDist - camera.Up * holdDown;
        }
        else {
            basinPos = basinPosDefault;
        }

        // SPACE logic
        if (pendingSpace) {
            pendingSpace = false;

            if (basinHeld) {
                glm::vec3 camF = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
                glm::vec3 toAC = glm::normalize(glm::vec3(AC_POS.x - camera.Position.x, 0.0f, AC_POS.z - camera.Position.z));

                float angToAC = angleDegXZ(camF, toAC);
                float angAway = angleDegXZ(camF, -toAC);

                const float TOL = 35.0f;

                if (basinFull && angAway <= TOL) {
                    waterLevel = 0.0f;
                    basinFull = false;
                }
                else if (!basinFull && angToAC <= TOL) {
                    basinHeld = false;
                    basinPos = basinPosDefault;

                    klimaOn = false;
                    targetTemp = 24;
                    currentTemp = 31.0f;
                    lidT = 0.0f;

                    initDroplets();
                }
            }
        }

        // ===== Draw cubes scene =====
        shader.use();
        shader.setVec3("uLightPos", 2.0f, 4.0f, 2.0f);
        shader.setVec3("uViewPos", camera.Position);
        shader.setVec3("uLightColor", 1.0f, 1.0f, 1.0f);
        shader.setMat4("uP", P);
        shader.setMat4("uV", V);

        glm::mat4 M;

        // room
        shader.setVec3("uObjectColor", 0.8f, 0.8f, 0.8f);

        // floor
        M = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, FLOOR_Y, 0.0f));
        M = glm::scale(M, glm::vec3(6.0f, FLOOR_THICK, 6.0f));
        shader.setMat4("uM", M); drawCube();

        // ceiling
        M = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f));
        M = glm::scale(M, glm::vec3(6.0f, 0.1f, 6.0f));
        shader.setMat4("uM", M); drawCube();

        // walls
        M = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 1.5f, 0.0f));
        M = glm::scale(M, glm::vec3(0.1f, 3.0f, 6.0f));
        shader.setMat4("uM", M); drawCube();

        M = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 1.5f, 0.0f));
        M = glm::scale(M, glm::vec3(0.1f, 3.0f, 6.0f));
        shader.setMat4("uM", M); drawCube();

        M = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, -3.0f));
        M = glm::scale(M, glm::vec3(6.0f, 3.0f, 0.1f));
        shader.setMat4("uM", M); drawCube();

        M = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 3.0f));
        M = glm::scale(M, glm::vec3(6.0f, 3.0f, 0.1f));
        shader.setMat4("uM", M); drawCube();

        // AC body
        shader.setVec3("uObjectColor", 0.55f, 0.55f, 0.55f);
        M = glm::translate(glm::mat4(1.0f), AC_POS);
        M = glm::scale(M, AC_SCALE);
        shader.setMat4("uM", M); drawCube();

        // lid
        drawKlimaLid(shader);

        // lamp + screens
        drawLampCircle(shader);
        drawScreen3D(shader, SCREEN_X_LEFT);
        drawScreen3D(shader, SCREEN_X_MID);
        drawScreen3D(shader, SCREEN_X_RIGHT);

        // digits + icon only when klimaOn
        if (klimaOn)
        {
            drawNumber2DLike3D(shader, targetTemp, glm::vec3(SCREEN_X_LEFT, screenY, screenZ));
            drawNumber2DLike3D(shader, (int)std::round(currentTemp), glm::vec3(SCREEN_X_MID, screenY, screenZ));

            drawStatusIcon(texShader, P, V);

            // restore main shader after texShader.use()
            shader.use();
            shader.setMat4("uP", P);
            shader.setMat4("uV", V);
            shader.setVec3("uLightPos", 2.0f, 4.0f, 2.0f);
            shader.setVec3("uViewPos", camera.Position);
            shader.setVec3("uLightColor", 1.0f, 1.0f, 1.0f);
        }

        // basin
        shader.setVec3("uObjectColor", 0.25f, 0.55f, 0.95f);
        M = glm::translate(glm::mat4(1.0f), basinPos);
        M = glm::scale(M, glm::vec3(basinScale));
        shader.setMat4("uM", M); drawBasin();

        // water (follows basin)
        updateWaterMesh(64, waterLevel, basinPos.x, basinPos.y, basinPos.z);
        if (waterVertexCount > 0)
        {
            shader.setVec3("uObjectColor", 0.25f, 0.60f, 1.0f);
            shader.setMat4("uM", glm::mat4(1.0f));
            glBindVertexArray(waterVAO);
            glDrawArrays(GL_TRIANGLES, 0, waterVertexCount);
            glBindVertexArray(0);
        }

        // droplets
        drawDroplets(shader);

        // ===== Draw OBJ models (toilet + remote) =====
        applyModelCommonUniforms(modelShader, P, V);

        drawToiletModel(toilet, modelShader);
        if (!basinHeld) {
            drawRemoteModel(remoteM, modelShader);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
