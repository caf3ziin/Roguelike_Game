#include "raylib.h"

enum GameState { PLAYING, INVENTORY, MAP };

struct GameStructure {
    GameState gameState = PLAYING;
    struct WindowConfig {
        const int Width = 1280;
        const int Height = 720;
    };
    WindowConfig screen;

    struct WorldConfig {
        const int Width = 3000;
        const int Height = 3000;
    };
    WorldConfig world;
};
GameStructure GAME;

struct Players {
    int speed = 5;
    Vector2 position = {GAME.screen.Width/2.0f, GAME.screen.Height/2.0f};
};
Players player;

enum enemyState { ROAMING, SEARCHING, ATTACKING};
struct Enemy {
    int healthPoints = 50;
    int powerPoints = 5;
    int attackRange = 5;

};

// raylib.com
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main() {

    // Initialization
    //-------------------------------------------------------------------------------------
    InitWindow(GAME.screen.Width, GAME.screen.Height, "Roguelike");
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(KEY_NULL);           // Retira o ESC como tecla de fechar o jogo


    //-------------------------------------------------------------------------------------
    // variables
    Texture2D background = LoadTexture("resources/grassBackground.png");
    SetTextureWrap(background, TEXTURE_WRAP_REPEAT);
    Camera2D camera = {0};

    camera.target = (Vector2){player.position.x, player.position.y};
    camera.offset = (Vector2){GAME.screen.Width/2.0f, GAME.screen.Height/2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {

        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        if (IsKeyDown(KEY_D)) {
            if (player.position.x + player.speed <= GAME.world.Width)
                player.position.x += player.speed;
        }
        if (IsKeyDown(KEY_A)) {
            if (player.position.x - player.speed >= 0)
                player.position.x -= player.speed;
        }
        if (IsKeyDown(KEY_W)) {
            if (player.position.y - player.speed >= 0)
                player.position.y -= player.speed;
        }
        if (IsKeyDown(KEY_S)) {
            if (player.position.y + player.speed <= GAME.world.Height)
                player.position.y += player.speed;
        }
      
        if (IsKeyPressed(KEY_TAB)) {
            if (GAME.gameState == GameState::PLAYING) GAME.gameState = GameState::INVENTORY;
            else if (GAME.gameState == GameState::INVENTORY) GAME.gameState = GameState::PLAYING;
            else if (GAME.gameState == MAP) GAME.gameState = GameState::MAP;        
        }
        if (IsKeyPressed(KEY_M))
            if (GAME.gameState == GameState::PLAYING) GAME.gameState = MAP;
            else if (GAME.gameState == GameState::INVENTORY) GAME.gameState = GameState::MAP;
            else if (GAME.gameState == MAP) GAME.gameState = GameState::PLAYING;   
        if (IsKeyPressed(KEY_ESCAPE)) GAME.gameState = GameState::PLAYING;

        camera.target = (Vector2){player.position.x, player.position.y};

        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode2D(camera);
                DrawTexturePro(
                    background,
                    (Rectangle){ 0, 0, (float)GAME.world.Width, (float)GAME.world.Height },  // pedaço da textura
                    (Rectangle){ 0, 0, (float)GAME.world.Width, (float)GAME.world.Height },  // onde vai no mundo
                    (Vector2){ 0, 0 },
                    0.0f,
                    WHITE
                );
                DrawText("@", player.position.x, player.position.y, 40, RAYWHITE);
            EndMode2D();
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
