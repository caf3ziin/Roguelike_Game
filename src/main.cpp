#include "raylib.h"
#include "raymath.h"
#include "debug.h"
#include <math.h>
#include <vector>
#include <random>
#include <ctime>


using namespace std;

mt19937 gen{};

//-----------------------------------------------------------------------------------------
// ENUMS
enum PlayerSkin { KITSUNE, GANGSTER, SOLDIER};
enum EnemyType { ZOMBIE, /* ORC, SKELETON */};
enum PlayerAnimation { PLAYER_RUN, PLAYER_IDLE, PLAYER_ATTACK, PLAYER_DEAD};

enum GameState { MENU, PLAYING, INVENTORY, MAP, STATUS_TREE, PAUSED, SETTINGS };
enum EnemyState { ROAMING, SEARCHING, ATTACKING, IDLE, DYING};
enum TileType { FLOOR, WALL, WATER };

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
// INICIO DAS STRUCTS

// Caixa da hitbox geral (retângulos)
struct Collider {
    Vector2 size   = { 24, 16 };   // largura e altura da caixa
    Vector2 offset = { 0, 24 };    // deslocamento a partir do centro da entidade
};

// struct que guarda a posição, a velocidade e a hitbox (collider) de cada entidade do jogo
struct Entity {
    Vector2 position = { 0, 0 };
    float speed = 5.0f;
    Collider collider;
};

// Objetos de dano arremessados, com posição independente da entidade atacante
struct Projectile {
    Entity body;
    Vector2 direction = { 0, 0 };
    float screenTime = 0.0f;
    int damage = 10;
    int life = 1;
};

// Toda entidade com movimento e IA próprios com objetivo de matar o player
struct Enemy {
    EnemyType type = ZOMBIE;
    EnemyState enemyState = ROAMING;

    Entity body = {.speed = 2.0f};
    int healthPoints = 50;
    int powerPoints = 5;
    int attackRange = 5;

    bool facingLeft = false;

    float roamingMovementTimer = 0.0f;
    Vector2 roamingDirection = {0.0f, 0.0f};
    int attackFrame = 0;
    float attackFrameTimer = 0.0f;
    float idleTimer = 0.0f;

    // animacao de morte (cada inimigo morre no seu proprio tempo,
    // por isso o frame e os timers sao de cada um, nao compartilhados)
    int deathFrame = 0;
    float deathFrameTimer = 0.0f;
    float deathHoldTimer = 0.0f;
};

// ESTRUTURA DE CONFIGURAÇÕES GERAIS DO JOGO
struct GameStructure {
    GameState gameState = MENU;      // variável que guarda o estado do jogo para recursos de menu e mapa
    static const int TILE = 32;         // tamanho de cada "quadrado" da matriz em pixels
    static const int MAP_W = 94;        // largura (width) do mapa em tiles ("quadrados") na matriz do jogo
    static const int MAP_H = 94;        // altura (height) do mapa em tiles ("quadrados") na matriz do jogo
    // mundo em pixels = MAP_W * TILE = 3008 pixels

    bool debugModeON = false;

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
    int gold = 0;
};
// Instância que guarda as configurações do jogo
GameStructure GAME;


// struct de cada jogador
struct Players {
    Entity body = {.position = GAME.world.Center};
    int healthPoints = 100;
    int speed = 5;
    int attackPower = 10;
    int defense = 0;
    int attackLevel = 0;
    int defenseLevel = 0;
    int movementLevel = 0;

    bool facingLeft = false;

    PlayerSkin skin = KITSUNE;
    PlayerAnimation animation = PLAYER_IDLE;

    Vector2 projectileOffset = { 0, 15 };

    int currentFrame = 0;
    float frameTimer = 0.0f;
};
// Player 1
Players player;

void drawShieldIcon(Vector2 center, Color color) {
    DrawPoly(center, 6, 13.0f, 0.0f, color);
    DrawLineEx({center.x - 7.0f, center.y - 2.0f},
        {center.x + 7.0f, center.y - 2.0f}, 2.0f, DARKGRAY);
}

void drawSwordIcon(Vector2 center, Color color) {
    DrawLineEx({center.x - 10.0f, center.y + 10.0f},
        {center.x + 9.0f, center.y - 9.0f}, 4.0f, color);
    DrawLineEx({center.x - 7.0f, center.y + 1.0f},
        {center.x + 1.0f, center.y + 9.0f}, 3.0f, color);
    DrawLineEx({center.x - 11.0f, center.y + 11.0f},
        {center.x - 5.0f, center.y + 5.0f}, 3.0f, color);
}

void drawWingIcon(Vector2 center, Color color) {
    DrawLineEx(center, {center.x - 14.0f, center.y - 8.0f}, 3.0f, color);
    DrawLineEx(center, {center.x - 14.0f, center.y + 2.0f}, 3.0f, color);
    DrawLineEx(center, {center.x - 9.0f, center.y + 10.0f}, 3.0f, color);
    DrawLineEx(center, {center.x + 14.0f, center.y - 8.0f}, 3.0f, color);
    DrawLineEx(center, {center.x + 14.0f, center.y + 2.0f}, 3.0f, color);
    DrawLineEx(center, {center.x + 9.0f, center.y + 10.0f}, 3.0f, color);
}

void drawStatusNode(Vector2 center, float radius, bool purchased, bool available, int cost, int icon) {
    Color nodeColor = purchased || available ? GRAY : BLACK;
    Color iconColor = purchased || available ? WHITE : GRAY;

    DrawCircleV(center, radius, nodeColor);
    DrawCircleLines((int)center.x, (int)center.y, radius, WHITE);

    if (icon == 0)
        drawSwordIcon(center, iconColor);
    else if (icon == 1)
        drawShieldIcon(center, iconColor);
    else if (icon == 2)
        drawWingIcon(center, iconColor);

    if (!purchased)
        DrawText(TextFormat("%d", cost), (int)center.x - 5, (int)center.y + radius + 3, 12, WHITE);
}

void drawDefenseStatusNode(
    Vector2 center,
    float radius,
    bool purchased,
    bool available,
    int cost,
    Texture2D defenseIcon
) {
    drawStatusNode(center, radius, purchased, available, cost, -1);

    float iconSize = radius * 1.55f;
    DrawTexturePro(
        defenseIcon,
        (Rectangle){ 0, 0, (float)defenseIcon.width, (float)defenseIcon.height },
        (Rectangle){ center.x - iconSize / 2.0f, center.y - iconSize / 2.0f, iconSize, iconSize },
        (Vector2){ 0, 0 },
        0.0f,
        purchased || available ? WHITE : GRAY
    );
}

struct PlayerSprites {
    Texture2D idle;
    Texture2D run;
    Texture2D attack;
    Texture2D dead;
    Vector2 offset = { 4.0f, 0.0f };    // ajuste do boneco dentro do frame
};

PlayerSprites kitsuneSprites;
PlayerSprites gangsterSprites;
PlayerSprites soldierSprites;

struct EnemySprites {
    Texture2D walk;
    Texture2D attack;
    Texture2D dead;
};

EnemySprites zombieSprites;

// FIM DAS STRUCTS
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
// INICIO DAS FUNCOES

// Vetor unidimensional que guarda os tipos de tiles
vector<TileType> tiles(GAME.MAP_W * GAME.MAP_H, FLOOR);
TileType& tileAt(int x, int y) { return tiles[y * GAME.MAP_W + x]; }        // Função que retorna o tipo do tile na posição (x, y)


// Verifica automaticamente se a posição (x, y) é uma parede na matriz do jogo
bool isWall(float x, float y) {
    int tileX = (int)(x / GAME.TILE), tileY = (int)(y / GAME.TILE);

    if (tileX < 0 || tileY < 0 || tileX >= GAME.MAP_W || tileY >= GAME.MAP_H) return true;

    return tileAt(tileX, tileY) == WALL;
}


// pega a caixa de hitbox atual do collider de acordo com a posição
Rectangle getHitbox(Vector2 pos, const Collider& c) {
    return {
        pos.x + c.offset.x - c.size.x/2,
        pos.y + c.offset.y - c.size.y/2,
        c.size.x,
        c.size.y
    };
}


// Gera os inimigos na volta do personagem
void createEnemies(vector<Enemy> &enemies, int enemiesAmount){
    uniform_int_distribution<int> ranHeight(0, GAME.MAP_H * GAME.TILE - 40);
    uniform_int_distribution<int> ranWidth(0, GAME.MAP_W * GAME.TILE - 40);

    for(int i = 0; i < enemiesAmount; i++){
        do {
            enemies[i].body.position = {
                (float)ranWidth(gen),
                (float)ranHeight(gen)
            };

        } while (Vector2Distance(enemies[i].body.position, player.body.position) < 150);

        enemies[i].body.collider.offset = { 0, 25 };
        enemies[i].body.collider.size   = { 20, 50 };
    }

}

// Move os inimigos em direção ao player de acordo com sua posição (x, y)
// OBS: o tamanho do vetor é lido com enemies.size() porque inimigos mortos são
// removidos do vetor durante a partida (a quantidade não é mais fixa).
void enemyMovement(vector<Enemy>& enemies, int enemyIndex) {
    Enemy& enemy = enemies[enemyIndex];

    // Lista de permissao: so anda quem esta ROAMING ou SEARCHING.
    // Assim, qualquer estado novo (DYING, etc.) ja nasce parado por padrao.
    if (enemy.enemyState != EnemyState::ROAMING &&
        enemy.enemyState != EnemyState::SEARCHING)
        return;

    const float deltaTime = GetFrameTime();
    Vector2 direction = { 0.0f, 0.0f };

    if (enemy.enemyState == EnemyState::ROAMING) {
        if (enemy.roamingMovementTimer <= 0.0f) {
            uniform_int_distribution<int> ranDirection(0, 3);
            uniform_real_distribution<float> ranDuration(1.0f, 3.0f);

            switch (ranDirection(gen)) {
                case 0: enemy.roamingDirection = {1.0f, 0.0f}; break;
                case 1: enemy.roamingDirection = {-1.0f, 0.0f}; break;
                case 2: enemy.roamingDirection = {0.0f, 1.0f}; break;
                case 3: enemy.roamingDirection = {0.0f, -1.0f}; break;
            }

            enemy.roamingMovementTimer = ranDuration(gen);

            if (enemy.roamingDirection.x != 0.0f)
                enemy.facingLeft = enemy.roamingDirection.x < 0.0f;
        }

        enemy.roamingMovementTimer -= deltaTime;
        direction = enemy.roamingDirection;
    } else if (enemy.enemyState == EnemyState::SEARCHING) {
        direction = Vector2Subtract(player.body.position, enemy.body.position);
    }

    if (direction.x < 0.0f)
        enemy.facingLeft = true;
    else if (direction.x > 0.0f)
        enemy.facingLeft = false;

    if (Vector2Length(direction) == 0.0f)
        return;

    direction = Vector2Normalize(direction);

    Vector2 nextPosition = Vector2Add(
        enemy.body.position,
        Vector2Scale(direction, enemy.body.speed * 60.0f * deltaTime)
    );

    const float minimumDistance = 15.0f;

    for (int i = 0; i < (int)enemies.size(); i++) {
        // o proprio inimigo e os que estao morrendo nao bloqueiam a passagem
        if (i == enemyIndex || enemies[i].enemyState == EnemyState::DYING)
            continue;

        if (Vector2Distance(nextPosition, enemies[i].body.position) < minimumDistance) {
            return;
        }
    }

    enemy.body.position = nextPosition;
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
                case PLAYER_IDLE: case PLAYER_ATTACK: case PLAYER_RUN: return 8;
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

EnemySprites& getEnemySprites(EnemyType type) {
    switch (type) {
        case ZOMBIE:
        default:
            return zombieSprites;
    }
}

/*
    TODO:
        - sistema de vida e dano do player
        - Sistema de itens
        - drop de gold ao matar inimigos
        - criar diferentes tipos de projéteis
        - criar armas corpo a corpo
        - criar a geração de fases do mundo
*/


// raylib.com
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------

int main() {

    Projectile tempbullet;
    tempbullet.body.position = GAME.world.Center;
    tempbullet.direction = {1, 0};
    tempbullet.damage = 0;
    tempbullet.body.collider.offset = { 0, 0 };
    tempbullet.body.collider.size = { 10, 8 };

    const int enemiesAmount = 20;
    GAME.enemies.resize(enemiesAmount);

    // Initialization
    //-------------------------------------------------------------------------------------
    ChangeDirectory(GetApplicationDirectory());                         // Muda o diretório de inicialiçação para o diretório onde se encontra o executável
    InitWindow(GAME.screen.Width, GAME.screen.Height, "Roguelike");     // INICIALIZAÇÃO DA JANELA (INIT-WINDOW)
    InitAudioDevice();
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(KEY_NULL);           // Retira o ESC como tecla de fechar o jogo

    Music menuMusic = LoadMusicStream("resources/audio/menuMusic.ogg");
    Music gameMusic = LoadMusicStream("resources/audio/gameMusic.ogg");
    menuMusic.looping = true;
    gameMusic.looping = true;
    int musicVolume = 15;
    SetMusicVolume(menuMusic, musicVolume / 100.0f);
    SetMusicVolume(gameMusic, musicVolume / 100.0f);
    PlayMusicStream(menuMusic);

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

    zombieSprites.walk =
        loadSprite("resources/sprites/enemy/zombie/Walk.png", WHITE);

    zombieSprites.attack =
        loadSprite("resources/sprites/enemy/zombie/Attack.png", WHITE);
    zombieSprites.dead =
        loadSprite("resources/sprites/enemy/zombie/Dead.png", WHITE);

    //
    //--------------------------------------------------------------------------------------

    //-------------------------------------------------------------------------------------
    // variables
    Texture2D background = LoadTexture("resources/backgrounds/grassBackground.png");
    Texture2D menuBackground = LoadTexture("resources/backgrounds/menuBackground.png");
    Texture2D skillTreeBackground = LoadTexture("resources/backgrounds/skillTreeBackground.png");
    Texture2D defenseStatsIcon = LoadTexture("resources/statusImages/defenseStats.png");
    Image playerImage = LoadImage("resources/sprites/player/kitsune/Run.png");
    ImageColorReplace(&playerImage, WHITE, BLANK);
    UnloadImage(playerImage);
    Image enemyImage = LoadImage("resources/sprites/enemy/zombie/Walk.png");
    ImageColorReplace(&enemyImage, WHITE, BLANK);
    Texture2D enemyZombie = LoadTextureFromImage(enemyImage);
    UnloadImage(enemyImage);
    SetTextureWrap(background, TEXTURE_WRAP_REPEAT);
    Camera2D camera = {0};

    camera.target = (Vector2){player.body.position.x, player.body.position.y};
    camera.offset = GAME.screen.Center;
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    float frameDuration = 0.1f;
    int enemyCurrentFrame = 0;
    float enemyFrameTimer = 0.0f;

    const float attackDistance = 15.0f;
    const int enemyDeathFrames = 5;         // Dead.png do zumbi: 640x128 = 5 frames de 128x128
    const float enemyDeathHoldTime = 5.0f;  // tempo caido no ultimo frame antes de sumir
    const Rectangle startButton = {
        GAME.screen.Center.x - 120.0f,
        GAME.screen.Center.y - 30.0f,
        240.0f,
        60.0f
    };
    const Rectangle statusTreeButton = {
        GAME.screen.Center.x - 120.0f,
        GAME.screen.Center.y + 50.0f,
        240.0f,
        60.0f
    };
    const Vector2 statusTreeCenter = { 700.0f, 430.0f };
    const Vector2 attackNodes[] = {
        { 650.0f, 310.0f }, { 570.0f, 240.0f }, { 480.0f, 170.0f }
    };
    const Vector2 defenseNodes[] = {
        { 600.0f, 390.0f }, { 500.0f, 340.0f }, { 390.0f, 270.0f }
    };
    const Vector2 movementNodes[] = {
        { 800.0f, 390.0f }, { 900.0f, 340.0f }, { 1010.0f, 270.0f }
    };
    const Vector2 weaponsNodes[] = {
        { 760.0f, 300.0f }, { 830.0f, 230.0f }, { 920.0f, 170.0f }
    };
    const float statusNodeRadius = 12.0f;
    const int statusUpgradeCosts[] = { 10, 20, 30 };
    const float skillTreeZoom = 1.6f;
    Vector2 skillTreePan = { 0.0f, 0.0f };
    Vector2 skillTreeDragStart = { 0.0f, 0.0f };
    Vector2 skillTreePanStart = { 0.0f, 0.0f };
    bool skillTreeDragging = false;
    bool skillTreeDidDrag = false;

    auto skillTreeToScreen = [&](Vector2 point) {
        return Vector2{
            GAME.screen.Center.x + (point.x - statusTreeCenter.x) * skillTreeZoom + skillTreePan.x,
            GAME.screen.Center.y + (point.y - statusTreeCenter.y) * skillTreeZoom + skillTreePan.y
        };
    };
    auto clampSkillTreePan = [&]() {
        float scaledWidth = skillTreeBackground.width * skillTreeZoom;
        float scaledHeight = skillTreeBackground.height * skillTreeZoom;
        float baseLeft = GAME.screen.Center.x - statusTreeCenter.x * skillTreeZoom;
        float baseTop = GAME.screen.Center.y - statusTreeCenter.y * skillTreeZoom;
        float minPanX = GAME.screen.Width - scaledWidth - baseLeft;
        float maxPanX = -baseLeft;
        float minPanY = GAME.screen.Height - scaledHeight - baseTop;
        float maxPanY = -baseTop;
        skillTreePan.x = Clamp(skillTreePan.x, minPanX, maxPanX);
        skillTreePan.y = Clamp(skillTreePan.y, minPanY, maxPanY);
    };
    const Rectangle pauseWindow = { 440.0f, 150.0f, 400.0f, 420.0f };
    const Rectangle resumeButton = { 490.0f, 260.0f, 300.0f, 55.0f };
    const Rectangle menuButton = { 490.0f, 330.0f, 300.0f, 55.0f };
    const Rectangle settingsButton = { 490.0f, 400.0f, 300.0f, 55.0f };
    const Rectangle settingsBackButton = { 490.0f, 500.0f, 300.0f, 55.0f };
    const Rectangle musicSlider = { 500.0f, 330.0f, 280.0f, 8.0f };
    const Rectangle musicSliderHitArea = { 475.0f, 300.0f, 330.0f, 70.0f };

    player.body.collider.size =     { 32.0f, 64.0f };
    player.body.collider.offset =   { 0.0f, 24.0f };

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {

        // Update
        //----------------------------------------------------------------------------------
        // Update your variables here

        if (GAME.gameState == MENU) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                 CheckCollisionPointRec(GetMousePosition(), startButton)) {
                // Geração da seed e do mapa somente ao iniciar uma partida.
                unsigned int seed = (unsigned int)time(nullptr); // a seed gerada é de acordo com o horário da execução no sistema
                SetRandomSeed(seed);
                gen.seed(seed);   // o mt19937 também precisa da seed, senão os inimigos nascem sempre no mesmo lugar
                GAME.seed = seed; // Guarda a seed na struct do jogo, e a variável local é jogada fora
                printSeedDebug(GAME.seed);

                // Reinicia o estado da partida.
                // Inimigos mortos são apagados do vetor, então o vetor precisa ser
                // refeito do zero a cada nova partida (vida, estado e timers resetados).
                GAME.projectiles.clear();
                GAME.enemies.assign(enemiesAmount, Enemy{});
                createEnemies(GAME.enemies, enemiesAmount);

                tiles.assign(tiles.size(), FLOOR);  // limpa as paredes da partida anterior
                for (int i = 0; i < tiles.size(); i++) {
                    int percentage = GetRandomValue(1, 100);
                    if (percentage <= 15)
                        tiles[i] = WALL;
                }

                StopMusicStream(menuMusic);
                PlayMusicStream(gameMusic);
                GAME.gameState = PLAYING;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), statusTreeButton)) {
                GAME.gameState = STATUS_TREE;
            }
        }

        if (GAME.gameState == STATUS_TREE) {
            Vector2 keyboardPan = { 0.0f, 0.0f };
            if (IsKeyDown(KEY_A))
                keyboardPan.x += 1.0f;
            if (IsKeyDown(KEY_D))
                keyboardPan.x -= 1.0f;
            if (IsKeyDown(KEY_W))
                keyboardPan.y += 1.0f;
            if (IsKeyDown(KEY_S))
                keyboardPan.y -= 1.0f;
            skillTreePan = Vector2Add(
                skillTreePan,
                Vector2Scale(keyboardPan, 500.0f * GetFrameTime())
            );
            clampSkillTreePan();

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                skillTreeDragging = true;
                skillTreeDidDrag = false;
                skillTreeDragStart = GetMousePosition();
                skillTreePanStart = skillTreePan;
            }

            if (skillTreeDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 mousePosition = GetMousePosition();
                Vector2 dragDistance = Vector2Subtract(mousePosition, skillTreeDragStart);
                if (Vector2Length(dragDistance) > 5.0f)
                    skillTreeDidDrag = true;

                skillTreePan = Vector2Add(skillTreePanStart, dragDistance);
                clampSkillTreePan();
            }

            if (skillTreeDragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                if (!skillTreeDidDrag) {
                    Vector2 mousePosition = GetMousePosition();

                    for (int i = 0; i < 3; i++) {
                        Vector2 attackNode = skillTreeToScreen(attackNodes[i]);
                        Vector2 defenseNode = skillTreeToScreen(defenseNodes[i]);
                        Vector2 movementNode = skillTreeToScreen(movementNodes[i]);
                        float nodeRadius = statusNodeRadius * skillTreeZoom;

                        if (i == player.attackLevel &&
                            CheckCollisionPointCircle(mousePosition, attackNode, nodeRadius) &&
                            GAME.gold >= statusUpgradeCosts[i]) {
                            GAME.gold -= statusUpgradeCosts[i];
                            player.attackPower += 5;
                            player.attackLevel++;
                        }

                        if (i == player.defenseLevel &&
                            CheckCollisionPointCircle(mousePosition, defenseNode, nodeRadius) &&
                            GAME.gold >= statusUpgradeCosts[i]) {
                            GAME.gold -= statusUpgradeCosts[i];
                            player.defense += 5;
                            player.defenseLevel++;
                        }

                        if (i == player.movementLevel &&
                            CheckCollisionPointCircle(mousePosition, movementNode, nodeRadius) &&
                            GAME.gold >= statusUpgradeCosts[i]) {
                            GAME.gold -= statusUpgradeCosts[i];
                            player.speed += 1;
                            player.movementLevel++;
                        }
                    }
                }
                skillTreeDragging = false;
            }
        }
        if (IsKeyPressed(KEY_F1)) GAME.debugModeON = (GAME.debugModeON + 1) % 2;
        if (IsKeyPressed(KEY_F2)) player.skin = (PlayerSkin)((player.skin + 1) % 3);

        if (player.frameTimer >= frameDuration) {
            player.frameTimer = 0.0f;
            player.currentFrame = (player.currentFrame + 1) % getPlayerFrameCount();
        }

        if (GAME.gameState == PAUSED &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePosition = GetMousePosition();

            if (CheckCollisionPointRec(mousePosition, resumeButton)) {
                GAME.gameState = PLAYING;
            } else if (CheckCollisionPointRec(mousePosition, menuButton)) {
                StopMusicStream(gameMusic);
                PlayMusicStream(menuMusic);
                GAME.gameState = MENU;
            } else if (CheckCollisionPointRec(mousePosition, settingsButton)) {
                GAME.gameState = SETTINGS;
            }
        }

        if (GAME.gameState == SETTINGS &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(), settingsBackButton)) {
            GAME.gameState = PAUSED;
        }

        if (GAME.gameState == SETTINGS &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(), musicSliderHitArea)) {
            float sliderPosition = Clamp(
                (GetMousePosition().x - musicSlider.x) / musicSlider.width,
                0.0f,
                1.0f
            );
            musicVolume = (int)(roundf(sliderPosition * 20.0f) * 5.0f);
            SetMusicVolume(menuMusic, musicVolume / 100.0f);
            SetMusicVolume(gameMusic, musicVolume / 100.0f);
        }

        if (GAME.gameState == MENU || GAME.gameState == STATUS_TREE)
            UpdateMusicStream(menuMusic);
        else
            UpdateMusicStream(gameMusic);

        if (GAME.gameState == PLAYING) {
            player.frameTimer += GetFrameTime();

            if (player.frameTimer >= frameDuration) {
                player.frameTimer = 0.0f;
                player.currentFrame = (player.currentFrame + 1) % getPlayerFrameCount();
            }
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

            player.body.position.x += movement.x * player.body.speed;
            player.body.position.y += movement.y * player.body.speed;

            player.body.position.x = Clamp(player.body.position.x, 0.0f, (float)GAME.world.Width);
            player.body.position.y = Clamp(player.body.position.y, 0.0f, (float)GAME.world.Height);
        }

        if (IsKeyPressed(KEY_TAB)) {
            if (GAME.gameState == GameState::PLAYING) GAME.gameState = GameState::INVENTORY;
            else if (GAME.gameState == GameState::INVENTORY) GAME.gameState = GameState::PLAYING;
        }
        if (IsKeyPressed(KEY_M))
            if (GAME.gameState == GameState::PLAYING || GAME.gameState == GameState::INVENTORY) GAME.gameState = GameState::MAP;
            else if (GAME.gameState == MAP) GAME.gameState = GameState::PLAYING;
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (GAME.gameState == STATUS_TREE)
                GAME.gameState = MENU;
            else if (GAME.gameState == PLAYING)
                GAME.gameState = PAUSED;
            else if (GAME.gameState == PAUSED)
                GAME.gameState = PLAYING;
            else if (GAME.gameState == SETTINGS)
                GAME.gameState = PAUSED;
        }

        camera.target = player.body.position;

        Vector2 visibleWorldTopLeft = GetScreenToWorld2D(
            (Vector2){0.0f, 0.0f},
            camera
        );
        Vector2 visibleWorldBottomRight = GetScreenToWorld2D(
            (Vector2){(float)GAME.screen.Width, (float)GAME.screen.Height},
            camera
        );
        Rectangle visibleWorld = {
            visibleWorldTopLeft.x,
            visibleWorldTopLeft.y,
            visibleWorldBottomRight.x - visibleWorldTopLeft.x,
            visibleWorldBottomRight.y - visibleWorldTopLeft.y
        };

        if (GAME.gameState == PLAYING) {
            for (int i = 0; i < (int)GAME.enemies.size(); i++) {
                Enemy& enemy = GAME.enemies[i];

                switch (enemy.enemyState) {
                    case EnemyState::DYING: {
                        // Toca a animacao de morte UMA vez e trava no ultimo frame;
                        // dali em diante so conta o tempo caido no chao.
                        if (enemy.deathFrame < enemyDeathFrames - 1) {
                            enemy.deathFrameTimer += GetFrameTime();

                            if (enemy.deathFrameTimer >= frameDuration) {
                                enemy.deathFrameTimer = 0.0f;
                                enemy.deathFrame++;
                            }
                        } else {
                            enemy.deathHoldTimer += GetFrameTime();
                        }
                        break;
                    }

                    case EnemyState::ATTACKING: {
                        enemy.attackFrameTimer += GetFrameTime();

                        if (enemy.attackFrameTimer >= frameDuration) {
                            enemy.attackFrameTimer = 0.0f;

                            if (enemy.attackFrame == 2) {
                                float distance = Vector2DistanceSqr(enemy.body.position, player.body.position);
                                if (distance <= attackDistance * attackDistance) {
                                    player.healthPoints -= enemy.powerPoints;
                                }
                            }

                            enemy.attackFrame++;

                            
                            if (enemy.attackFrame >= 4) {
                                enemy.attackFrame = 0;
                                enemy.enemyState = EnemyState::IDLE;
                                enemy.idleTimer = 0.5f;
                            }
                        }
                        break;
                    }

                    case EnemyState::IDLE: {
                        enemy.idleTimer -= GetFrameTime();

                        if (enemy.idleTimer <= 0.0f)
                            enemy.enemyState = EnemyState::SEARCHING;
                        break;
                    }

                    case EnemyState::ROAMING:
                    case EnemyState::SEARCHING: {
                        // distância do inimigo até o player (é melhor calcular com "Vector2DistanceSqr" pq não usa de raiz quadrada - e raiz quadrada pesa bastante)
                        // porém daí a distância é calculada ao quadrado
                        float distance = Vector2DistanceSqr(enemy.body.position, player.body.position);

                        // e portanto é importante lembrar de calcular o quadrado da distância que se deseja comparar:
                        if (distance <= attackDistance * attackDistance) {
                            enemy.enemyState = EnemyState::ATTACKING;
                            enemy.attackFrame = 0;
                            enemy.attackFrameTimer = 0.0f;
                        } else if (CheckCollisionPointRec(enemy.body.position, visibleWorld)) {
                            enemy.enemyState = EnemyState::SEARCHING;
                        } else {
                            enemy.enemyState = EnemyState::ROAMING;
                        }
                        break;
                    }
                }

                enemyMovement(GAME.enemies, i);
            }
        }

        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);

        camera.target = (Vector2){player.body.position.x, player.body.position.y};

        if (GAME.gameState == PLAYING) {
            if (IsCursorOnScreen() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 direction = Vector2Normalize(Vector2Subtract(mouseWorld, player.body.position));

                Projectile p;
                p.body.position = Vector2Add(player.body.position, player.projectileOffset);
                p.body.speed    = 10.0f;
                p.direction     = direction;
                p.damage        = 10;
                p.body.collider.offset = { 0, 0 };
                p.body.collider.size = { 10, 8 };
                GAME.projectiles.push_back(p);
            }

            for (int i = (int)GAME.projectiles.size() - 1; i >= 0; i--) {
                Projectile& bullet = GAME.projectiles[i];

                bullet.screenTime += GetFrameTime();

                // Colisão das balas com os inimigos
                for (int j = (int)GAME.enemies.size() - 1; j >= 0; j--) {
                    Enemy& enemy = GAME.enemies[j];

                    // quem esta morrendo perdeu a hitbox: a bala passa direto
                    // (zerar o collider nao basta - CheckCollisionRecs ainda
                    //  acusa colisao para um retangulo degenerado por dentro de outro)
                    if (enemy.enemyState == EnemyState::DYING)
                        continue;

                    if (CheckCollisionRecs(
                        getHitbox(bullet.body.position, bullet.body.collider),
                        getHitbox(enemy.body.position, enemy.body.collider))) {
                            bullet.life--;
                            enemy.healthPoints -= bullet.damage;
                        }

                    if (bullet.life <= 0) break;
                }

                if (bullet.screenTime > 5.0f || bullet.life <= 0) {
                    GAME.projectiles.erase(GAME.projectiles.begin() + i);
                    continue;
                }

                bullet.body.position = Vector2Add(bullet.body.position, Vector2Scale(bullet.direction, bullet.body.speed));
            }

            // Inimigo sem vida entra em DYING (perde a hitbox e para de agir);
            // so sai do vetor depois da animacao + o tempo caido no chao.
            for (int i = (int)GAME.enemies.size() - 1; i >= 0; i--) {
                Enemy& enemy = GAME.enemies[i];

                if (enemy.healthPoints <= 0 && enemy.enemyState != EnemyState::DYING) {
                    enemy.enemyState = EnemyState::DYING;
                    enemy.deathFrame = 0;
                    enemy.deathFrameTimer = 0.0f;
                    enemy.deathHoldTimer = 0.0f;
                    enemy.body.collider.size = { 0.0f, 0.0f };   // sem hitbox a partir daqui
                }

                if (enemy.enemyState == EnemyState::DYING &&
                    enemy.deathHoldTimer >= enemyDeathHoldTime) {
                    GAME.enemies.erase(GAME.enemies.begin() + i);
                }
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
            if (GAME.gameState == MENU) {
                DrawTexturePro(
                    menuBackground,
                    (Rectangle){0, 0, (float)menuBackground.width, (float)menuBackground.height},
                    (Rectangle){0, 0, (float)GAME.screen.Width, (float)GAME.screen.Height},
                    (Vector2){0, 0},
                    0.0f,
                    WHITE
                );
                Vector2 mousePosition = GetMousePosition();
                bool startButtonHovered = CheckCollisionPointRec(mousePosition, startButton);

                DrawText("ROGUELIKE", GAME.screen.Center.x - 180.0f, 150.0f, 60, WHITE);

                DrawRectangleRec(
                    startButton,
                    startButtonHovered ? DARKGREEN : GREEN
                );
                DrawRectangleLinesEx(startButton, 3.0f, WHITE);

                DrawText(
                    "INICIAR",
                    startButton.x + startButton.width / 2.0f - MeasureText("INICIAR", 30) / 2.0f,
                    startButton.y + 15.0f,
                    30,
                    WHITE
                );
                bool statusTreeButtonHovered = CheckCollisionPointRec(mousePosition, statusTreeButton);
                DrawRectangleRec(
                    statusTreeButton,
                    statusTreeButtonHovered ? DARKBLUE : BLUE
                );
                DrawRectangleLinesEx(statusTreeButton, 3.0f, WHITE);
                DrawText(
                    "ARVORE DE STATUS",
                    statusTreeButton.x + statusTreeButton.width / 2.0f -
                        MeasureText("ARVORE DE STATUS", 18) / 2.0f,
                    statusTreeButton.y + 21.0f,
                    18,
                    WHITE
                );
            } else if (GAME.gameState == STATUS_TREE) {
                Vector2 treeTopLeft = skillTreeToScreen({ 0.0f, 0.0f });
                DrawTexturePro(
                    skillTreeBackground,
                    (Rectangle){ 0, 0, (float)skillTreeBackground.width, (float)skillTreeBackground.height },
                    (Rectangle){
                        treeTopLeft.x,
                        treeTopLeft.y,
                        skillTreeBackground.width * skillTreeZoom,
                        skillTreeBackground.height * skillTreeZoom
                    },
                    (Vector2){ 0, 0 },
                    0.0f,
                    WHITE
                );

                Vector2 screenCenter = skillTreeToScreen(statusTreeCenter);
                DrawLineEx(screenCenter, skillTreeToScreen(attackNodes[0]), 3.0f, DARKBROWN);
                DrawLineEx(screenCenter, skillTreeToScreen(defenseNodes[0]), 3.0f, DARKBROWN);
                DrawLineEx(screenCenter, skillTreeToScreen(movementNodes[0]), 3.0f, DARKBROWN);
                DrawLineEx(screenCenter, skillTreeToScreen(weaponsNodes[0]), 3.0f, DARKBROWN);
                for (int i = 0; i < 2; i++) {
                    DrawLineEx(skillTreeToScreen(attackNodes[i]), skillTreeToScreen(attackNodes[i + 1]),
                        3.0f, DARKBROWN);
                    DrawLineEx(skillTreeToScreen(defenseNodes[i]), skillTreeToScreen(defenseNodes[i + 1]),
                        3.0f, DARKBROWN);
                    DrawLineEx(skillTreeToScreen(movementNodes[i]), skillTreeToScreen(movementNodes[i + 1]),
                        3.0f, DARKBROWN);
                    DrawLineEx(skillTreeToScreen(weaponsNodes[i]), skillTreeToScreen(weaponsNodes[i + 1]),
                        3.0f, DARKBROWN);
                }

                DrawCircleV(screenCenter, 34.0f, RED);
                DrawCircleV(screenCenter, 22.0f, ORANGE);
                DrawCircleLines((int)screenCenter.x, (int)screenCenter.y, 34.0f, WHITE);

                for (int i = 0; i < 3; i++) {
                    drawStatusNode(skillTreeToScreen(attackNodes[i]), statusNodeRadius * skillTreeZoom,
                        i < player.attackLevel,
                        i == player.attackLevel && GAME.gold >= statusUpgradeCosts[i],
                        statusUpgradeCosts[i], 0);
                    drawDefenseStatusNode(skillTreeToScreen(defenseNodes[i]), statusNodeRadius * skillTreeZoom,
                        i < player.defenseLevel,
                        i == player.defenseLevel && GAME.gold >= statusUpgradeCosts[i],
                        statusUpgradeCosts[i], defenseStatsIcon);
                    drawStatusNode(skillTreeToScreen(movementNodes[i]), statusNodeRadius * skillTreeZoom,
                        i < player.movementLevel,
                        i == player.movementLevel && GAME.gold >= statusUpgradeCosts[i],
                        statusUpgradeCosts[i], 2);
                    drawStatusNode(skillTreeToScreen(weaponsNodes[i]), statusNodeRadius * skillTreeZoom,
                        false, false, statusUpgradeCosts[i], 0);
                }
            } else if (GAME.gameState == MAP) {
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

                    if (GAME.gameState == PLAYING || GAME.gameState == PAUSED ||
                        GAME.gameState == SETTINGS) {

                        for (int i = 0; i < (int)GAME.enemies.size(); i++) {
                            EnemySprites& sprites = getEnemySprites(GAME.enemies[i].type);
                            Texture2D enemyTexture = sprites.walk;
                            int currentFrame = enemyCurrentFrame;

                            if (GAME.enemies[i].enemyState == EnemyState::DYING) {
                                enemyTexture = sprites.dead;
                                currentFrame = GAME.enemies[i].deathFrame;
                            } else if (GAME.enemies[i].enemyState == EnemyState::ATTACKING) {
                                enemyTexture = sprites.attack;
                                currentFrame = GAME.enemies[i].attackFrame;
                            } else if (GAME.enemies[i].enemyState == EnemyState::IDLE) {
                                currentFrame = 0;
                            }

                            Rectangle enemySource = {
                                currentFrame * 128.0f,
                                0.0f,
                                128.0f,
                                128.0f
                            };
                            if (GAME.enemies[i].facingLeft)
                                enemySource.width = -enemySource.width;

                            Rectangle enemyDestination = {
                                GAME.enemies[i].body.position.x - 48.0f,
                                GAME.enemies[i].body.position.y - 48.0f,
                                96.0f,
                                96.0f
                            };

                            DrawTexturePro(
                                enemyTexture,
                                enemySource,
                                enemyDestination,
                                (Vector2){0.0f, 0.0f},
                                0.0f,
                                WHITE
                            );

                            if (GAME.debugModeON && GAME.enemies[i].enemyState != EnemyState::DYING)
                                DrawRectangleLinesEx(getHitbox(GAME.enemies[i].body.position, GAME.enemies[i].body.collider), 1.0f, ORANGE);
                        }
                        if (!GAME.projectiles.empty()) {
                            for (const auto bullet : GAME.projectiles) {
                                Rectangle bulletSprite = {bullet.body.position.x, bullet.body.position.y, 8.0f, 4.0f};
                                DrawRectanglePro(bulletSprite, (Vector2){4.0f, 2.0f}, (atan2f(bullet.direction.y, bullet.direction.x) * RAD2DEG), YELLOW);

                                if (GAME.debugModeON)
                                    DrawRectangleLinesEx(getHitbox(bullet.body.position, bullet.body.collider), 1.0f, ORANGE);
                            }
                        }

                        

                        // tempbullet DEBUG DEBUG DEBUG -----------------------------------------------------------
                        Rectangle tempBulletSprite = {tempbullet.body.position.x, tempbullet.body.position.y, 8.0f, 4.0f};
                        DrawRectanglePro(tempBulletSprite, (Vector2){4.0f, 2.0f}, (atan2f(tempbullet.direction.y, tempbullet.direction.x) * RAD2DEG), YELLOW);
                        DrawRectangleLinesEx(getHitbox(tempbullet.body.position, tempbullet.body.collider), 1.0f, ORANGE);
                        // DEBUG DEBUG DEBUG ----------------------------------------------------------------------

                        


                        // ------- INICIO DO DESENHO DO PLAYER --------------

                        Rectangle playerSource = {
                            player.currentFrame * 128.0f,
                            0.0f,
                            128.0f,
                            128.0f
                        };

                        if (player.facingLeft)
                            playerSource.width = -playerSource.width;

                        const float SPRITE_SIZE = 96.0f;
                        Vector2 off = getPlayerSprites().offset;
                        float dir = player.facingLeft ? -1.0f : 1.0f;

                        Rectangle playerDestination = {
                            player.body.position.x + off.x * dir,
                            player.body.position.y + off.y,
                            SPRITE_SIZE,
                            SPRITE_SIZE
                        };

                        DrawTexturePro(
                            getPlayerTexture(),
                            playerSource,
                            playerDestination,
                            (Vector2){ SPRITE_SIZE / 2.0f, SPRITE_SIZE / 2.0f },
                            0.0f,
                            WHITE
                        );

                        if (GAME.debugModeON) {
                            DrawRectangleLinesEx(getHitbox(player.body.position, player.body.collider), 2.0f, ORANGE);
                            DrawLine(player.body.position.x, player.body.position.y - 60,
                                    player.body.position.x, player.body.position.y + 60, GREEN);
                        }

                        // ------- FIM DO DESENHO DO PLAYER --------------


                    } else if (GAME.gameState == INVENTORY) {
                        DrawRectangle((camera.target.x - GAME.screen.Center.x + GAME.screen.Width/10.0f), (camera.target.y - GAME.screen.Center.y + GAME.screen.Height/10.0f), 8.0f * GAME.screen.Width/10.0f, 8.0f * GAME.screen.Height/10.0f, BLACK);
                    }
                EndMode2D();
            }
            if (GAME.gameState == PAUSED || GAME.gameState == SETTINGS) {
                DrawRectangle(0, 0, GAME.screen.Width, GAME.screen.Height, Fade(BLACK, 0.6f));

                if (GAME.gameState == PAUSED) {
                    DrawRectangleRec(pauseWindow, DARKGRAY);
                    DrawRectangleLinesEx(pauseWindow, 3.0f, WHITE);
                    DrawText("PAUSADO", 555, 185, 36, WHITE);

                    const Rectangle pauseButtons[] = {
                        resumeButton, menuButton, settingsButton
                    };
                    const char* pauseLabels[] = {
                        "VOLTAR AO JOGO", "VOLTAR AO MENU", "CONFIGURACOES"
                    };
                    const Vector2 mousePosition = GetMousePosition();

                    for (int i = 0; i < 3; i++) {
                        bool hovered = CheckCollisionPointRec(mousePosition, pauseButtons[i]);
                        DrawRectangleRec(pauseButtons[i], hovered ? BLUE : DARKBLUE);
                        DrawRectangleLinesEx(pauseButtons[i], 2.0f, WHITE);
                        DrawText(pauseLabels[i],
                            (int)(pauseButtons[i].x + pauseButtons[i].width / 2.0f) -
                                MeasureText(pauseLabels[i], 20) / 2,
                            (int)pauseButtons[i].y + 17, 20, WHITE);
                    }
                } else {
                    DrawRectangleRec(pauseWindow, DARKGRAY);
                    DrawRectangleLinesEx(pauseWindow, 3.0f, WHITE);
                    DrawText("CONFIGURACOES", 500, 205, 32, WHITE);
                    DrawText("VOLUME DA MUSICA", 515, 275, 22, WHITE);
                    const char* volumeText = TextFormat("%d%%", musicVolume);
                    DrawText(volumeText,
                        (int)(musicSlider.x + musicSlider.width / 2.0f) -
                            MeasureText(volumeText, 22) / 2,
                        365, 22, WHITE);

                    DrawRectangleRec(musicSlider, LIGHTGRAY);
                    float sliderProgress = musicVolume / 100.0f;
                    float sliderPosition = musicSlider.x + musicSlider.width * sliderProgress;
                    DrawCircle((int)sliderPosition, (int)(musicSlider.y + musicSlider.height / 2.0f),
                        14, GOLD);
                    DrawCircleLines((int)sliderPosition, (int)(musicSlider.y + musicSlider.height / 2.0f),
                        14, WHITE);

                    const Vector2 mousePosition = GetMousePosition();
                    bool hovered = CheckCollisionPointRec(mousePosition, settingsBackButton);
                    DrawRectangleRec(settingsBackButton, hovered ? BLUE : DARKBLUE);
                    DrawRectangleLinesEx(settingsBackButton, 2.0f, WHITE);
                    DrawText("VOLTAR", 600, 517, 20, WHITE);
                    DrawText("ESC: voltar para a pausa", 520, 590, 18, LIGHTGRAY);
                }
            }
            if (GAME.gameState == MENU || GAME.gameState == STATUS_TREE)
                DrawText(TextFormat("GOLD: %d", GAME.gold), GAME.screen.Width - 180, 20, 24, GOLD);
            if (GAME.gameState == PLAYING || GAME.gameState == PAUSED) {
                DrawRectangle(7, 7, 75, 25, WHITE);
                DrawText(TextFormat("HP: %u", player.healthPoints), 10, 10, 20, RED);
                if (GAME.debugModeON) {
                    if (GAME.gameState == PLAYING)
                        DrawLineV(Vector2Add(GAME.screen.Center, player.projectileOffset), mouseScreen, RED);
                    DrawText(TextFormat("seed: %u", GAME.seed), GAME.screen.Width - 180, 10, 20, WHITE);      // Debug: escreve a seed no canto da tela
                }
            }
        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(background);
    UnloadTexture(menuBackground);
    UnloadTexture(skillTreeBackground);
    UnloadTexture(defenseStatsIcon);

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

    UnloadTexture(zombieSprites.walk);
    UnloadTexture(zombieSprites.attack);
    UnloadTexture(zombieSprites.dead);
    UnloadMusicStream(menuMusic);
    UnloadMusicStream(gameMusic);
    CloseAudioDevice();
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
