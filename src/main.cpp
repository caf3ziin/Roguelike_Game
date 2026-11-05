#include "raylib.h"
#include "raymath.h"
#include <random>

using namespace std;

random_device rd; 
mt19937 gen(rd()); 

enum GameState { PLAYING, INVENTORY, MAP };

struct GameStructure {
    GameState gameState = PLAYING;
    struct WindowConfig {
        const int Width = 1280;
        const int Height = 960;
    };
    WindowConfig screen;
};

GameStructure GAME;

struct Players {
    int speed = 5;
    Vector2 position = {GAME.screen.Width/2, GAME.screen.Height/2};
};

Players player;

enum enemyState { ROAMING, SEARCHING, ATTACKING};
struct Enemy {
    int healthPoints = 50;
    int powerPoints = 5;
    int attackRange = 5;
    int speed = 2;

    Vector2 position;

};

// raylib.com
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------

void createEnemies(Enemy (&enemy)[20], int enemiesAmount){
    uniform_int_distribution<int> ranHeight(0, GAME.screen.Height - 40); 
    uniform_int_distribution<int> ranWidth(0, GAME.screen.Width - 40); 

    for(int i = 0; i < enemiesAmount; i++){
        do {
            enemy[i].position = {
                (float)ranWidth(gen),
                (float)ranHeight(gen)
            };

        } while (Vector2Distance(enemy[i].position, player.position) < 150);
    }

}

void enemyMovement(Enemy &enemy) {

    if (enemy.position.x < player.position.x)
        enemy.position.x += enemy.speed;

    else if (enemy.position.x > player.position.x)
        enemy.position.x -= enemy.speed;

    if (enemy.position.y < player.position.y)
        enemy.position.y += enemy.speed;

    else if (enemy.position.y > player.position.y)
        enemy.position.y -= enemy.speed;
}

int main() {

    const int enemiesAmount = 20;
    Enemy enemy[enemiesAmount];

        // Initialization
    //-------------------------------------------------------------------------------------
    // variables


    InitWindow(GAME.screen.Width, GAME.screen.Height, "Roguelike");
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(KEY_NULL);           // Retira o ESC como tecla de fechar o jogo
    createEnemies(enemy, enemiesAmount);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {


        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        if (IsKeyDown(KEY_D)) {
            if (player.position.x + player.speed <= GAME.screen.Width - 30)
                player.position.x += player.speed;
        }
        if (IsKeyDown(KEY_A)) {
            if (player.position.x - player.speed >= 0)
                player.position.x -= player.speed;
        }
        if (IsKeyDown(KEY_W)) {
            if (player.position.y - player.speed >= -10)
                player.position.y -= player.speed;
        }
        if (IsKeyDown(KEY_S)) {
            if (player.position.y + player.speed <= GAME.screen.Height - 35)
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

        for (int i = 0; i < enemiesAmount; i++) {
            enemyMovement(enemy[i]);
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("@", player.position.x, player.position.y, 40, RAYWHITE);
        for (int i = 0; i < enemiesAmount; i++) {
            DrawText("E", enemy[i].position.x, enemy[i].position.y, 40, RED);
        }
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
