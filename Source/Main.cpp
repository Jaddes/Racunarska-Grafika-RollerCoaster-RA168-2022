#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../Header/Util.h"

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
};

static float length(const Vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
static Vec2 normalize(const Vec2& v) {
    float len = length(v);
    if (len < 1e-6f) return { 0.0f, 0.0f };
    return { v.x / len, v.y / len };
}
static Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return a + (b - a) * t; }

struct Passenger {
    bool occupied = false;
    bool strapped = false;
    bool sick = false;
};

enum class RideState { Boarding, Riding, StoppedForSick, Returning };

struct Track {
    std::vector<Vec2> samples;
    std::vector<float> cumulative;
    float totalLength = 1.0f;
};

struct Car {
    Passenger seats[8]{};
    float param = 0.0f;  // 0..1 along track
    float speed = 0.0f;
    bool removalMode = false;
};

// Globals
GLFWwindow* window = nullptr;
int screenWidth = 1280;
int screenHeight = 720;
unsigned int colorShader = 0;
unsigned int textureShader = 0;
unsigned int quadVAO = 0;
unsigned int quadVBO = 0;
unsigned int trackVAO = 0;
unsigned int trackVBO = 0;
unsigned int passengerTex = 0;
unsigned int beltTex = 0;
unsigned int seatTex = 0;
unsigned int labelTex = 0;
unsigned int sunTex = 0;
unsigned int cloudTex = 0;
GLFWcursor* railCursor = nullptr;

Track track;
Car car;
RideState rideState = RideState::Boarding;

Vec2 carPos{ -0.8f, -0.9f };
Vec2 carDir{ 1.0f, 0.0f };
Vec2 railPos{ -0.8f, -0.9f };
Vec2 carTangent{ 1.0f, 0.0f };
Vec2 carNormal{ 0.0f, 1.0f };
float carAngle = 0.0f;

const float targetCruiseSpeed = 0.75f;
const float baseAcceleration = 0.90f;
const float slopeAcceleration = 1.30f;
const float stopDeceleration = 1.50f;
const float returnSpeed = 0.20f;
float sickStopTimer = 0.0f;

std::array<Vec2, 8> seatOffsets = {
    Vec2{ 0.09f, -0.04f }, Vec2{ 0.09f, 0.04f },
    Vec2{ 0.03f, -0.04f }, Vec2{ 0.03f, 0.04f },
    Vec2{ -0.03f, -0.04f }, Vec2{ -0.03f, 0.04f },
    Vec2{ -0.09f, -0.04f }, Vec2{ -0.09f, 0.04f }
};
std::array<int, 8> seatOrder = { 0, 1, 2, 3, 4, 5, 6, 7 };

// Forward decls
bool initGLFW();
bool initWindow();
bool initGLEW();
void initOpenGLState();
void createVAOs();
void createTrackGeometry();
void mainLoop();
void update(float dt);
void render();
void renderEnvironment();
void renderSkyGradient();
void renderSkyline();
void renderGroundSilhouette();
void renderClouds();
void renderTrackSilhouette();
void windowToOpenGL(double mx, double my, float& glx, float& gly);
void mouseClickCallback(GLFWwindow* window, int button, int action, int mods);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
Track buildTrack();
Vec2 catmullRom(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t);
void evaluateTrack(float t, Vec2& outPos, Vec2& outTangent);
void drawQuadColor(const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& color);
void drawQuadTexture(unsigned int tex, const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& tint);
std::vector<unsigned char> makeCircleTexture(int size, const std::array<unsigned char, 4>& fill);
std::vector<unsigned char> makeSeatbeltTexture(int size);
std::vector<unsigned char> makeSeatTexture(int size);
std::vector<unsigned char> makeRailCursorPixels(int size);
std::vector<unsigned char> makeLabelTexture(int width, int height, const std::string& text);
void addPassenger();
void toggleSeatStrap(int idx);
void removePassenger(int idx);
void tryStartRide();
void triggerSick(int idx);
bool allStrapped() ;
int nextFreeSeat();
void resetAfterReturn();
Vec2 seatWorldPosition(int idx);

bool initGLFW()
{
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    return true;
}

bool initWindow()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    screenWidth = mode->width;
    screenHeight = mode->height;

    window = glfwCreateWindow(screenWidth, screenHeight, "RollerCoaster - Boris Lahos RA 168/2022", monitor, NULL);
    if (!window) return false;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    return true;
}

bool initGLEW()
{
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return false;
    return true;
}

void initOpenGLState() {
    glViewport(0, 0, screenWidth, screenHeight);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void windowToOpenGL(double mx, double my, float& glx, float& gly)
{
    glx = float((mx / screenWidth) * 2.0 - 1.0);
    gly = float(1.0 - (my / screenHeight) * 2.0);
}

Track buildTrack()
{
    const float pi = 3.14159265f;
    auto smoothStep = [&](float t) {
        return 0.5f - 0.5f * std::cos(t * pi);
    };
    auto addSmoothSegment = [&](Track& tr, const Vec2& a, const Vec2& b, int steps) {
        for (int i = 1; i <= steps; ++i) {
            float u = float(i) / float(steps);
            float w = smoothStep(u);
            Vec2 p = lerp(a, b, w);
            tr.samples.push_back(p);
        }
    };
    auto addLoop = [&](Track& tr, const Vec2& center, float radius, float drift, float startAngle, float endAngle, int steps) {
        for (int i = 1; i <= steps; ++i) {
            float u = float(i) / float(steps);
            float theta = startAngle + (endAngle - startAngle) * u;
            float x = center.x + radius * std::cos(theta) + drift * theta;
            float y = center.y + radius * std::sin(theta);
            tr.samples.push_back({ x, y });
        }
    };

    Track t;
    Vec2 start = { -0.95f, -0.86f };
    t.samples.push_back(start);

    // Visible start line near ground.
    addSmoothSegment(t, start, { -0.82f, -0.86f }, 35);

    addSmoothSegment(t, { -0.82f, -0.86f }, { -0.70f, 0.54f }, 95);   // Tall left hill
    addSmoothSegment(t, { -0.70f, 0.54f }, { -0.58f, -0.72f }, 90);   // Drop back to ground
    addSmoothSegment(t, { -0.58f, -0.72f }, { -0.46f, -0.26f }, 80);  // Small rise before loop
    addSmoothSegment(t, { -0.46f, -0.26f }, { -0.30f, -0.44f }, 65);

    // Vertical loop with a slight horizontal drift so entry and exit are offset.
    const float loopRadius = 0.22f;
    const float loopDrift = 0.06f;
    const float loopStart = -pi * 0.5f;
    const float loopEnd = loopStart + 2.0f * pi;
    Vec2 loopCenter = { -0.12f, -0.25f };
    Vec2 loopEntry = { loopCenter.x + loopRadius * std::cos(loopStart) + loopDrift * loopStart,
                       loopCenter.y + loopRadius * std::sin(loopStart) };
    addSmoothSegment(t, { -0.31f, -0.44f }, loopEntry, 45);
    addLoop(t, loopCenter, loopRadius, loopDrift, loopStart, loopEnd, 220);
    Vec2 loopExit = { loopCenter.x + loopRadius * std::cos(loopEnd) + loopDrift * loopEnd,
                      loopCenter.y + loopRadius * std::sin(loopEnd) };

    addSmoothSegment(t, loopExit, { 0.32f, 0.32f }, 100); // Climb after loop
    addSmoothSegment(t, { 0.32f, 0.32f }, { 0.48f, -0.60f }, 95); // Big valley
    addSmoothSegment(t, { 0.48f, -0.60f }, { 0.70f, -0.05f }, 80); // Final rolling hill
    addSmoothSegment(t, { 0.70f, -0.05f }, { 0.96f, -0.86f }, 105); // Gentle drop to exit

    // Lift and nudge the entire track so the cart and all seats are fully visible at start.
    for (auto& p : t.samples) {
        p.y += 0.12f;
        p.x += 0.12f;
    }

    t.cumulative.resize(t.samples.size(), 0.0f);
    for (size_t i = 1; i < t.samples.size(); ++i) {
        t.cumulative[i] = t.cumulative[i - 1] + length(t.samples[i] - t.samples[i - 1]);
    }
    t.totalLength = std::max(0.001f, t.cumulative.empty() ? 0.0f : t.cumulative.back());
    return t;
}

Vec2 catmullRom(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    float x = 0.5f * ((2 * p1.x) + (-p0.x + p2.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 + (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
    float y = 0.5f * ((2 * p1.y) + (-p0.y + p2.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 + (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
    return { x, y };
}

void evaluateTrack(float t, Vec2& outPos, Vec2& outTangent)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float target = t * track.totalLength;
    size_t idx = 0;
    while (idx + 1 < track.cumulative.size() && track.cumulative[idx + 1] < target) idx++;
    size_t next = std::min(idx + 1, track.samples.size() - 1);
    float segLen = track.cumulative[next] - track.cumulative[idx];
    float localT = segLen > 0.0f ? (target - track.cumulative[idx]) / segLen : 0.0f;
    outPos = lerp(track.samples[idx], track.samples[next], localT);
    outTangent = normalize(track.samples[next] - track.samples[idx]);
}

void createVAOs()
{
    float quadVertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glGenVertexArrays(1, &trackVAO);
    glGenBuffers(1, &trackVBO);
    glBindVertexArray(trackVAO);
    glBindBuffer(GL_ARRAY_BUFFER, trackVBO);
    glBufferData(GL_ARRAY_BUFFER, track.samples.size() * sizeof(Vec2), track.samples.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void createTrackGeometry()
{
    glBindBuffer(GL_ARRAY_BUFFER, trackVBO);
    glBufferData(GL_ARRAY_BUFFER, track.samples.size() * sizeof(Vec2), track.samples.data(), GL_STATIC_DRAW);
}

std::vector<unsigned char> makeCircleTexture(int size, const std::array<unsigned char, 4>& fill)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    Vec2 center = { size * 0.5f, size * 0.5f };
    float radius = size * 0.45f;
    float radius2 = radius * radius;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - center.x;
            float dy = y - center.y;
            float dist2 = dx * dx + dy * dy;
            if (dist2 <= radius2) {
                int idx = (y * size + x) * 4;
                data[idx + 0] = fill[0];
                data[idx + 1] = fill[1];
                data[idx + 2] = fill[2];
                data[idx + 3] = fill[3];
            }
        }
    }
    return data;
}

std::vector<unsigned char> makeSeatbeltTexture(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (y > x - 4 && y < x + 4) {
                int idx = (y * size + x) * 4;
                data[idx + 0] = 40;
                data[idx + 1] = 40;
                data[idx + 2] = 40;
                data[idx + 3] = 220;
            }
        }
    }
    return data;
}

std::vector<unsigned char> makeSeatTexture(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            bool border = x < 2 || x > size - 3 || y < 2 || y > size - 3;
            if (border) {
                data[idx + 0] = 110;
                data[idx + 1] = 20;
                data[idx + 2] = 25;
                data[idx + 3] = 230;
            } else {
                data[idx + 0] = 150;
                data[idx + 1] = 35;
                data[idx + 2] = 45;
                data[idx + 3] = 220;
            }
        }
    }
    return data;
}

std::unordered_map<char, std::array<uint8_t, 7>> fontGlyphs()
{
    return {
        { 'A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
        { 'B',{0b11110,0b10001,0b11110,0b10001,0b10001,0b10001,0b11110} },
        { 'C',{0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110} },
        { 'D',{0b11100,0b10010,0b10001,0b10001,0b10001,0b10010,0b11100} },
        { 'E',{0b11111,0b10000,0b11100,0b10000,0b10000,0b10000,0b11111} },
        { 'F',{0b11111,0b10000,0b11100,0b10000,0b10000,0b10000,0b10000} },
        { 'G',{0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110} },
        { 'H',{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
        { 'I',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111} },
        { 'J',{0b00111,0b00010,0b00010,0b00010,0b10010,0b10010,0b01100} },
        { 'K',{0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001} },
        { 'L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111} },
        { 'M',{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001} },
        { 'N',{0b10001,0b11001,0b10101,0b10101,0b10011,0b10001,0b10001} },
        { 'O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
        { 'P',{0b11110,0b10001,0b11110,0b10000,0b10000,0b10000,0b10000} },
        { 'Q',{0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101} },
        { 'R',{0b11110,0b10001,0b11110,0b10001,0b10001,0b10001,0b10001} },
        { 'S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110} },
        { 'T',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100} },
        { 'U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
        { 'V',{0b10001,0b10001,0b10001,0b10001,0b01010,0b01010,0b00100} },
        { 'W',{0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001} },
        { 'X',{0b10001,0b01010,0b00100,0b00100,0b00100,0b01010,0b10001} },
        { 'Y',{0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100} },
        { 'Z',{0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111} },
        { ' ',{0,0,0,0,0,0,0} },
        { '/',{0b00001,0b00010,0b00100,0b01000,0b10000,0,0} },
        { '0',{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110} },
        { '1',{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110} },
        { '2',{0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111} },
        { '3',{0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110} },
        { '4',{0b10010,0b10010,0b10010,0b11111,0b00010,0b00010,0b00010} },
        { '5',{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110} },
        { '6',{0b01110,0b10000,0b11110,0b10001,0b10001,0b10001,0b01110} },
        { '7',{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000} },
        { '8',{0b01110,0b10001,0b01110,0b10001,0b10001,0b10001,0b01110} },
        { '9',{0b01110,0b10001,0b10001,0b01111,0b00001,0b00001,0b11110} },
        { '-',{0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000} },
        { ':',{0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000} },
    };
}

std::vector<unsigned char> makeLabelTexture(int width, int height, const std::string& text)
{
    std::vector<unsigned char> data(width * height * 4, 0);
    // semi-transparent background
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            data[idx + 0] = 20;
            data[idx + 1] = 24;
            data[idx + 2] = 32;
            data[idx + 3] = 200;
        }
    }

    auto glyphs = fontGlyphs();
    int scale = 2;
    int lineHeight = 7 * scale + 6;
    int marginX = 16;
    int cursorX = marginX;
    int cursorY = 32;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
        if (c == '\n') {
            cursorX = marginX;
            cursorY += lineHeight;
            continue;
        }
        if (glyphs.find(c) == glyphs.end()) continue;
        const auto& rows = glyphs[c];
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1 << (4 - col))) {
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            int px = cursorX + col * scale + sx;
                            int py = cursorY + row * scale + sy;
                            if (px >= 0 && px < width && py >= 0 && py < height) {
                                int idx = (py * width + px) * 4;
                                data[idx + 0] = 235;
                                data[idx + 1] = 235;
                                data[idx + 2] = 245;
                                data[idx + 3] = 255;
                            }
                        }
                    }
                }
            }
        }
        cursorX += 6 * scale;
    }
    return data;
}

std::vector<unsigned char> makeRailCursorPixels(int size)
{
    std::vector<unsigned char> data(size * size * 4, 0);
    // Simple arrow pointing up-left. Transparent background, light body.
    auto setPix = [&](int x, int y, std::array<unsigned char, 4> rgba) {
        if (x < 0 || x >= size || y < 0 || y >= size) return;
        int idx = (y * size + x) * 4;
        data[idx + 0] = rgba[0];
        data[idx + 1] = rgba[1];
        data[idx + 2] = rgba[2];
        data[idx + 3] = rgba[3];
    };

    std::array<unsigned char, 4> body = { 235, 235, 235, 255 };
    // Arrow head (triangle)
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x <= y; ++x) {
            if (x <= 12 && y <= 20) setPix(x, y, body);
        }
    }
    // Stem
    for (int y = 8; y < 24; ++y) {
        for (int x = 8; x < 11; ++x) {
            setPix(x, y, body);
        }
    }
    return data;
}

void addPassenger()
{
    if (rideState != RideState::Boarding || car.removalMode) return;
    int idx = nextFreeSeat();
    if (idx < 0) return;
    car.seats[idx].occupied = true;
    car.seats[idx].strapped = false;
    car.seats[idx].sick = false;
}

void toggleSeatStrap(int idx)
{
    if (rideState != RideState::Boarding || car.removalMode) return;
    if (idx < 0 || idx >= 8) return;
    if (!car.seats[idx].occupied) return;
    car.seats[idx].strapped = !car.seats[idx].strapped;
}

void removePassenger(int idx)
{
    if (!car.removalMode || rideState != RideState::Boarding) return;
    if (idx < 0 || idx >= 8) return;
    car.seats[idx] = Passenger{};
    bool any = false;
    for (const auto& s : car.seats) any = any || s.occupied;
    if (!any) car.removalMode = false;
}

bool allStrapped()
{
    bool any = false;
    for (const auto& s : car.seats) {
        if (s.occupied) {
            any = true;
            if (!s.strapped) return false;
        }
    }
    return any;
}

int nextFreeSeat()
{
    for (int idx : seatOrder) {
        if (!car.seats[idx].occupied) return idx;
    }
    return -1;
}

void tryStartRide()
{
    if (rideState != RideState::Boarding) return;
    if (!allStrapped()) return;
    rideState = RideState::Riding;
    sickStopTimer = 0.0f;
    car.speed = 0.0f;
}

void triggerSick(int idx)
{
    if (rideState != RideState::Riding) return;
    if (idx < 0 || idx >= 8) return;
    if (!car.seats[idx].occupied) return;
    car.seats[idx].sick = true;
    rideState = RideState::StoppedForSick;
    sickStopTimer = 0.0f;
}

void resetAfterReturn()
{
    for (auto& s : car.seats) {
        s.strapped = false;
        s.sick = false;
    }
    rideState = RideState::Boarding;
    car.speed = 0.0f;
    car.param = 0.0f;
    car.removalMode = true;
}

Vec2 seatWorldPosition(int idx)
{
    Vec2 offset = seatOffsets[idx];
    float c = std::cos(carAngle);
    float s = std::sin(carAngle);
    Vec2 rotated = { offset.x * c - offset.y * s, offset.x * s + offset.y * c };
    return carPos + rotated;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_SPACE) addPassenger();
    if (key == GLFW_KEY_ENTER) tryStartRide();
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_8) {
        int idx = key - GLFW_KEY_1;
        triggerSick(idx);
    }
}

void mouseClickCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    float gx, gy;
    windowToOpenGL(mx, my, gx, gy);
    for (int i = 0; i < 8; ++i) {
        Vec2 pos = seatWorldPosition(i);
        float dist = length({ gx - pos.x, gy - pos.y });
        if (dist < 0.06f) {
            if (car.removalMode) removePassenger(i);
            else toggleSeatStrap(i);
            break;
        }
    }
}

void update(float dt)
{
    if (rideState == RideState::Riding) {
        car.speed = std::min(targetCruiseSpeed, car.speed + baseAcceleration * dt);
        float slope = carDir.y;
        car.speed += (-slope) * slopeAcceleration * dt;
        car.speed = std::clamp(car.speed, 0.05f, 0.8f);
        car.param += (car.speed / track.totalLength) * dt;
        if (car.param >= 1.0f) {
            rideState = RideState::Returning;
            car.speed = returnSpeed;
        }
    }
    else if (rideState == RideState::StoppedForSick) {
        if (car.speed > 0.01f) {
            car.param += (car.speed / track.totalLength) * dt;
            car.param = std::min(1.0f, car.param);
            car.speed = std::max(0.0f, car.speed - stopDeceleration * dt);
        }
        else {
            sickStopTimer += dt;
            if (sickStopTimer >= 10.0f) {
                rideState = RideState::Returning;
                car.speed = returnSpeed;
            }
        }
    }
    else if (rideState == RideState::Returning) {
        car.param -= (returnSpeed / track.totalLength) * dt;
        car.param = std::max(0.0f, car.param);
        if (car.param <= 0.0f) {
            resetAfterReturn();
        }
    }

    evaluateTrack(car.param, railPos, carDir);
    carTangent = normalize(carDir);
    carNormal = { -carTangent.y, carTangent.x };
    carAngle = std::atan2(carTangent.y, carTangent.x);
    float cartNormalOffset = 0.0f; // keep cart centered on rail
    carPos = railPos + carNormal * cartNormalOffset;
}

void drawQuadColor(const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& color)
{
    glUseProgram(colorShader);
    glUniform2f(glGetUniformLocation(colorShader, "uPos"), pos.x, pos.y);
    glUniform2f(glGetUniformLocation(colorShader, "uSize"), size.x, size.y);
    glUniform1f(glGetUniformLocation(colorShader, "uRotation"), rot);
    glUniform4f(glGetUniformLocation(colorShader, "uColor"), color[0], color[1], color[2], color[3]);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void drawQuadTexture(unsigned int tex, const Vec2& pos, const Vec2& size, float rot, const std::array<float, 4>& tint)
{
    glUseProgram(textureShader);
    glUniform2f(glGetUniformLocation(textureShader, "uPos"), pos.x, pos.y);
    glUniform2f(glGetUniformLocation(textureShader, "uSize"), size.x, size.y);
    glUniform1f(glGetUniformLocation(textureShader, "uRotation"), rot);
    glUniform4f(glGetUniformLocation(textureShader, "uTint"), tint[0], tint[1], tint[2], tint[3]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(textureShader, "uTex"), 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void renderTrackSilhouette()
{
    const std::array<float, 4> railColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    const std::array<float, 4> innerShade = { 0.03f, 0.03f, 0.03f, 1.0f };
    const std::array<float, 4> tieColor = { 0.97f, 0.85f, 0.58f, 1.0f };
    const float trackWidth = 0.072f;
    const float groundY = -0.80f;

    // Supports
    for (float d = 0.0f; d <= track.totalLength; d += 0.12f) {
        float t = d / track.totalLength;
        Vec2 pos, tan;
        evaluateTrack(t, pos, tan);
        float topY = pos.y - trackWidth * 0.55f;
        float height = topY - groundY;
        if (height < 0.02f) continue;
        Vec2 center = { pos.x, groundY + height * 0.5f };
        drawQuadColor(center, { 0.014f, height }, 0.0f, railColor);
    }

    auto drawBrace = [&](const Vec2& a, const Vec2& b) {
        Vec2 mid = (a + b) * 0.5f;
        float len = length(b - a);
        float ang = std::atan2(b.y - a.y, b.x - a.x);
        drawQuadColor(mid, { len, 0.016f }, ang, railColor);
    };
    drawBrace({ -0.66f, groundY }, { -0.44f, -0.18f });
    drawBrace({ -0.18f, groundY }, { -0.05f, -0.18f });
    drawBrace({ 0.18f, groundY }, { 0.34f, -0.08f });
    drawBrace({ 0.54f, groundY }, { 0.70f, -0.16f });

    // Base rail ribbon
    for (size_t i = 0; i + 1 < track.samples.size(); ++i) {
        Vec2 p0 = track.samples[i];
        Vec2 p1 = track.samples[i + 1];
        float segLen = length(p1 - p0);
        if (segLen < 1e-5f) continue;
        Vec2 mid = (p0 + p1) * 0.5f;
        float ang = std::atan2(p1.y - p0.y, p1.x - p0.x);
        drawQuadColor(mid, { segLen, trackWidth }, ang, railColor);
        drawQuadColor(mid, { segLen, trackWidth * 0.55f }, ang, innerShade);
    }

    // Track ties (segment markers)
    float tieSpacing = 0.060f;
    if (track.totalLength > 0.0f) {
        for (float d = 0.0f; d <= track.totalLength; d += tieSpacing) {
            float t = d / track.totalLength;
            Vec2 pos, tan;
            evaluateTrack(t, pos, tan);
            float ang = std::atan2(tan.y, tan.x);
            drawQuadColor(pos, { 0.016f, trackWidth * 0.52f }, ang, tieColor);
            drawQuadColor(pos, { 0.020f, trackWidth * 0.22f }, ang, railColor);
        }
    }

}

void renderCar()
{
    float angle = carAngle;
    Vec2 cartSize = { 0.35f, 0.24f };
    drawQuadColor(carPos, cartSize, angle, { 0.80f, 0.12f, 0.15f, 0.96f });

    for (int i = 0; i < 8; ++i) {
        Vec2 sPos = seatWorldPosition(i);
        drawQuadTexture(seatTex, sPos, { 0.08f, 0.08f }, angle, { 1.0f, 1.0f, 1.0f, 0.9f });
        if (car.seats[i].occupied) {
            std::array<float, 4> tint = car.seats[i].sick ? std::array<float, 4>{ 0.5f, 1.0f, 0.5f, 1.0f } : std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f };
            drawQuadTexture(passengerTex, sPos, { 0.07f, 0.07f }, angle, tint);
        }
        if (car.seats[i].strapped && car.seats[i].occupied) {
            drawQuadTexture(beltTex, sPos, { 0.09f, 0.09f }, angle, { 1.0f, 1.0f, 1.0f, 0.85f });
        }
    }
}

void renderLabel()
{
    drawQuadTexture(labelTex, { 0.0f, 0.83f }, { 1.6f, 0.30f }, 0.0f, { 1.0f,1.0f,1.0f,1.0f });
}

void renderSkyGradient()
{
    drawQuadColor({ 0.0f, 0.80f }, { 2.4f, 0.9f }, 0.0f, { 0.97f, 0.63f, 0.24f, 1.0f });
    drawQuadColor({ 0.0f, 0.30f }, { 2.4f, 0.9f }, 0.0f, { 0.99f, 0.74f, 0.32f, 1.0f });
    drawQuadColor({ 0.0f, -0.10f }, { 2.4f, 0.9f }, 0.0f, { 1.00f, 0.86f, 0.54f, 1.0f });
}

void drawBench(const Vec2& pos, float scale)
{
    std::array<float, 4> c = { 0.0f, 0.0f, 0.0f, 1.0f };
    drawQuadColor(pos + Vec2{ 0.0f, -0.01f * scale }, { 0.14f * scale, 0.02f * scale }, 0.0f, c); // seat
    drawQuadColor(pos + Vec2{ -0.05f * scale, -0.05f * scale }, { 0.012f * scale, 0.08f * scale }, 0.0f, c); // left leg
    drawQuadColor(pos + Vec2{ 0.05f * scale, -0.05f * scale }, { 0.012f * scale, 0.08f * scale }, 0.0f, c); // right leg
    drawQuadColor(pos + Vec2{ 0.0f, 0.04f * scale }, { 0.14f * scale, 0.02f * scale }, 0.0f, c); // backrest
}

void drawTree(const Vec2& pos, float scale)
{
    std::array<float, 4> black = { 0.0f, 0.0f, 0.0f, 1.0f };
    drawQuadColor(pos + Vec2{ 0.0f, -0.06f * scale }, { 0.022f * scale, 0.12f * scale }, 0.0f, black); // trunk
    drawQuadColor(pos + Vec2{ 0.0f, 0.04f * scale }, { 0.14f * scale, 0.12f * scale }, 0.75f, black);
    drawQuadColor(pos + Vec2{ 0.0f, 0.09f * scale }, { 0.12f * scale, 0.10f * scale }, -0.75f, black);
}

void renderSkyline()
{
    const std::array<float, 4> shade = { 0.06f, 0.06f, 0.08f, 1.0f };
    float baseY = -0.58f;
    struct Building { float x; float width; float height; float extra; };
    std::array<Building, 18> buildings = { {
        { -0.95f, 0.10f, 0.32f, 0.02f },
        { -0.82f, 0.08f, 0.26f, 0.00f },
        { -0.72f, 0.12f, 0.30f, 0.06f },
        { -0.60f, 0.06f, 0.18f, 0.04f },
        { -0.52f, 0.08f, 0.24f, 0.05f },
        { -0.42f, 0.10f, 0.28f, 0.00f },
        { -0.30f, 0.07f, 0.22f, 0.08f },
        { -0.20f, 0.12f, 0.34f, 0.04f },
        { -0.08f, 0.10f, 0.20f, 0.10f },
        { 0.05f, 0.08f, 0.25f, 0.05f },
        { 0.16f, 0.12f, 0.36f, 0.02f },
        { 0.30f, 0.10f, 0.30f, 0.06f },
        { 0.44f, 0.08f, 0.22f, 0.04f },
        { 0.56f, 0.14f, 0.34f, 0.04f },
        { 0.72f, 0.12f, 0.30f, 0.08f },
        { 0.86f, 0.08f, 0.22f, 0.02f },
        { 0.98f, 0.08f, 0.26f, 0.00f },
        { -0.02f, 0.06f, 0.18f, 0.12f }
    } };
    for (const auto& b : buildings) {
        Vec2 center = { b.x, baseY + b.height * 0.5f };
        drawQuadColor(center, { b.width, b.height + b.extra }, 0.0f, shade);
    }

    // A few antennas and spires to break up the roofline.
    std::array<Vec2, 5> spires = { Vec2{ -0.70f, -0.22f }, Vec2{ -0.35f, -0.16f }, Vec2{ 0.16f, -0.20f }, Vec2{ 0.44f, -0.18f }, Vec2{ 0.70f, -0.20f } };
    for (const auto& s : spires) {
        drawQuadColor(s, { 0.006f, 0.24f }, 0.0f, shade);
    }
}

void renderGroundSilhouette()
{
    std::array<float, 4> black = { 0.0f, 0.0f, 0.0f, 1.0f };
    drawQuadColor({ 0.0f, -0.80f }, { 2.4f, 0.38f }, 0.0f, black);
    drawQuadColor({ 0.0f, -0.60f }, { 2.4f, 0.04f }, 0.0f, black);

    drawBench({ -0.52f, -0.68f }, 1.0f);
    drawBench({ 0.40f, -0.70f }, 1.0f);

    drawTree({ -0.78f, -0.62f }, 1.0f);
    drawTree({ 0.82f, -0.62f }, 0.85f);
}

void renderClouds()
{
    drawQuadTexture(cloudTex, { -0.60f, 0.72f }, { 0.30f, 0.12f }, 0.0f, { 1.0f, 1.0f, 1.0f, 0.92f });
    drawQuadTexture(cloudTex, { 0.28f, 0.76f }, { 0.26f, 0.11f }, 0.0f, { 1.0f, 1.0f, 1.0f, 0.88f });
}

void renderEnvironment()
{
    renderSkyGradient();
    renderSkyline();
    renderClouds();
    renderGroundSilhouette();
}

void render()
{
    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderEnvironment();
    renderTrackSilhouette();
    renderCar();
    renderLabel();
}

void mainLoop()
{
    const double targetFrame = 1.0 / 75.0;
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;

        update(dt);
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();

        double frameTime = glfwGetTime() - now;
        if (frameTime < targetFrame) {
            std::this_thread::sleep_for(std::chrono::duration<double>(targetFrame - frameTime));
        }
    }
}

int main()
{
    if (!initGLFW()) return endProgram("GLFW init failed.");
    if (!initWindow()) return endProgram("Window creation failed.");
    if (!initGLEW()) return endProgram("GLEW init failed.");

    glfwSetMouseButtonCallback(window, mouseClickCallback);
    glfwSetKeyCallback(window, keyCallback);

    track = buildTrack();
    createVAOs();
    createTrackGeometry();

    colorShader = createShader("Source/Shaders/color.vert", "Source/Shaders/color.frag");
    textureShader = createShader("Source/Shaders/texture.vert", "Source/Shaders/texture.frag");

    passengerTex = createTextureFromRGBA(makeCircleTexture(64, { 230, 200, 120, 255 }), 64, 64);
    beltTex = createTextureFromRGBA(makeSeatbeltTexture(64), 64, 64);
    seatTex = createTextureFromRGBA(makeSeatTexture(64), 64, 64);
    labelTex = createTextureFromRGBA(
        makeLabelTexture(1024, 240,
            "BORIS LAHOS RA 168/2022\n\n"
            "SPACE  - ADD PASSENGER TO CAR\n"
            "CLICK  - TOGGLE BELT / REMOVE WHEN RETURNED\n"
            "ENTER  - START RIDE\n"
            "1-8    - MAKE PASSENGER SICK DURING RIDE\n"
            "ESC    - EXIT PROGRAM"),
        1024, 240);
    sunTex = createTextureFromRGBA(makeCircleTexture(128, { 250, 210, 80, 255 }), 128, 128);
    cloudTex = createTextureFromRGBA(makeCircleTexture(128, { 230, 230, 240, 220 }), 128, 128);

    auto railPixels = makeRailCursorPixels(32);
    GLFWimage img;
    img.width = 32;
    img.height = 32;
    img.pixels = railPixels.data();
    railCursor = glfwCreateCursor(&img, 4, 4);
    if (railCursor) glfwSetCursor(window, railCursor);

    initOpenGLState();
    mainLoop();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
