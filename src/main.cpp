#include "raylib.h"


//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main() {

    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 600;

    int playerSpeed = 3;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    Vector2 playerPosition = {screenWidth/2, screenHeight/2};

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {

        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        if (IsKeyDown(KEY_RIGHT)) playerPosition.x += playerSpeed;
        if (IsKeyDown(KEY_LEFT)) playerPosition.x -= playerSpeed;
        if (IsKeyDown(KEY_UP)) playerPosition.y -= playerSpeed;
        if (IsKeyDown(KEY_DOWN)) playerPosition.y += playerSpeed;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("@", playerPosition.x, playerPosition.y, 40, RAYWHITE);
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
