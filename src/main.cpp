#include "raylib.h"
#include "raymath.h"
#include "test.h"
#include <vector>
#include <random>

using namespace std;

random_device rd; 
mt19937 gen(rd()); 

//-----------------------------------------------------------------------------------------
// ENUMS

enum GameState { PLAYING, INVENTORY, MAP };
enum enemyState { ROAMING, SEARCHING, ATTACKING};
enum TileType { FLOOR, WALL, WATER };

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
// INICIO DAS STRUCTS

struct Projectile {
    Vector2 position;
    Vector2 direction;
    float speed, screenTime;
    int damage;
};

struct Enemy {
    int healthPoints = 50;
    int powerPoints = 5;
    int attackRange = 5;
    int speed = 2;

    Vector2 position;
};

struct GameStructure {
    GameState gameState = PLAYING;
    static const int TILE = 32;
    static const int MAP_W = 94;
    static const int MAP_H = 94;
    // mundo em pixels = MAP_W * TILE = 3008 pixels

    struct WindowConfig {
        const int Width = 1280;
        const int Height = 720;
        const Vector2 Center = {Width/2.0f, Height/2.0f};
    };
    WindowConfig screen;

    struct WorldConfig {
        const int Width = MAP_W * TILE;
        const int Height = MAP_H * TILE;
        const Vector2 Center = {Width/2.0f, Height/2.0f};
    };
    WorldConfig world;

    std::vector<Projectile> projectiles;
    std::vector<Enemy> enemies;
};

GameStructure GAME;

struct Players {
    int speed = 5;
    Vector2 position = GAME.world.Center;
};

Players player;

// FIM DAS STRUCTS
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
// INICIO DAS FUNCOES


std::vector<TileType> tiles(GAME.MAP_W * GAME.MAP_H, FLOOR);                // Vetor unidimensional que guarda os tipos de tiles
TileType& tileAt(int x, int y) { return tiles[y * GAME.MAP_W + x]; }        // Função que retorna o tipo do tile na posição (x, y)


bool isWall(int x, int y) {
    int tileX = (int)(x / GAME.TILE), tileY = (int)(y / GAME.TILE);

    if (tileX < 0 || tileY < 0 || tileX >= GAME.MAP_W || tileY >= GAME.MAP_H) return true;

    return tileAt(tileX, tileY) == WALL;
}


/*
    TODO:
        - Colisão das balas
        - Geração de inimigos
        - IA dos inimigos
        - sistema de vida e dano do player
        - Sistema de itens
        - pegar sprites para o player, cenário, inimigos, projéteis, etc
        - criar diferentes tipos de projéteis
        - criar armas corpo a corpo
        - criar a geração de fases do mundo
*/


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

    printingFunctionTEST();
    
    // Initialization
    //-------------------------------------------------------------------------------------
    ChangeDirectory(GetApplicationDirectory());
    InitWindow(GAME.screen.Width, GAME.screen.Height, "Roguelike");
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(KEY_NULL);           // Retira o ESC como tecla de fechar o jogo
    createEnemies(enemy, enemiesAmount);


    //-------------------------------------------------------------------------------------
    // variables
    Texture2D background = LoadTexture("resources/grassBackground.png");
    SetTextureWrap(background, TEXTURE_WRAP_REPEAT);
    Camera2D camera = {0};

    camera.target = (Vector2){player.position.x, player.position.y};
    camera.offset = GAME.screen.Center;
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {


        // Update
        //----------------------------------------------------------------------------------
        // Update your variables here
        if (GAME.gameState == PLAYING) {
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
        }

        if (IsKeyPressed(KEY_TAB)) {
            if (GAME.gameState == GameState::PLAYING) GAME.gameState = GameState::INVENTORY;
            else if (GAME.gameState == GameState::INVENTORY) GAME.gameState = GameState::PLAYING;      
        }
        if (IsKeyPressed(KEY_M))
            if (GAME.gameState == GameState::PLAYING || GAME.gameState == GameState::INVENTORY) GAME.gameState = GameState::MAP;
            else if (GAME.gameState == MAP) GAME.gameState = GameState::PLAYING;   
        if (IsKeyPressed(KEY_ESCAPE)) GAME.gameState = GameState::PLAYING;
        
        for (int i = 0; i < enemiesAmount; i++) {
            enemyMovement(enemy[i]);
        }

        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);

        camera.target = (Vector2){player.position.x, player.position.y};

        if (GAME.gameState == PLAYING) {
            if (IsCursorOnScreen() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 direction = Vector2Normalize(Vector2Subtract(mouseWorld, player.position));

                GAME.projectiles.push_back({
                    player.position,
                    direction,
                    10.0f,
                    0,
                    0
                });
            }

            for (int i = (int)GAME.projectiles.size() - 1; i >= 0; i--) {
                Projectile& bullet = GAME.projectiles[i];
                
                bullet.screenTime += GetFrameTime();
                if (bullet.screenTime > 5.0f) {
                    GAME.projectiles.erase(GAME.projectiles.begin() + i);
                    continue;
                }

                bullet.position = Vector2Add(bullet.position, Vector2Scale(bullet.direction, bullet.speed));
            }
        }

        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(BLACK);
            if (GAME.gameState == MAP) {
                DrawRectangle(0, 0, GAME.screen.Width, GAME.screen.Height, YELLOW);
                DrawTextPro(GetFontDefault(), "MAPA", GAME.screen.Center, (Vector2){60, 20}, 0.0f, 40.0f, 5.0f, BLACK);
            } else {
                BeginMode2D(camera);
                    DrawTexturePro(
                        background,
                        (Rectangle){ 0, 0, (float)GAME.world.Width, (float)GAME.world.Height },  // pedaço da textura
                        (Rectangle){ 0, 0, (float)GAME.world.Width, (float)GAME.world.Height },  // onde vai no mundo
                        (Vector2){ 0, 0 },
                        0.0f,
                        WHITE
                    );

                    {
                        Vector2 beg = GetScreenToWorld2D({0, 0}, camera);
                        Vector2 end = GetScreenToWorld2D({(float)GAME.screen.Width, (float)GAME.screen.Height}, camera);

                        int x0 = (int)(beg.x / GAME.TILE) - 1,  x1 = (int)(end.x / GAME.TILE) + 1;
                        int y0 = (int)(beg.y / GAME.TILE) - 1,  y1 = (int)(end.y / GAME.TILE) + 1;

                        if (x0 < 0) x0 = 0;  if (x1 >= GAME.MAP_W) x1 = GAME.MAP_W - 1;
                        if (y0 < 0) y0 = 0;  if (y1 >= GAME.MAP_H) y1 = GAME.MAP_H - 1;

                        for (int y = y0; y <= y1; y++) {
                            for (int x = x0; x <= x1; x++) {
                                if (tileAt(x, y) == WALL)
                                    DrawRectangle(x * GAME.TILE, y * GAME.TILE, GAME.TILE, GAME.TILE, DARKGRAY);
                            }
                        }
                    }

                    if (GAME.gameState == PLAYING) {
                        DrawText("@", player.position.x - 15, player.position.y - 20, 40, RAYWHITE);
                
                        for (int i = 0; i < enemiesAmount; i++) {
                          DrawText("E", enemy[i].position.x, enemy[i].position.y, 40, RED);
                        }
                        if (!GAME.projectiles.empty()) {
                            for (const auto bullet : GAME.projectiles) {
                                Rectangle bulletSprite = {bullet.position.x, bullet.position.y, 8.0f, 4.0f};
                                DrawRectanglePro(bulletSprite, (Vector2){4.0f, 2.0f}, (atan2f(bullet.direction.y, bullet.direction.x) * RAD2DEG), YELLOW);
                            }
                        }
                    } else if (GAME.gameState == INVENTORY) {
                        DrawRectangle((camera.target.x - GAME.screen.Center.x + GAME.screen.Width/10.0f), (camera.target.y - GAME.screen.Center.y + GAME.screen.Height/10.0f), 8.0f * GAME.screen.Width/10.0f, 8.0f * GAME.screen.Height/10.0f, BLACK);
                    }
                EndMode2D();
            }
            DrawLineV(GAME.screen.Center, mouseScreen, RED);
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
