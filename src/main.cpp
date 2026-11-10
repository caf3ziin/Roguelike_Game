#include "raylib.h"
#include "raymath.h"
#include "debug.h"
#include <vector>
#include <random>
#include <ctime>

#define MAX_FRAME_SPEED     15
#define MIN_FRAME_SPEED      1

using namespace std;

random_device rd; 
mt19937 gen(rd()); 

//-----------------------------------------------------------------------------------------
// ENUMS
enum PlayerSkin { KITSUNE, GANGSTER, SOLDIER};
enum PlayerAnimation { PLAYER_RUN, PLAYER_IDLE, PLAYER_ATTACK, PLAYER_DEAD};

enum GameState { PLAYING, INVENTORY, MAP };
enum EnemyState { ROAMING, SEARCHING, ATTACKING};
enum TileType { FLOOR, WALL, WATER };

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
// INICIO DAS STRUCTS

struct Projectile {
    Vector2 position;
    Vector2 direction;
    float speed, screenTime;
    int damage, life;
};

struct Enemy {
    int healthPoints = 50;
    int powerPoints = 5;
    int attackRange = 5;
    int speed = 2;

    Vector2 position;
    bool facingLeft = false;
};

struct GameStructure {
    GameState gameState = PLAYING;      // variável que guarda o estado do jogo para recursos de menu e mapa
    static const int TILE = 32;         // tamanho de cada "quadrado" da matriz em pixels
    static const int MAP_W = 94;        // largura (width) do mapa em tiles ("quadrados") na matriz do jogo
    static const int MAP_H = 94;        // altura (height) do mapa em tiles ("quadrados") na matriz do jogo
    // mundo em pixels = MAP_W * TILE = 3008 pixels

    // unsigned = não existem valores negativos
    // variável da seed da run
    unsigned int seed;

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

    vector<Projectile> projectiles;
    vector<Enemy> enemies;
};

GameStructure GAME;

struct Players {
    int healthPoints = 100;
    int speed = 5;

    Vector2 position = GAME.world.Center;
    bool facingLeft = false;

    PlayerSkin skin = KITSUNE;
    PlayerAnimation animation = PLAYER_IDLE;
  
    int currentFrame = 0;
    float frameTimer = 0.0f;
};

Players player;

struct PlayerSprites {
    Texture2D idle;
    Texture2D run;
    Texture2D attack;
    Texture2D dead;
};

PlayerSprites kitsuneSprites;
PlayerSprites gangsterSprites;
PlayerSprites soldierSprites;

// FIM DAS STRUCTS
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
// INICIO DAS FUNCOES


vector<TileType> tiles(GAME.MAP_W * GAME.MAP_H, FLOOR);                // Vetor unidimensional que guarda os tipos de tiles
TileType& tileAt(int x, int y) { return tiles[y * GAME.MAP_W + x]; }        // Função que retorna o tipo do tile na posição (x, y)


// Verifica automaticamente se a posição (x, y) é uma parede na matriz do jogo
bool isWall(int x, int y) {
    int tileX = (int)(x / GAME.TILE), tileY = (int)(y / GAME.TILE);

    if (tileX < 0 || tileY < 0 || tileX >= GAME.MAP_W || tileY >= GAME.MAP_H) return true;

    return tileAt(tileX, tileY) == WALL;
}


// Gera os inimigos na volta do personagem
void createEnemies(vector<Enemy> &enemies, int enemiesAmount){
    uniform_int_distribution<int> ranHeight(0, GAME.MAP_W * GAME.TILE - 40); 
    uniform_int_distribution<int> ranWidth(0, GAME.MAP_H * GAME.TILE - 40); 

    for(int i = 0; i < enemiesAmount; i++){
        do {
            enemies[i].position = {
                (float)ranWidth(gen),
                (float)ranHeight(gen)
            };

        } while (Vector2Distance(enemies[i].position, player.position) < 150);
    }

}

// Move os inimigos em direção ao player de acordo com sua posição (x, y)
void enemyMovement(vector<Enemy>& enemies, int enemyIndex, int enemiesAmount) {
    Enemy& enemy = enemies[enemyIndex];

    Vector2 direction = Vector2Subtract(player.position, enemy.position);

    if (direction.x < 0)
        enemy.facingLeft = true;
    else if (direction.x > 0)
        enemy.facingLeft = false;

    if (Vector2Length(direction) == 0.0f)
        return;

    direction = Vector2Normalize(direction);

    Vector2 nextPosition = Vector2Add(enemy.position, Vector2Scale(direction, (float)enemy.speed)
    );

    const float minimumDistance = 15.0f;

    for (int i = 0; i < enemiesAmount; i++) {
        if (i == enemyIndex)
            continue;

        if (Vector2Distance(nextPosition, enemies[i].position) < minimumDistance) {
            return;
        }
    }

    enemy.position = nextPosition;
}

PlayerSprites& getPlayerSprites() {
    switch (player.skin) {
        case GANGSTER:
            return gangsterSprites;

        case SOLDIER:
            return soldierSprites;

        case KITSUNE:
        default:
            return kitsuneSprites;
    }
}

Texture2D getPlayerTexture() {
    PlayerSprites& sprites = getPlayerSprites();

    switch (player.animation) {
        case PLAYER_IDLE:
            return sprites.idle;

        case PLAYER_ATTACK:
            return sprites.attack;

        case PLAYER_DEAD:
            return sprites.dead;

        case PLAYER_RUN:
        default:
            return sprites.run;
    }
}

Texture2D loadSprite(const char* path, Color backgroundColor) {
    Image image = LoadImage(path);
    ImageColorReplace(&image, backgroundColor, BLANK);

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);

    return texture;
}

int getPlayerFrameCount() {
    switch (player.skin) {
        case KITSUNE:
            switch (player.animation) {
                case PLAYER_DEAD: return 10;
                default: return 8;
            }

        case GANGSTER:
            switch (player.animation) {
                case PLAYER_RUN: return 10;
                case PLAYER_IDLE: return 6;
                case PLAYER_ATTACK: return 4;
                case PLAYER_DEAD: return 5;
            }

        case SOLDIER:
            switch (player.animation) {
                case PLAYER_RUN:
                case PLAYER_IDLE:
                    return 8;

                case PLAYER_ATTACK:
                case PLAYER_DEAD:
                    return 4;
            }
    }

    return 1;
}

void setPlayerAnimation(PlayerAnimation animation) {
    if (player.animation != animation) {
        player.animation = animation;
        player.currentFrame = 0;
        player.frameTimer = 0.0f;
    }
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

int main() {

    const int enemiesAmount = 20;
    vector<Enemy> enemies(enemiesAmount);
    
    // Initialization
    //-------------------------------------------------------------------------------------
    ChangeDirectory(GetApplicationDirectory());                         // Muda o diretório de inicialiçação para o diretório onde se encontra o executável
    InitWindow(GAME.screen.Width, GAME.screen.Height, "Roguelike");     // INICIALIZAÇÃO DA JANELA (INIT-WINDOW)
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(KEY_NULL);           // Retira o ESC como tecla de fechar o jogo

    // Geração de seed aleatória manual, por motivos de debug ("manual" pois o raylib por padrão já gera seeds aleatórias irrastreáveis)
    {
        unsigned int seed = (unsigned int)time(nullptr);       // a seed gerada é de acordo com o horário da execução no sistema
        SetRandomSeed(seed);
        GAME.seed = seed;   // Guarda a seed na struct do jogo, e a variável local é jogada fora
    }
    printSeedDebug(GAME.seed);

    createEnemies(enemies, enemiesAmount);

    for (int i = 0; i < tiles.size(); i++) {
        int percentage = GetRandomValue(1, 100);
        if (percentage <= 15)
            tiles[i] = WALL;
    }

    //-------------------------------------------------------------------------------------
    //Sprites Generation

    kitsuneSprites.idle =
        loadSprite("resources/sprites/player/kitsune/Idle.png", WHITE);
    kitsuneSprites.run =
        loadSprite("resources/sprites/player/kitsune/Run.png", WHITE);
    kitsuneSprites.attack =
        loadSprite("resources/sprites/player/kitsune/Attack.png", WHITE);
    kitsuneSprites.dead =
        loadSprite("resources/sprites/player/kitsune/Dead.png", WHITE);

    gangsterSprites.idle =
        loadSprite("resources/sprites/player/gangster/Idle.png", BLACK);
    gangsterSprites.run =
        loadSprite("resources/sprites/player/gangster/Run.png", BLACK);
    gangsterSprites.attack =
        loadSprite("resources/sprites/player/gangster/Attack.png", BLACK);
    gangsterSprites.dead =
        loadSprite("resources/sprites/player/gangster/Dead.png", BLACK);

    soldierSprites.idle =
        loadSprite("resources/sprites/player/soldier/Idle.png", BLACK);
    soldierSprites.run =
        loadSprite("resources/sprites/player/soldier/Run.png", BLACK);
    soldierSprites.attack =
        loadSprite("resources/sprites/player/soldier/Attack.png", BLACK);
    soldierSprites.dead =
        loadSprite("resources/sprites/player/soldier/Dead.png", BLACK);

    //
    //--------------------------------------------------------------------------------------

    //-------------------------------------------------------------------------------------
    // variables
    Texture2D background = LoadTexture("resources/backgrounds/grassBackground.png");
    Image playerImage = LoadImage("resources/sprites/player/kitsune/Run.png");
    ImageColorReplace(&playerImage, WHITE, BLANK);
    UnloadImage(playerImage);
    Image enemyImage = LoadImage("resources/sprites/enemy/zombie/Walk.png");
    ImageColorReplace(&enemyImage, WHITE, BLANK);
    Texture2D enemyZombie = LoadTextureFromImage(enemyImage);
    UnloadImage(enemyImage);
    SetTextureWrap(background, TEXTURE_WRAP_REPEAT);
    Camera2D camera = {0};

    camera.target = (Vector2){player.position.x, player.position.y};
    camera.offset = GAME.screen.Center;
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    float frameDuration = 0.1f;
    int enemyCurrentFrame = 0;
    float enemyFrameTimer = 0.0f;

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {

        // Update
        //----------------------------------------------------------------------------------
        // Update your variables here

        player.frameTimer += GetFrameTime();

        if (player.frameTimer >= frameDuration) {
            player.frameTimer = 0.0f;
            player.currentFrame = (player.currentFrame + 1) % getPlayerFrameCount();
        }
        if (GAME.gameState == PLAYING) {
            Vector2 movement = {0.0f, 0.0f};

            if (IsKeyDown(KEY_D)) {
                movement.x += 1.0f;
                player.facingLeft = false;
            }

            if (IsKeyDown(KEY_A)) {
                movement.x -= 1.0f;
                player.facingLeft = true;
            }

            if (IsKeyDown(KEY_S))
                movement.y += 1.0f;

            if (IsKeyDown(KEY_W))
                movement.y -= 1.0f;

            if (Vector2Length(movement) > 0.0f) {
                movement = Vector2Normalize(movement);
                setPlayerAnimation(PLAYER_RUN);
            } else {
                setPlayerAnimation(PLAYER_IDLE);
            }

            player.position.x += movement.x * player.speed;
            player.position.y += movement.y * player.speed;

            player.position.x = Clamp(player.position.x, 0.0f, (float)GAME.world.Width);
            player.position.y = Clamp(player.position.y, 0.0f, (float)GAME.world.Height);
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
            enemyMovement(enemies, i, enemiesAmount);
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

            if (player.frameTimer >= frameDuration) {
                player.frameTimer = 0.0f;
                player.currentFrame = (player.currentFrame + 1) % 8;
            }

            enemyFrameTimer += GetFrameTime();
            if (enemyFrameTimer >= frameDuration) {
                enemyFrameTimer = 0.0f;
                enemyCurrentFrame = (enemyCurrentFrame + 1) % 10;
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
                        Rectangle playerSource = {
                            player.currentFrame * 128.0f,
                            0.0f,
                            128.0f,
                            128.0f
                        };
                        if (player.facingLeft)
                            playerSource.width = -playerSource.width;

                        Rectangle playerDestination = {
                            player.position.x - 48.0f,
                            player.position.y - 48.0f,
                            96.0f,
                            96.0f
                        };

                        DrawTexturePro(
                            getPlayerTexture(),
                            playerSource,
                            playerDestination,
                            (Vector2){0.0f, 0.0f},
                            0.0f,
                            WHITE
                        );
                
                        for (int i = 0; i < enemiesAmount; i++) {
                            Rectangle enemySource = {
                                enemyCurrentFrame * 128.0f,
                                0.0f,
                                128.0f,
                                128.0f
                            };
                            if (enemies[i].facingLeft)
                                enemySource.width = -enemySource.width;

                            Rectangle enemyDestination = {
                                enemies[i].position.x - 48.0f,
                                enemies[i].position.y - 48.0f,
                                96.0f,
                                96.0f
                            };

                            DrawTexturePro(
                                enemyZombie,
                                enemySource,
                                enemyDestination,
                                (Vector2){0.0f, 0.0f},
                                0.0f,
                                WHITE
                            );
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
            DrawText(TextFormat("seed: %u", GAME.seed), 10, 10, 20, WHITE);      // Debug: escreve a seed no canto da tela
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(background);

    UnloadTexture(kitsuneSprites.idle);
    UnloadTexture(kitsuneSprites.run);
    UnloadTexture(kitsuneSprites.attack);
    UnloadTexture(kitsuneSprites.dead);

    UnloadTexture(gangsterSprites.idle);
    UnloadTexture(gangsterSprites.run);
    UnloadTexture(gangsterSprites.attack);
    UnloadTexture(gangsterSprites.dead);

    UnloadTexture(soldierSprites.idle);
    UnloadTexture(soldierSprites.run);
    UnloadTexture(soldierSprites.attack);
    UnloadTexture(soldierSprites.dead);

    UnloadTexture(enemyZombie);
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
