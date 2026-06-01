#pragma once
// renderer/raylib_renderer.h
// Реализация IRenderer через Raylib.
// Чтобы сменить бэкенд (OpenGL/Vulkan) — написать новый класс, реализующий IRenderer.

#include "renderer.h"
#include "rlgl.h"

class RaylibRenderer : public IRenderer {
public:
    void beginFrame(Camera3D& cam, Color clearColor = SKYBLUE) override {
        BeginDrawing();
        ClearBackground(clearColor);
        BeginMode3D(cam);
    }

    void endFrame() override {
        EndMode3D();
        EndDrawing();
    }

    void drawCube(Vector3 pos, Vector3 size, Color color, float rotY = 0.0f) override {
        rlPushMatrix();
        rlTranslatef(pos.x, pos.y, pos.z);
        rlRotatef(rotY, 0.0f, 1.0f, 0.0f);
        DrawCube({0,0,0}, size.x, size.y, size.z, color);
        rlPopMatrix();
    }

    void drawCubeWires(Vector3 pos, Vector3 size, Color color, float rotY = 0.0f) override {
        rlPushMatrix();
        rlTranslatef(pos.x, pos.y, pos.z);
        rlRotatef(rotY, 0.0f, 1.0f, 0.0f);
        DrawCubeWires({0,0,0}, size.x, size.y, size.z, color);
        rlPopMatrix();
    }

    void drawGrid(int slices, float spacing) override {
        DrawGrid(slices, spacing);
    }

    void drawSphere(Vector3 pos, float radius, Color color) override {
        DrawSphere(pos, radius, color);
    }

    void drawLine3D(Vector3 from, Vector3 to, Color color) override {
        DrawLine3D(from, to, color);
    }

    void drawText(const char* text, int x, int y, int size, Color color) override {
        DrawText(text, x, y, size, color);
    }

    void drawCircle(int x, int y, float radius, Color color) override {
        DrawCircle(x, y, radius, color);
    }
};
