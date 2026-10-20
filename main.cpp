#include <windows.h>
#include <mmsystem.h>
#include <GL/glut.h>
#include <math.h>
#include <stdlib.h>

#pragma comment(lib, "winmm.lib")

#define PI 3.14159265358979323846

int   windowW = 900, windowH = 650;
float t = 0.0f;

float dayPhase = 0.02f;
float phaseSpeed = 0.00045f;

const int NUM_BIRDS = 5;
float birdX[NUM_BIRDS]     = { -1.3f, -1.6f, -1.9f, -2.3f, -2.7f };
float birdY[NUM_BIRDS]     = {  0.62f,  0.55f,  0.68f,  0.50f,  0.60f };
float birdSpeed[NUM_BIRDS] = {  0.006f, 0.005f, 0.007f, 0.0045f, 0.0065f };

float boatX      = 0.9f;
float boatSpeed  = 0.0022f;
float boatBob    = 0.0f;

float boat2X     = -0.9f;
float boat2Speed = 0.0018f;
float boat2Bob   = 0.0f;

const int NUM_STARS = 80;
float starX[NUM_STARS], starY[NUM_STARS], starTwinkle[NUM_STARS];

float lerp(float a, float b, float f) { return a + (b - a) * f; }

void drawCircle(float cx, float cy, float radius, int segments,
                 float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= segments; i++) {
            float ang = i * 2.0f * PI / segments;
            glVertex2f(cx + radius * cosf(ang), cy + radius * sinf(ang));
        }
    glEnd();
}

void drawGlow(float cx, float cy, float radius, float r, float g, float b)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    int rings = 6;
    for (int i = rings; i >= 1; i--) {
        float scale = 1.0f + i * 0.35f;
        float alpha = 0.09f * (rings - i + 1) / rings;
        glColor4f(r, g, b, alpha);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for (int k = 0; k <= 40; k++) {
                float ang = k * 2.0f * PI / 40;
                glVertex2f(cx + radius * scale * cosf(ang),
                           cy + radius * scale * sinf(ang));
            }
        glEnd();
    }
    glDisable(GL_BLEND);
}

void drawEllipse(float cx, float cy, float rx, float ry, int segments,
                  float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= segments; i++) {
            float ang = i * 2.0f * PI / segments;
            glVertex2f(cx + rx * cosf(ang), cy + ry * sinf(ang));
        }
    glEnd();
}

void drawBird(float x, float y, float wingSpread, float alpha)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.05f, 0.05f, 0.08f, alpha);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - wingSpread,        y);
        glVertex2f(x - wingSpread * 0.4f, y + wingSpread * 0.5f);
        glVertex2f(x,                     y);
        glVertex2f(x + wingSpread * 0.4f, y + wingSpread * 0.5f);
        glVertex2f(x + wingSpread,        y);
    glEnd();
    glDisable(GL_BLEND);
}

void drawThickSegment(float x1, float y1, float x2, float y2,
                       float thickness, float r, float g, float b)
{
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    float px = -dy / len * thickness * 0.5f;
    float py =  dx / len * thickness * 0.5f;

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x1 + px, y1 + py);
        glVertex2f(x1 - px, y1 - py);
        glVertex2f(x2 - px, y2 - py);
        glVertex2f(x2 + px, y2 + py);
    glEnd();
}

float cyclicDist(float a, float b)
{
    float d = fabsf(a - b);
    if (d > 0.5f) d = 1.0f - d;
    return d;
}

void computeWeights(float phase, float &wDay, float &wSunset, float &wNight)
{
    float dDay     = cyclicDist(phase, 0.0f);
    float dSunsetA = cyclicDist(phase, 0.25f);
    float dSunsetB = cyclicDist(phase, 0.75f);
    float dSunset  = (dSunsetA < dSunsetB) ? dSunsetA : dSunsetB;
    float dNight   = cyclicDist(phase, 0.5f);

    wDay    = 1.0f - dDay    / 0.25f; if (wDay    < 0.0f) wDay    = 0.0f;
    wSunset = 1.0f - dSunset / 0.25f; if (wSunset < 0.0f) wSunset = 0.0f;
    wNight  = 1.0f - dNight  / 0.25f; if (wNight  < 0.0f) wNight  = 0.0f;

    float sum = wDay + wSunset + wNight;
    if (sum > 0.0001f) { wDay /= sum; wSunset /= sum; wNight /= sum; }
}

float wDay, wSunset, wNight;
float skyTopR, skyTopG, skyTopB, skyBotR, skyBotG, skyBotB;
float seaTopR, seaTopG, seaTopB, seaBotR, seaBotG, seaBotB;
float sandTopR, sandTopG, sandTopB, sandBotR, sandBotG, sandBotB;
float cloudR, cloudG, cloudB;
float foamShade;

void computePalette()
{
    computeWeights(dayPhase, wDay, wSunset, wNight);

    skyTopR = wDay*0.25f + wSunset*0.10f + wNight*0.01f;
    skyTopG = wDay*0.55f + wSunset*0.12f + wNight*0.01f;
    skyTopB = wDay*0.95f + wSunset*0.35f + wNight*0.06f;
    skyBotR = wDay*0.65f + wSunset*1.00f + wNight*0.05f;
    skyBotG = wDay*0.85f + wSunset*0.55f + wNight*0.07f;
    skyBotB = wDay*1.00f + wSunset*0.20f + wNight*0.20f;

    seaTopR = wDay*0.10f + wSunset*0.55f + wNight*0.05f;
    seaTopG = wDay*0.55f + wSunset*0.35f + wNight*0.10f;
    seaTopB = wDay*0.85f + wSunset*0.35f + wNight*0.25f;
    seaBotR = wDay*0.02f + wSunset*0.02f + wNight*0.01f;
    seaBotG = wDay*0.30f + wSunset*0.15f + wNight*0.03f;
    seaBotB = wDay*0.55f + wSunset*0.35f + wNight*0.10f;

    sandTopR = wDay*0.93f + wSunset*0.93f + wNight*0.25f;
    sandTopG = wDay*0.80f + wSunset*0.80f + wNight*0.22f;
    sandTopB = wDay*0.55f + wSunset*0.55f + wNight*0.18f;
    sandBotR = wDay*0.80f + wSunset*0.80f + wNight*0.15f;
    sandBotG = wDay*0.65f + wSunset*0.65f + wNight*0.12f;
    sandBotB = wDay*0.40f + wSunset*0.40f + wNight*0.10f;

    float notNight = wDay + wSunset;
    cloudR = lerp(0.12f, 1.0f, notNight);
    cloudG = lerp(0.12f, 1.0f, notNight);
    cloudB = lerp(0.20f, 1.0f, notNight);

    foamShade = lerp(0.55f, 1.0f, notNight);
}

void init()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    for (int i = 0; i < NUM_STARS; i++) {
        starX[i] = -1.0f + (rand() % 2000) / 1000.0f;
        starY[i] =  0.05f + (rand() % 950) / 1000.0f;
        starTwinkle[i] = (rand() % 100) / 100.0f;
    }
}

void drawSky()
{
    glBegin(GL_QUADS);
        glColor3f(skyTopR, skyTopG, skyTopB); glVertex2f(-1.0f, 1.0f);
        glColor3f(skyTopR, skyTopG, skyTopB); glVertex2f( 1.0f, 1.0f);
        glColor3f(skyBotR, skyBotG, skyBotB); glVertex2f( 1.0f, 0.0f);
        glColor3f(skyBotR, skyBotG, skyBotB); glVertex2f(-1.0f, 0.0f);
    glEnd();

    if (wNight > 0.02f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < NUM_STARS; i++) {
            float twinkle = 0.5f + 0.5f * sinf(t * 3.0f + starTwinkle[i] * 20.0f);
            glColor4f(twinkle, twinkle, twinkle, wNight);
            glVertex2f(starX[i], starY[i]);
        }
        glEnd();
        glDisable(GL_BLEND);
    }
}

void drawSunOrMoon()
{
    float sunX = 0.15f, sunY = 0.08f, sunR = 0.24f;
    float moonX = 0.05f, moonY = 0.55f, moonR = 0.11f;

    float cx = lerp(sunX, moonX, wNight);
    float cy = lerp(sunY, moonY, wNight);
    float radius = lerp(sunR, moonR, wNight);

    float warmR = 1.00f, warmG = 0.78f, warmB = 0.15f;
    float coolR = 0.95f, coolG = 0.96f, coolB = 0.90f;
    float bodyR = lerp(warmR, coolR, wNight);
    float bodyG = lerp(warmG, coolG, wNight);
    float bodyB = lerp(warmB, coolB, wNight);

    float glowWarmR = 1.00f, glowWarmG = 0.70f, glowWarmB = 0.20f;
    float glowCoolR = 0.85f, glowCoolG = 0.90f, glowCoolB = 1.00f;
    float glowR = lerp(glowWarmR, glowCoolR, wNight);
    float glowG = lerp(glowWarmG, glowCoolG, wNight);
    float glowB = lerp(glowWarmB, glowCoolB, wNight);

    drawGlow(cx, cy, radius, glowR, glowG, glowB);
    drawCircle(cx, cy, radius, 60, bodyR, bodyG, bodyB);

    if (wNight > 0.5f) {
        float ca = (wNight - 0.5f) * 2.0f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.85f, 0.86f, 0.80f, ca);
        drawCircle(cx - radius*0.27f, cy + radius*0.18f, radius*0.18f, 20, 0.85f, 0.86f, 0.80f);
        drawCircle(cx + radius*0.36f, cy - radius*0.27f, radius*0.14f, 20, 0.85f, 0.86f, 0.80f);
        glDisable(GL_BLEND);
    }
}

void drawClouds()
{
    float r = cloudR, g = cloudG, b = cloudB;

    float drift = sinf(t * 0.15f) * 0.02f;

    float cx1 = -0.55f + drift, cy1 = 0.72f;
    drawEllipse(cx1,        cy1,        0.14f, 0.06f, 30, r, g, b);
    drawEllipse(cx1 - 0.11f,cy1 - 0.015f,0.09f, 0.045f,30, r, g, b);
    drawEllipse(cx1 + 0.12f,cy1 - 0.01f, 0.10f, 0.05f, 30, r, g, b);

    float cx2 = 0.55f - drift, cy2 = 0.80f;
    drawEllipse(cx2,        cy2,        0.11f, 0.05f, 30, r, g, b);
    drawEllipse(cx2 - 0.09f,cy2 - 0.01f, 0.07f, 0.035f,30, r, g, b);
    drawEllipse(cx2 + 0.09f,cy2 - 0.01f, 0.07f, 0.035f,30, r, g, b);

    float cx3 = -0.85f - drift, cy3 = 0.55f;
    drawEllipse(cx3, cy3, 0.07f, 0.03f, 24, r, g, b);
}

void drawBirds()
{
    float alpha = 1.0f - wNight;
    if (alpha < 0.03f) return;
    for (int i = 0; i < NUM_BIRDS; i++)
        drawBird(birdX[i], birdY[i], 0.035f, alpha);
}

void drawSea()
{
    glBegin(GL_QUADS);
        glColor3f(seaTopR, seaTopG, seaTopB); glVertex2f(-1.0f, 0.0f);
        glColor3f(seaTopR, seaTopG, seaTopB); glVertex2f( 1.0f, 0.0f);
        glColor3f(seaBotR, seaBotG, seaBotB); glVertex2f( 1.0f, -0.62f);
        glColor3f(seaBotR, seaBotG, seaBotB); glVertex2f(-1.0f, -0.62f);
    glEnd();

    float rx = lerp(0.15f, 0.05f, wNight);
    float glowR = lerp(1.00f, 0.85f, wNight);
    float glowG = lerp(0.72f, 0.90f, wNight);
    float glowB = lerp(0.28f, 1.00f, wNight);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    int stripes = 10;
    for (int i = 0; i < stripes; i++) {
        float y0 = -0.02f - i * 0.06f;
        float y1 = y0 - 0.03f;
        float widen = 0.03f + i * 0.012f;
        float shimmer = sinf(t * 4.0f + i * 1.3f) * 0.015f;
        glColor4f(glowR, glowG, glowB, 0.35f - i * 0.03f);
        glBegin(GL_QUADS);
            glVertex2f(rx - widen + shimmer, y0);
            glVertex2f(rx + widen + shimmer, y0);
            glVertex2f(rx + widen * 0.6f + shimmer, y1);
            glVertex2f(rx - widen * 0.6f + shimmer, y1);
        glEnd();
    }
    glDisable(GL_BLEND);

    glColor3f(seaTopR + 0.12f, seaTopG + 0.12f, seaTopB + 0.08f);
    glLineWidth(1.5f);
    for (int row = 0; row < 5; row++) {
        float baseY = -0.05f - row * 0.11f;
        glBegin(GL_LINE_STRIP);
        for (float x = -1.0f; x <= 1.0f; x += 0.04f) {
            float y = baseY + sinf(x * 6.0f + t * 2.0f + row) * 0.012f;
            glVertex2f(x, y);
        }
        glEnd();
    }
}

void drawBeach()
{
    glBegin(GL_QUADS);
        glColor3f(sandTopR, sandTopG, sandTopB); glVertex2f(-1.0f, -0.62f);
        glColor3f(sandTopR, sandTopG, sandTopB); glVertex2f( 1.0f, -0.62f);
        glColor3f(sandBotR, sandBotG, sandBotB); glVertex2f( 1.0f, -1.0f);
        glColor3f(sandBotR, sandBotG, sandBotB); glVertex2f(-1.0f, -1.0f);
    glEnd();

    glColor3f(foamShade, foamShade, foamShade);
    glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
        for (float x = -1.0f; x <= 1.0f; x += 0.03f) {
            float y = -0.62f + sinf(x * 5.0f + t * 2.2f) * 0.015f - 0.01f;
            glVertex2f(x, y);
        }
    glEnd();
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
        for (float x = -1.0f; x <= 1.0f; x += 0.03f) {
            float y = -0.66f + sinf(x * 5.0f + t * 2.2f + 1.0f) * 0.012f;
            glVertex2f(x, y);
        }
    glEnd();
}

void drawPalmTree()
{
    glPushMatrix();
    glTranslatef(-0.72f, -0.62f, 0.0f);

    glColor3f(0.42f, 0.27f, 0.14f);
    int segs = 10;
    float bx = 0.0f, by = 0.0f, curve = 0.0f;
    for (int i = 0; i < segs; i++) {
        float segLen = 0.045f;
        float nextCurve = curve + 0.045f;
        float nx = bx + sinf(nextCurve) * segLen;
        float ny = by + segLen;
        float widthTop = 0.028f - i * 0.0012f;
        float widthBot = 0.032f - i * 0.0012f;

        float dx = nx - bx, dy = ny - by;
        float len = sqrtf(dx*dx + dy*dy);
        float px = -dy / len, py = dx / len;

        glBegin(GL_QUADS);
            glVertex2f(bx + px * widthBot, by + py * widthBot);
            glVertex2f(bx - px * widthBot, by - py * widthBot);
            glVertex2f(nx - px * widthTop, ny - py * widthTop);
            glVertex2f(nx + px * widthTop, ny + py * widthTop);
        glEnd();

        bx = nx; by = ny; curve = nextCurve;
    }

    float leafAngles[7] = { -70, -40, -12, 15, 42, 68, 95 };
    for (int i = 0; i < 7; i++) {
        glPushMatrix();
        glTranslatef(bx, by, 0.0f);
        glRotatef(leafAngles[i], 0.0f, 0.0f, 1.0f);
        glColor3f(0.10f + i * 0.01f, 0.42f + (i % 3) * 0.02f, 0.12f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(0.0f, 0.0f);
            for (int k = 0; k <= 12; k++) {
                float u = k / 12.0f;
                float lx = u * 0.34f;
                float ly = sinf(u * PI) * 0.075f;
                glVertex2f(lx, ly);
            }
            for (int k = 12; k >= 0; k--) {
                float u = k / 12.0f;
                float lx = u * 0.34f;
                float ly = -sinf(u * PI) * 0.075f;
                glVertex2f(lx, ly);
            }
        glEnd();
        glPopMatrix();
    }

    glPopMatrix();

    drawEllipse(-0.78f, -0.66f, 0.045f, 0.028f, 20, 0.35f, 0.35f, 0.37f);
    drawEllipse(-0.68f, -0.665f, 0.03f, 0.02f, 20, 0.30f, 0.30f, 0.32f);
    glColor3f(0.15f, 0.35f, 0.15f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        for (int i = 0; i < 6; i++) {
            float gx = -0.62f + i * 0.02f;
            glVertex2f(gx, -0.67f);
            glVertex2f(gx + 0.01f, -0.60f);
        }
    glEnd();
}

void drawUmbrellaAt(float x, float y, float scale, float tiltDeg)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glRotatef(tiltDeg, 0.0f, 0.0f, 1.0f);

    float poleR = 0.55f, poleG = 0.55f, poleB = 0.55f;
    float centerY = 0.30f;
    float rx = 0.16f, ry = 0.13f;
    float apexY = centerY + ry;

    drawThickSegment(0.0f, 0.0f, 0.0f, centerY, 0.010f, poleR, poleG, poleB);

    int segs = 14;
    float aR = 0.82f, aG = 0.16f, aB = 0.16f;
    float bR = 0.96f, bG = 0.96f, bB = 0.93f;
    for (int i = 0; i < segs; i++) {
        float a0 = PI * i     / (float)segs;
        float a1 = PI * (i+1) / (float)segs;
        float x0 = cosf(a0) * rx, y0 = centerY + sinf(a0) * ry;
        float x1 = cosf(a1) * rx, y1 = centerY + sinf(a1) * ry;
        bool alt = (i % 2 == 0);
        glColor3f(alt ? aR : bR, alt ? aG : bG, alt ? aB : bB);
        glBegin(GL_TRIANGLES);
            glVertex2f(0.0f, centerY);
            glVertex2f(x0, y0);
            glVertex2f(x1, y1);
        glEnd();
    }

    glColor3f(0.45f, 0.10f, 0.10f);
    glLineWidth(1.2f);
    for (int i = 0; i <= segs; i += 2) {
        float a = PI * i / (float)segs;
        glBegin(GL_LINES);
            glVertex2f(0.0f, centerY);
            glVertex2f(cosf(a) * rx, centerY + sinf(a) * ry);
        glEnd();
    }

    glColor3f(0.35f, 0.08f, 0.08f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= segs; i++) {
            float a = PI * i / (float)segs;
            glVertex2f(cosf(a) * rx, centerY + sinf(a) * ry);
        }
    glEnd();

    drawCircle(0.0f, apexY + 0.012f, 0.016f, 12, poleR, poleG, poleB);

    glPopMatrix();
}

void drawLoungerAt(float x, float y, bool mirror, float scale)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(mirror ? -scale : scale, scale, 1.0f);

    float frameR = 0.85f, frameG = 0.84f, frameB = 0.80f;
    float woodR  = 0.55f, woodG  = 0.40f, woodB  = 0.22f;
    float padR   = 0.22f, padG   = 0.42f, padB   = 0.60f;

    float Fx = 0.00f, Fy = 0.000f;
    float Bx = 0.26f, By = 0.000f;
    float footX = 0.00f, footY = 0.045f;
    float backJx = 0.22f, backJy = 0.050f;
    float headX  = 0.34f, headY  = 0.115f;

    float frameT = 0.026f, legT = 0.016f;

    drawThickSegment(Fx, Fy, footX, footY, legT, frameR, frameG, frameB);
    drawThickSegment(Bx, By, backJx, backJy, legT, frameR, frameG, frameB);

    drawThickSegment(footX, footY, backJx, backJy, frameT, frameR, frameG, frameB);
    drawThickSegment(backJx, backJy, headX, headY, frameT, frameR, frameG, frameB);
    drawCircle(footX, footY, frameT * 0.5f, 14, frameR, frameG, frameB);
    drawCircle(backJx, backJy, frameT * 0.5f, 14, frameR, frameG, frameB);
    drawCircle(headX, headY, frameT * 0.5f, 14, frameR, frameG, frameB);

    drawThickSegment(footX, footY + 0.007f, backJx, backJy + 0.007f, 0.018f, woodR, woodG, woodB);
    {
        float dx = headX - backJx, dy = headY - backJy;
        float len = sqrtf(dx*dx + dy*dy);
        float nx = -dy / len, ny = dx / len;
        drawThickSegment(backJx + nx*0.007f, backJy + ny*0.007f,
                          headX  + nx*0.007f, headY  + ny*0.007f, 0.018f, woodR, woodG, woodB);
    }

    for (int i = 1; i <= 3; i++) {
        float u = i / 4.0f;
        float sx = footX + (backJx - footX) * u;
        float sy = footY + (backJy - footY) * u;
        drawThickSegment(sx, sy - 0.012f, sx, sy + 0.026f, 0.006f, woodR*0.7f, woodG*0.7f, woodB*0.7f);
    }

    drawThickSegment(footX + 0.01f, footY + 0.012f, backJx, backJy + 0.012f, 0.013f, padR, padG, padB);
    {
        float dx = headX - backJx, dy = headY - backJy;
        float len = sqrtf(dx*dx + dy*dy);
        float nx = -dy / len, ny = dx / len;
        drawThickSegment(backJx + nx*0.012f, backJy + ny*0.012f,
                          headX  + nx*0.012f - dx*0.03f, headY + ny*0.012f - dy*0.03f,
                          0.013f, padR, padG, padB);
    }

    glPopMatrix();
}

void drawLoungeSetAt(float x, float y, bool mirror, float scale)
{
    float umbrellaOffsetX = mirror ? -0.16f * scale : 0.16f * scale;
    drawUmbrellaAt(x + umbrellaOffsetX, y, scale, mirror ? 4.0f : -4.0f);
    drawLoungerAt(x, y, mirror, scale);
}

void drawBoatAt(float x, float y, bool faceRight, float scale)
{
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(faceRight ? -scale : scale, scale, 1.0f);

    glColor3f(0.22f, 0.13f, 0.07f);
    glBegin(GL_POLYGON);
        glVertex2f(-0.19f,  0.010f);
        glVertex2f(-0.12f, -0.025f);
        glVertex2f( 0.13f, -0.025f);
        glVertex2f( 0.15f,  0.010f);
        glVertex2f( 0.08f,  0.028f);
        glVertex2f(-0.10f,  0.028f);
    glEnd();
    glColor3f(0.35f, 0.22f, 0.12f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(-0.10f, 0.028f);
        glVertex2f( 0.08f, 0.028f);
    glEnd();

    glColor3f(0.30f, 0.20f, 0.10f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(0.06f, 0.028f);
        glVertex2f(0.06f, 0.26f);
    glEnd();

    glColor3f(0.95f, 0.95f, 0.92f);
    glBegin(GL_TRIANGLES);
        glVertex2f(0.06f, 0.26f);
        glVertex2f(0.06f, 0.05f);
        glVertex2f(-0.14f, 0.09f);
    glEnd();

    glColor3f(0.60f, 0.60f, 0.64f);
    glBegin(GL_TRIANGLES);
        glVertex2f(0.06f, 0.19f);
        glVertex2f(0.06f, 0.05f);
        glVertex2f(-0.17f, 0.02f);
    glEnd();

    glColor3f(0.85f, 0.15f, 0.15f);
    glBegin(GL_TRIANGLES);
        glVertex2f(0.06f, 0.26f);
        glVertex2f(0.06f, 0.23f);
        glVertex2f(0.10f, 0.245f);
    glEnd();

    glPopMatrix();
}

void drawBoat()
{
    drawBoatAt(boatX,  -0.30f + boatBob,  false, 1.00f);
    drawBoatAt(boat2X, -0.17f + boat2Bob, true,  0.82f);
}

void display()
{
    computePalette();

    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    drawSky();
    drawClouds();
    drawBirds();
    drawSunOrMoon();
    drawSea();
    drawBoat();
    drawBeach();
    drawPalmTree();
    drawLoungeSetAt(-0.20f, -0.82f, true,  1.00f);
    drawLoungeSetAt( 0.66f, -0.88f, false, 0.72f);

    glFlush();
    glutSwapBuffers();
}

void update(int value)
{
    t += 0.02f;

    dayPhase += phaseSpeed;
    if (dayPhase >= 1.0f) dayPhase -= 1.0f;

    boatBob = sinf(t * 1.6f) * 0.012f;
    boatX  -= boatSpeed;
    if (boatX < -1.3f) boatX = 1.3f;

    boat2Bob = sinf(t * 1.4f + 1.7f) * 0.012f;
    boat2X  += boat2Speed;
    if (boat2X > 1.3f) boat2X = -1.3f;

    for (int i = 0; i < NUM_BIRDS; i++) {
        birdX[i] += birdSpeed[i];
        if (birdX[i] > 1.3f) {
            birdX[i] = -1.3f;
            birdY[i] = 0.45f + (rand() % 30) / 100.0f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(30, update, 0);
}

void keyboard(unsigned char key, int x, int y)
{
    switch (key) {
        case '1': dayPhase = 0.00f; break;
        case '2': dayPhase = 0.25f; break;
        case '3': dayPhase = 0.50f; break;
        case 27:  exit(0);          break;
    }
    glutPostRedisplay();
}

void reshape(int w, int h)
{
    windowW = w; windowH = h;
    glViewport(0, 0, w, h);
}

void playWaveSound()
{
    PlaySound("wave.wav", NULL, SND_ASYNC | SND_FILENAME | SND_LOOP);
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowW, windowH);
    glutInitWindowPosition(80, 60);
    glutCreateWindow("Sea View - 2D Graphics Project");

    init();
    playWaveSound();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(30, update, 0);

    glutMainLoop();
    return 0;
}
