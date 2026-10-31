#include "raylib.h"

int main() {
    InitWindow(800, 600, "roguelike");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("@", 400, 300, 40, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
