#include "raylib.h"
#include "raymath.h"
#include "debug.h"
#include "skillTreeDescriptions.h"
#include "worldGeneration.h"
#include <algorithm>   // sort, para a ordenacao por Y do desenho
#include <math.h>
#include <stdio.h>   // sscanf, para ler o settings.txt
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

enum GameState { MENU, PLAYING, INVENTORY, MAP, STATUS_TREE, PAUSED, SETTINGS, PLAYER_DYING, GAME_OVER };
enum EnemyState { ROAMING, SEARCHING, ATTACKING, IDLE, DYING};

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

    // animação de morte (cada inimigo morre no seu proprio tempo,
    // por isso o frame e os timers são de cada um, não compartilhados)
    int deathFrame = 0;
    float deathFrameTimer = 0.0f;
    float deathHoldTimer = 0.0f;
};

// Ouro largado no chao por um inimigo morto. Fica parado ate o player chegar
// perto; dentro do raio do imã ele e puxado e coletado ao encostar.
struct GoldDrop {
    Vector2 position = { 0, 0 };
    int value = 1;
    float bobTimer = 0.0f;    // balancinho vertical, puramente visual
    float pullSpeed = 0.0f;   // velocidade atual da atracao (acelera enquanto puxa)
};

// ESTRUTURA DE CONFIGURAÇÕES GERAIS DO JOGO
struct GameStructure {
    GameState gameState = MENU;      // variável que guarda o estado do jogo para recursos de menu e mapa
    static const int TILE = 32;         // tamanho de cada "quadrado" da matriz em pixels
    static const int MAP_W = 157;       // largura (width) do mapa em tiles ("quadrados") na matriz do jogo
    static const int MAP_H = 157;       // altura (height) do mapa em tiles ("quadrados") na matriz do jogo
    // mundo em pixels = MAP_W * TILE = 5024 pixels

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
    vector<GoldDrop> goldDrops;
    int gold = 0;        // ouro total, gasto na arvore de status
    int runGold = 0;     // ouro coletado so nesta run (para a tela de game over)

    // Escala de tempo do mundo: 1.0 = normal. Na morte do player ela cai ate 0,
    // e tudo que anda/anima pelo mundo entra em camera lenta e congela junto.
    float worldTimeScale = 1.0f;
};
// Instância que guarda as configurações do jogo
GameStructure GAME;

// Tempo do frame ja multiplicado pela escala do mundo.
// Com worldTimeScale = 1 e identico ao GetFrameTime(). O que deve ignorar a
// camera lenta (a animacao de morte do player, os timers do game over) continua
// usando GetFrameTime() direto.
float getWorldDeltaTime() { return GetFrameTime() * GAME.worldTimeScale; }


// struct de cada jogador
struct Players {
    Entity body = {.position = GAME.world.Center};
    int healthPoints = 100;
    int maxHealthPoints = 100;   // vida cheia, usada para restaurar no inicio de cada run
    int speed = 5;
    int attackPower = 10;
    int defense = 0;
    int attackLevel = 0;
    int defenseLevel = 0;
    int movementLevel = 0;

    // ramo do ouro: ainda sem nos na arvore, mas as formulas de drop ja leem daqui
    int goldLevel = 0;         // quantos nos do ramo do ouro ja foram comprados
    int goldDropLevel = 0;     // aumenta a CHANCE de dropar
    int goldAmountLevel = 0;   // aumenta a QUANTIDADE dropada

    bool facingLeft = false;

    PlayerSkin skin = KITSUNE;
    PlayerAnimation animation = PLAYER_IDLE;

    Vector2 projectileOffset = { 0, 15 };

    int currentFrame = 0;
    float frameTimer = 0.0f;

    // morte do player
    float deathTimer = 0.0f;       // tempo desde a morte (tempo real), move a camera lenta
    float deathHoldTimer = 0.0f;   // tempo parado no ultimo frame antes do menu
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

void drawCoinIcon(Vector2 center, Color color) {
    DrawCircleLines((int)center.x, (int)center.y, 11.0f, color);
    DrawCircleLines((int)center.x, (int)center.y, 7.0f, color);
    DrawLineEx({center.x, center.y - 8.0f}, {center.x, center.y + 8.0f}, 2.0f, color);
}

void drawStatusNode(Vector2 center, float radius, bool purchased, bool available, int cost, int icon) {
    // comprado = verde cheio; da pra comprar = cinza claro; travado = preto
    Color nodeColor = purchased ? DARKGREEN : (available ? GRAY : BLACK);
    Color ringColor = purchased ? GREEN : WHITE;
    Color iconColor = purchased || available ? WHITE : GRAY;

    DrawCircleV(center, radius, nodeColor);
    DrawCircleLines((int)center.x, (int)center.y, radius, ringColor);

    if (purchased)   // anel um pouco mais grosso para destacar o que ja e seu
        DrawCircleLines((int)center.x, (int)center.y, radius - 1.0f, ringColor);

    if (icon == 0)
        drawSwordIcon(center, iconColor);
    else if (icon == 1)
        drawShieldIcon(center, iconColor);
    else if (icon == 2)
        drawWingIcon(center, iconColor);
    else if (icon == 3)
        drawCoinIcon(center, iconColor);

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
// Props do cenario (arvore, pedra, arbusto, lago), gerados a cada run
vector<Prop> worldProps;
TileType& tileAt(int x, int y) { return tiles[y * GAME.MAP_W + x]; }        // Função que retorna o tipo do tile na posição (x, y)


// Verifica automaticamente se a posição (x, y) é uma parede na matriz do jogo
bool isWall(float x, float y) {
    int tileX = (int)(x / GAME.TILE), tileY = (int)(y / GAME.TILE);

    if (tileX < 0 || tileY < 0 || tileX >= GAME.MAP_W || tileY >= GAME.MAP_H) return true;

    return tileAt(tileX, tileY) != FLOOR;   // WALL e WATER bloqueiam
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


// true se a hitbox nessa posicao cobre algum tile solido.
// Testa a FAIXA INTEIRA de tiles, e nao so os quatro cantos: a hitbox do player
// tem 64px de altura, entao um tile solido caberia inteiro entre dois cantos e
// passaria despercebido.
bool hitsWall(Vector2 position, const Collider& collider) {
    Rectangle box = getHitbox(position, collider);

    // 1) Borda do mundo, testada em pixels (indice de tile negativo trunca para
    //    zero em C++ e deixaria passar quem esta saindo pela esquerda/topo).
    if (box.x < 0.0f || box.y < 0.0f ||
        box.x + box.width  > (float)GAME.world.Width ||
        box.y + box.height > (float)GAME.world.Height)
        return true;

    // 2) Peneira rapida no grid. O carimbo de um prop e sempre MAIOR que a
    //    parte solida dele, entao nenhum tile tocado aqui significa, com
    //    certeza, nenhuma colisao - e sai sem olhar prop nenhum.
    //    Testa a faixa inteira de tiles, e nao so os quatro cantos: a hitbox do
    //    player tem 64px de altura, e um tile caberia inteiro entre dois cantos.
    int firstTileX = (int)(box.x / GAME.TILE);
    int firstTileY = (int)(box.y / GAME.TILE);
    int lastTileX  = (int)((box.x + box.width  - 1) / GAME.TILE);
    int lastTileY  = (int)((box.y + box.height - 1) / GAME.TILE);

    bool maybeBlocked = false;

    for (int tileY = firstTileY; tileY <= lastTileY && !maybeBlocked; tileY++)
        for (int tileX = firstTileX; tileX <= lastTileX; tileX++)
            if (tileAt(tileX, tileY) != FLOOR) {
                maybeBlocked = true;
                break;
            }

    if (!maybeBlocked)
        return false;

    // 3) Confirmacao exata contra a BASE do prop. E este passo que tira a
    //    barreira de ar: o grid arredonda para tiles de 32px, o retangulo nao.
    return propsBlock(worldProps, box);
}


// Move testando UM EIXO DE CADA VEZ. Batendo numa arvore, a entidade para
// naquele eixo mas continua andando no outro: ela desliza contornando o
// obstaculo em vez de grudar nele.
Vector2 moveWithCollision(Vector2 position, Vector2 delta, const Collider& collider) {
    Vector2 result = position;

    Vector2 tryX = { result.x + delta.x, result.y };
    if (!hitsWall(tryX, collider))
        result = tryX;

    Vector2 tryY = { result.x, result.y + delta.y };
    if (!hitsWall(tryY, collider))
        result = tryY;

    return result;
}


// Gera os inimigos na volta do personagem
void createEnemies(vector<Enemy> &enemies, int enemiesAmount){
    uniform_int_distribution<int> ranHeight(0, GAME.MAP_H * GAME.TILE - 40);
    uniform_int_distribution<int> ranWidth(0, GAME.MAP_W * GAME.TILE - 40);

    for(int i = 0; i < enemiesAmount; i++){
        // o collider vem antes da posicao porque o teste de parede usa ele
        enemies[i].body.collider.offset = { 0, 25 };
        enemies[i].body.collider.size   = { 20, 50 };

        // Procura um lugar longe do player e fora de arvore/pedra/lago.
        // O limite de tentativas evita laco infinito se o mapa estiver cheio.
        const int maxSpawnAttempts = 50;

        for (int attempt = 0; attempt < maxSpawnAttempts; attempt++) {
            enemies[i].body.position = {
                (float)ranWidth(gen),
                (float)ranHeight(gen)
            };

            if (Vector2Distance(enemies[i].body.position, player.body.position) >= 150 &&
                !hitsWall(enemies[i].body.position, enemies[i].body.collider))
                break;
        }
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

    const float deltaTime = getWorldDeltaTime();
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

    Vector2 delta = Vector2Subtract(nextPosition, enemy.body.position);
    Vector2 moved = moveWithCollision(enemy.body.position, delta, enemy.body.collider);

    // Bateu numa parede enquanto vagava sem rumo: sorteia outra direcao agora,
    // em vez de ficar empurrando o obstaculo ate o timer de roaming acabar.
    if (enemy.enemyState == EnemyState::ROAMING &&
        moved.x == enemy.body.position.x && moved.y == enemy.body.position.y)
        enemy.roamingMovementTimer = 0.0f;

    enemy.body.position = moved;
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

// ---------------- CONFIGURACOES SALVAS EM DISCO ----------------
// Um txt simples ao lado do executavel. O ChangeDirectory no inicio da main
// garante que esse caminho relativo caia na pasta certa.
const char* settingsFilePath = "settings.txt";

int loadMusicVolume(int defaultVolume) {
    if (!FileExists(settingsFilePath))
        return defaultVolume;

    char* fileText = LoadFileText(settingsFilePath);
    if (fileText == nullptr)
        return defaultVolume;

    int volume = defaultVolume;
    if (sscanf(fileText, "musicVolume=%d", &volume) != 1)
        volume = defaultVolume;   // arquivo existe mas nao esta no formato esperado

    UnloadFileText(fileText);

    // o arquivo pode ter sido editado na mao: nao confie no numero
    return (int)Clamp((float)volume, 0.0f, 100.0f);
}

void saveMusicVolume(int volume) {
    // SaveFileText pede char* (nao const), dai o cast
    SaveFileText(settingsFilePath, (char*)TextFormat("musicVolume=%d\n", volume));
}

/*
    TODO:
        - Sistema de itens
        - criar diferentes tipos de projéteis
        - criar armas corpo a corpo
        - adicionar música "Never Gonna Give You Up - 8 bit" como easter egg
        - criar a geração de fases do mundo
        - criar sistema de xp e levels
        - criar geração de poderes aleatórios a cada level novo
*/


// Item da fila de desenho ordenada por Y. Ver o bloco "ORDENACAO POR Y" no
// desenho do mundo.
enum DrawKind { DRAW_PROP, DRAW_ENEMY, DRAW_PLAYER };

struct DrawItem {
    float sortY;     // o PE do objeto: quem tem o pe mais embaixo fica na frente
    DrawKind kind;
    int index;       // posicao no vetor correspondente ao kind
};


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
    InitWindow(GAME.screen.Width, GAME.screen.Height, "DENUO - A Roguelike Project");     // INICIALIZAÇÃO DA JANELA (INIT-WINDOW)
    InitAudioDevice();
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    SetExitKey(KEY_NULL);           // Retira o ESC como tecla de fechar o jogo

    Music menuMusic = LoadMusicStream("resources/audio/menuMusic.ogg");
    Music gameMusic = LoadMusicStream("resources/audio/gameMusic.ogg");
    menuMusic.looping = true;
    gameMusic.looping = true;
    int musicVolume = loadMusicVolume(15);   // 15 e o padrao de quem nunca abriu as configuracoes
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
    const float playerDeathFrameDuration = 0.15f; // morte do player: mais lenta que os 0,1s do resto
    const float gameOverDelay = 2.0f;             // tempo travado no ultimo frame antes do menu
    const float slowMotionStart = 0.35f;          // velocidade do mundo no instante da morte
    const float slowMotionDuration = 2.0f;        // tempo ate o mundo congelar de vez
    const float gameOverFadeDuration = 0.5f;      // duracao do escurecimento da tela
    // ---- ouro ----
    const float goldBaseDropChance = 0.40f;       // 40% dos inimigos dropam
    const int goldBaseMinValue = 1;               // quantidade minima dropada
    const int goldBaseMaxValue = 3;               // quantidade maxima dropada
    const float goldDropChancePerLevel = 0.10f;   // skill futura: +10 pontos percentuais por nivel
    const int goldValuePerLevel = 1;              // skill futura: +1 no minimo e no maximo
    const float goldMagnetRadius = 90.0f;         // distancia em que o ouro comeca a ser puxado
    const float goldPickupRadius = 16.0f;         // distancia em que o ouro e coletado
    const float goldPullSpeedStart = 60.0f;       // px/s no inicio da atracao
    const float goldPullAcceleration = 900.0f;    // px/s^2 enquanto esta sendo puxado
    const float goldSize = 10.0f;                 // PLACEHOLDER: lado do quadrado amarelo
    float gameOverFadeTimer = 0.0f;
    const Rectangle gameOverWindow = { 440.0f, 230.0f, 400.0f, 260.0f };
    const Rectangle gameOverMenuButton = { 490.0f, 390.0f, 300.0f, 55.0f };
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
    const Rectangle menuSettingsButton = {
        GAME.screen.Center.x - 120.0f,
        GAME.screen.Center.y + 130.0f,
        240.0f,
        60.0f
    };
    // De onde a tela de configuracoes foi aberta: e para ca que o VOLTAR e o ESC
    // devolvem o jogador. Evita precisar de um estado separado so para o menu.
    GameState settingsReturnState = PAUSED;
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
    // Ramo do ouro: sobe reto pelo meio, entre o de ataque e o de armas.
    // No 1 = chance de dropar, no 2 = quantidade, no 3 = os dois.
    const Vector2 goldNodes[] = {
        { 700.0f, 270.0f }, { 700.0f, 180.0f }, { 700.0f, 90.0f }
    };
    const float statusNodeRadius = 12.0f;
    const int statusUpgradeCosts[] = { 10, 20, 30 };
    const int goldUpgradeCosts[] = { 5, 15, 25 };   // mais baratos que os outros ramos

    // Ganho de cada nivel. Estao nomeados porque as descricoes do tooltip leem
    // daqui: mudando o balanceamento, o texto muda junto sozinho.
    const int attackPowerPerLevel = 5;
    const int defensePerLevel = 5;
    const int speedPerLevel = 1;

    const SkillValues skillValues = {
        .attackPerLevel = attackPowerPerLevel,
        .defensePerLevel = defensePerLevel,
        .speedPerLevel = speedPerLevel,
        .goldChancePerLevel = (int)(goldDropChancePerLevel * 100.0f),
        .goldAmountPerLevel = goldValuePerLevel
    };
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

    // Fila de desenho ordenada por Y, remontada todo frame. Declarada aqui fora
    // para reaproveitar a memoria em vez de alocar 60 vezes por segundo.
    vector<DrawItem> drawOrder;

    // O desenho do inimigo e o do player viraram lambdas porque a ordenacao por
    // Y precisa chamar os dois intercalados com os props, em ordem variavel.
    auto drawEnemySprite = [&](int index) {
        Enemy& enemy = GAME.enemies[index];
        EnemySprites& sprites = getEnemySprites(enemy.type);
        Texture2D enemyTexture = sprites.walk;
        int currentFrame = enemyCurrentFrame;

        if (enemy.enemyState == EnemyState::DYING) {
            enemyTexture = sprites.dead;
            currentFrame = enemy.deathFrame;
        } else if (enemy.enemyState == EnemyState::ATTACKING) {
            enemyTexture = sprites.attack;
            currentFrame = enemy.attackFrame;
        } else if (enemy.enemyState == EnemyState::IDLE) {
            currentFrame = 0;
        }

        Rectangle enemySource = { currentFrame * 128.0f, 0.0f, 128.0f, 128.0f };

        if (enemy.facingLeft)
            enemySource.width = -enemySource.width;

        Rectangle enemyDestination = {
            enemy.body.position.x - 48.0f,
            enemy.body.position.y - 48.0f,
            96.0f,
            96.0f
        };

        DrawTexturePro(enemyTexture, enemySource, enemyDestination,
            (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);

        if (GAME.debugModeON && enemy.enemyState != EnemyState::DYING)
            DrawRectangleLinesEx(getHitbox(enemy.body.position, enemy.body.collider),
                1.0f, ORANGE);
    };

    auto drawPlayerSprite = [&]() {
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
    };

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {

        // Update
        //----------------------------------------------------------------------------------
        // Update your variables here

        // Estado em que o frame COMECOU. Toda checagem de clique abaixo usa isto,
        // e nao o GAME.gameState atual.
        //
        // Sem isso, um clique que troca de tela e lido de novo pela tela nova no
        // mesmo frame: o botao CONFIGURACOES do menu (y 490-550) fica em cima do
        // botao VOLTAR da tela de configuracoes (y 500-555), entao o mesmo clique
        // abria a tela e ja saia dela, salvando o settings.txt no caminho.
        const GameState frameState = GAME.gameState;

        if (frameState == MENU) {
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
                GAME.goldDrops.clear();
                GAME.runGold = 0;

                // Reset do player. Os campos sao zerados um a um (e nao com
                // player = Players{}) justamente para preservar o que foi comprado
                // na arvore de status: attackPower, defense, speed e os niveis.
                //
                // Vem ANTES da geracao e do createEnemies de proposito: os dois
                // usam a posicao do player (spawn limpo e distancia minima dos
                // inimigos), e antes ela ainda era a do fim da run passada.
                player.healthPoints = player.maxHealthPoints;
                player.body.speed = (float)player.speed;   // aplica o upgrade de movimento
                player.body.position = GAME.world.Center;
                player.facingLeft = false;
                player.deathTimer = 0.0f;
                player.deathHoldTimer = 0.0f;
                setPlayerAnimation(PLAYER_IDLE);
                player.currentFrame = 0;
                player.frameTimer = 0.0f;
                GAME.worldTimeScale = 1.0f;
                gameOverFadeTimer = 0.0f;

                // ---------------- GERACAO DO MUNDO ----------------
                // Espalha os props e carimba a area solida deles no grid de tiles.
                // Limpa o grid e reinicia o sorteio por dentro, entao a mesma seed
                // sempre devolve o mesmo mundo.
                WorldGenConfig worldConfig;
                worldConfig.tileSize         = GAME.TILE;
                worldConfig.mapWidthInTiles  = GAME.MAP_W;
                worldConfig.mapHeightInTiles = GAME.MAP_H;
                worldConfig.playerSpawn      = GAME.world.Center;

                generateWorld(worldProps, tiles, worldConfig, seed);

                // Depois do mundo pronto: assim os inimigos evitam nascer dentro
                // de arvore, pedra ou lago.
                GAME.enemies.assign(enemiesAmount, Enemy{});
                createEnemies(GAME.enemies, enemiesAmount);

                StopMusicStream(menuMusic);
                PlayMusicStream(gameMusic);
                GAME.gameState = PLAYING;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), statusTreeButton)) {
                GAME.gameState = STATUS_TREE;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), menuSettingsButton)) {
                settingsReturnState = MENU;
                GAME.gameState = SETTINGS;
            }
        }

        if (frameState == STATUS_TREE) {
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
                        Vector2 goldNode = skillTreeToScreen(goldNodes[i]);
                        float nodeRadius = statusNodeRadius * skillTreeZoom;

                        if (i == player.attackLevel &&
                            CheckCollisionPointCircle(mousePosition, attackNode, nodeRadius) &&
                            GAME.gold >= statusUpgradeCosts[i]) {
                            GAME.gold -= statusUpgradeCosts[i];
                            player.attackPower += attackPowerPerLevel;
                            player.attackLevel++;
                        }

                        if (i == player.defenseLevel &&
                            CheckCollisionPointCircle(mousePosition, defenseNode, nodeRadius) &&
                            GAME.gold >= statusUpgradeCosts[i]) {
                            GAME.gold -= statusUpgradeCosts[i];
                            player.defense += defensePerLevel;
                            player.defenseLevel++;
                        }

                        if (i == player.movementLevel &&
                            CheckCollisionPointCircle(mousePosition, movementNode, nodeRadius) &&
                            GAME.gold >= statusUpgradeCosts[i]) {
                            GAME.gold -= statusUpgradeCosts[i];
                            player.speed += speedPerLevel;
                            player.body.speed = (float)player.speed;  // e o corpo que anda
                            player.movementLevel++;
                        }

                        if (i == player.goldLevel &&
                            CheckCollisionPointCircle(mousePosition, goldNode, nodeRadius) &&
                            GAME.gold >= goldUpgradeCosts[i]) {
                            GAME.gold -= goldUpgradeCosts[i];

                            if (i == 0 || i == 2) player.goldDropLevel++;    // chance
                            if (i == 1 || i == 2) player.goldAmountLevel++;  // quantidade

                            player.goldLevel++;
                        }
                    }
                }
                skillTreeDragging = false;
            }
        }
        if (IsKeyPressed(KEY_F1)) GAME.debugModeON = (GAME.debugModeON + 1) % 2;
        // a skin nao muda depois que o player morre: a animacao de morte tem que
        // terminar na mesma skin em que a run foi jogada
        if (IsKeyPressed(KEY_F2) &&
            GAME.gameState != PLAYER_DYING && GAME.gameState != GAME_OVER)
            player.skin = (PlayerSkin)((player.skin + 1) % 3);

        if (frameState == PAUSED &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePosition = GetMousePosition();

            if (CheckCollisionPointRec(mousePosition, resumeButton)) {
                GAME.gameState = PLAYING;
            } else if (CheckCollisionPointRec(mousePosition, menuButton)) {
                StopMusicStream(gameMusic);
                PlayMusicStream(menuMusic);
                GAME.gameState = MENU;
            } else if (CheckCollisionPointRec(mousePosition, settingsButton)) {
                settingsReturnState = PAUSED;
                GAME.gameState = SETTINGS;
            }
        }

        if (frameState == SETTINGS &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(GetMousePosition(), settingsBackButton)) {
            saveMusicVolume(musicVolume);
            GAME.gameState = settingsReturnState;
        }

        if (frameState == SETTINGS &&
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

        // Nas configuracoes, quem manda e a tela de tras: aberta pelo menu, quem
        // precisa continuar sendo alimentada e a musica do menu.
        bool onMenuMusic = GAME.gameState == MENU || GAME.gameState == STATUS_TREE ||
            (GAME.gameState == SETTINGS && settingsReturnState == MENU);

        if (onMenuMusic)
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

            Vector2 playerDelta = {
                movement.x * player.body.speed,
                movement.y * player.body.speed
            };
            player.body.position = moveWithCollision(
                player.body.position, playerDelta, player.body.collider);

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
            else if (GAME.gameState == SETTINGS) {
                saveMusicVolume(musicVolume);
                GAME.gameState = settingsReturnState;
            }
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

        if (GAME.gameState == PLAYING || GAME.gameState == PLAYER_DYING) {
            for (int i = 0; i < (int)GAME.enemies.size(); i++) {
                Enemy& enemy = GAME.enemies[i];

                switch (enemy.enemyState) {
                    case EnemyState::DYING: {
                        // Toca a animacao de morte UMA vez e trava no ultimo frame;
                        // dali em diante so conta o tempo caido no chao.
                        if (enemy.deathFrame < enemyDeathFrames - 1) {
                            enemy.deathFrameTimer += getWorldDeltaTime();

                            if (enemy.deathFrameTimer >= frameDuration) {
                                enemy.deathFrameTimer = 0.0f;
                                enemy.deathFrame++;
                            }
                        } else {
                            enemy.deathHoldTimer += getWorldDeltaTime();
                        }
                        break;
                    }

                    case EnemyState::ATTACKING: {
                        enemy.attackFrameTimer += getWorldDeltaTime();

                        if (enemy.attackFrameTimer >= frameDuration) {
                            enemy.attackFrameTimer = 0.0f;

                            if (enemy.attackFrame == 2 && GAME.gameState == PLAYING) {
                                float distance = Vector2DistanceSqr(enemy.body.position, player.body.position);
                                if (distance <= attackDistance * attackDistance) {
                                    // A defesa corta uma PORCENTAGEM do dano, truncando para
                                    // baixo (assim o upgrade nunca arredonda o dano pra cima)
                                    // e nunca deixando o golpe valer menos que 1.
                                    int damage = (int)(enemy.powerPoints *
                                        (1.0f - player.defense / 100.0f));
                                    if (damage < 1)
                                        damage = 1;

                                    player.healthPoints -= damage;
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
                        enemy.idleTimer -= getWorldDeltaTime();

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

        // ---------------- MORTE DO PLAYER E GAME OVER ----------------
        // Dispara no frame em que a vida zera: tira o controle do player, comeca
        // a animacao de morte e liga a camera lenta.
        if (GAME.gameState == PLAYING && player.healthPoints <= 0) {
            player.healthPoints = 0;           // evita vida negativa no HUD
            GAME.gameState = PLAYER_DYING;
            setPlayerAnimation(PLAYER_DEAD);   // zera currentFrame e frameTimer
            player.deathTimer = 0.0f;
            player.deathHoldTimer = 0.0f;
            gameOverFadeTimer = 0.0f;
        }

        if (GAME.gameState == PLAYER_DYING) {
            // A animacao do player roda em TEMPO REAL: se entrasse na camera lenta
            // ela congelaria no meio junto com o mundo.
            const float realDeltaTime = GetFrameTime();

            // mundo desacelerando de slowMotionStart ate parar de vez
            player.deathTimer += realDeltaTime;
            GAME.worldTimeScale = slowMotionStart *
                (1.0f - Clamp(player.deathTimer / slowMotionDuration, 0.0f, 1.0f));

            if (player.currentFrame < getPlayerFrameCount() - 1) {
                player.frameTimer += realDeltaTime;

                if (player.frameTimer >= playerDeathFrameDuration) {
                    player.frameTimer = 0.0f;
                    player.currentFrame++;
                }
            } else {
                // travado no ultimo frame: conta o tempo de leitura antes do menu
                player.deathHoldTimer += realDeltaTime;

                if (player.deathHoldTimer >= gameOverDelay)
                    GAME.gameState = GAME_OVER;
            }
        }

        if (GAME.gameState == GAME_OVER) {
            GAME.worldTimeScale = 0.0f;        // mundo parado por baixo do menu
            gameOverFadeTimer += GetFrameTime();

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), gameOverMenuButton)) {
                StopMusicStream(gameMusic);
                PlayMusicStream(menuMusic);
                GAME.worldTimeScale = 1.0f;
                GAME.gameState = MENU;
            }
        }

        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);

        camera.target = (Vector2){player.body.position.x, player.body.position.y};

        if (GAME.gameState == PLAYING || GAME.gameState == PLAYER_DYING) {
            // atirar so enquanto vivo; as balas ja no ar seguem na camera lenta
            if (GAME.gameState == PLAYING &&
                IsCursorOnScreen() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 direction = Vector2Normalize(Vector2Subtract(mouseWorld, player.body.position));

                Projectile p;
                p.body.position = Vector2Add(player.body.position, player.projectileOffset);
                p.body.speed    = 10.0f;
                p.direction     = direction;
                p.damage        = player.attackPower;   // upgrade de ataque entra aqui
                p.body.collider.offset = { 0, 0 };
                p.body.collider.size = { 10, 8 };
                GAME.projectiles.push_back(p);
            }

            for (int i = (int)GAME.projectiles.size() - 1; i >= 0; i--) {
                Projectile& bullet = GAME.projectiles[i];

                bullet.screenTime += getWorldDeltaTime();

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

                bullet.body.position = Vector2Add(bullet.body.position,
                    Vector2Scale(bullet.direction, bullet.body.speed * GAME.worldTimeScale));
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

                    // Sorteio do drop de ouro. Os dois niveis entram aqui: um na
                    // chance, outro na quantidade (ambos em 0 ate o ramo existir).
                    uniform_real_distribution<float> ranChance(0.0f, 1.0f);
                    float dropChance = goldBaseDropChance +
                        player.goldDropLevel * goldDropChancePerLevel;

                    if (ranChance(gen) < dropChance) {
                        uniform_int_distribution<int> ranValue(
                            goldBaseMinValue + player.goldAmountLevel * goldValuePerLevel,
                            goldBaseMaxValue + player.goldAmountLevel * goldValuePerLevel);

                        GoldDrop drop;
                        // cai nos pes do inimigo, nao no centro do sprite
                        drop.position = Vector2Add(enemy.body.position, (Vector2){ 0.0f, 25.0f });
                        drop.value = ranValue(gen);
                        GAME.goldDrops.push_back(drop);
                    }
                }

                if (enemy.enemyState == EnemyState::DYING &&
                    enemy.deathHoldTimer >= enemyDeathHoldTime) {
                    GAME.enemies.erase(GAME.enemies.begin() + i);
                }
            }

            // ---------------- OURO NO CHAO ----------------
            // Fora do raio do ima o ouro so fica parado (sem timer: some quando o
            // player pegar). Dentro do raio ele e puxado acelerando ate encostar.
            if (GAME.gameState == PLAYING) {
                Vector2 playerCenter = Vector2Add(player.body.position, player.body.collider.offset);

                for (int i = (int)GAME.goldDrops.size() - 1; i >= 0; i--) {
                    GoldDrop& drop = GAME.goldDrops[i];

                    drop.bobTimer += getWorldDeltaTime();

                    float distance = Vector2Distance(drop.position, playerCenter);
                    bool collected = distance <= goldPickupRadius;

                    if (!collected && distance <= goldMagnetRadius) {
                        if (drop.pullSpeed < goldPullSpeedStart)
                            drop.pullSpeed = goldPullSpeedStart;

                        drop.pullSpeed += goldPullAcceleration * getWorldDeltaTime();

                        float step = drop.pullSpeed * getWorldDeltaTime();

                        if (step >= distance) {
                            // o passo passaria do player: coleta em vez de atravessar
                            // (protege contra o ouro furar o raio de coleta com FPS baixo)
                            collected = true;
                        } else {
                            Vector2 toPlayer = Vector2Normalize(
                                Vector2Subtract(playerCenter, drop.position));
                            drop.position = Vector2Add(drop.position, Vector2Scale(toPlayer, step));
                        }
                    } else if (!collected) {
                        drop.pullSpeed = 0.0f;   // saiu do alcance: para e volta a esperar
                    }

                    if (collected) {
                        GAME.gold += drop.value;
                        GAME.runGold += drop.value;
                        GAME.goldDrops.erase(GAME.goldDrops.begin() + i);
                    }
                }
            }

            enemyFrameTimer += getWorldDeltaTime();
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
            // O menu tambem e desenhado por tras das configuracoes abertas por ele,
            // senao o fundo cairia no desenho do mundo (com inimigos de alguma run velha).
            bool drawingMenu = GAME.gameState == MENU ||
                (GAME.gameState == SETTINGS && settingsReturnState == MENU);

            if (drawingMenu) {
                DrawTexturePro(
                    menuBackground,
                    (Rectangle){0, 0, (float)menuBackground.width, (float)menuBackground.height},
                    (Rectangle){0, 0, (float)GAME.screen.Width, (float)GAME.screen.Height},
                    (Vector2){0, 0},
                    0.0f,
                    WHITE
                );
                Vector2 mousePosition = GetMousePosition();
                // com a janela de configuracoes aberta por cima, os botoes do menu
                // nao respondem mais: nao devem acender com o cursor tambem
                bool menuButtonsActive = GAME.gameState == MENU;
                bool startButtonHovered = menuButtonsActive &&
                    CheckCollisionPointRec(mousePosition, startButton);

                DrawText("DENUO", GAME.screen.Center.x - 100.0f, 150.0f, 60, WHITE);
                DrawText("A Roguelike Project", GAME.screen.Center.x - 150.0f, 200.0f, 30, WHITE);

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
                bool statusTreeButtonHovered = menuButtonsActive &&
                    CheckCollisionPointRec(mousePosition, statusTreeButton);
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

                bool menuSettingsHovered = menuButtonsActive &&
                    CheckCollisionPointRec(mousePosition, menuSettingsButton);
                DrawRectangleRec(
                    menuSettingsButton,
                    menuSettingsHovered ? GRAY : DARKGRAY
                );
                DrawRectangleLinesEx(menuSettingsButton, 3.0f, WHITE);
                DrawText(
                    "CONFIGURACOES",
                    menuSettingsButton.x + menuSettingsButton.width / 2.0f -
                        MeasureText("CONFIGURACOES", 20) / 2.0f,
                    menuSettingsButton.y + 20.0f,
                    20,
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
                DrawLineEx(screenCenter, skillTreeToScreen(goldNodes[0]), 3.0f, DARKBROWN);
                for (int i = 0; i < 2; i++) {
                    DrawLineEx(skillTreeToScreen(attackNodes[i]), skillTreeToScreen(attackNodes[i + 1]),
                        3.0f, DARKBROWN);
                    DrawLineEx(skillTreeToScreen(defenseNodes[i]), skillTreeToScreen(defenseNodes[i + 1]),
                        3.0f, DARKBROWN);
                    DrawLineEx(skillTreeToScreen(movementNodes[i]), skillTreeToScreen(movementNodes[i + 1]),
                        3.0f, DARKBROWN);
                    DrawLineEx(skillTreeToScreen(weaponsNodes[i]), skillTreeToScreen(weaponsNodes[i + 1]),
                        3.0f, DARKBROWN);
                    DrawLineEx(skillTreeToScreen(goldNodes[i]), skillTreeToScreen(goldNodes[i + 1]),
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
                    drawStatusNode(skillTreeToScreen(goldNodes[i]), statusNodeRadius * skillTreeZoom,
                        i < player.goldLevel,
                        i == player.goldLevel && GAME.gold >= goldUpgradeCosts[i],
                        goldUpgradeCosts[i], 3);
                }

                // ---------------- TOOLTIP DO NO SOB O CURSOR ----------------
                // Desenhado por ultimo, para ficar por cima dos nos e das linhas.
                // Some enquanto o jogador esta arrastando a arvore.
                if (!(skillTreeDragging && skillTreeDidDrag)) {
                    const Vector2 cursor = GetMousePosition();
                    const float nodeRadius = statusNodeRadius * skillTreeZoom;

                    const Vector2* branchNodes[] = {
                        attackNodes, defenseNodes, movementNodes, weaponsNodes, goldNodes
                    };
                    const SkillBranch branchIds[] = {
                        SKILL_ATTACK, SKILL_DEFENSE, SKILL_MOVEMENT, SKILL_WEAPONS, SKILL_GOLD
                    };
                    const int* branchCosts[] = {
                        statusUpgradeCosts, statusUpgradeCosts, statusUpgradeCosts,
                        statusUpgradeCosts, goldUpgradeCosts
                    };
                    // nivel atual de cada ramo; -1 no de armas, que nao e comprável
                    const int branchLevels[] = {
                        player.attackLevel, player.defenseLevel, player.movementLevel,
                        -1, player.goldLevel
                    };
                    const int branchCount = 5;

                    int hoveredBranch = -1;
                    int hoveredNode = -1;

                    for (int b = 0; b < branchCount && hoveredBranch < 0; b++) {
                        for (int i = 0; i < SKILL_NODES_PER_BRANCH; i++) {
                            if (CheckCollisionPointCircle(cursor,
                                    skillTreeToScreen(branchNodes[b][i]), nodeRadius)) {
                                hoveredBranch = b;
                                hoveredNode = i;
                                break;
                            }
                        }
                    }

                    if (hoveredBranch >= 0) {
                        SkillBranch branch = branchIds[hoveredBranch];
                        int branchLevel = branchLevels[hoveredBranch];
                        int cost = branchCosts[hoveredBranch][hoveredNode];

                        const char* title = getSkillName(branch, hoveredNode);
                        const char* description =
                            getSkillDescription(branch, hoveredNode, skillValues);

                        const char* status;
                        Color statusColor;

                        if (branch == SKILL_WEAPONS) {
                            status = "EM BREVE";
                            statusColor = GRAY;
                        } else if (hoveredNode < branchLevel) {
                            status = "COMPRADO";
                            statusColor = GREEN;
                        } else if (hoveredNode == branchLevel && GAME.gold >= cost) {
                            status = TextFormat("CUSTO: %d - clique para comprar", cost);
                            statusColor = GOLD;
                        } else if (hoveredNode == branchLevel) {
                            status = TextFormat("CUSTO: %d - ouro insuficiente", cost);
                            statusColor = RED;
                        } else {
                            status = TextFormat("CUSTO: %d - requer o no anterior", cost);
                            statusColor = GRAY;
                        }

                        const int titleSize = 18;
                        const int textSize = 16;
                        const int padding = 10;
                        const int lineGap = 6;

                        int boxWidth = MeasureText(title, titleSize);
                        if (MeasureText(description, textSize) > boxWidth)
                            boxWidth = MeasureText(description, textSize);
                        if (MeasureText(status, textSize) > boxWidth)
                            boxWidth = MeasureText(status, textSize);
                        boxWidth += padding * 2;

                        int boxHeight = padding * 2 + titleSize + textSize * 2 + lineGap * 2;

                        // sai de baixo do cursor e vira para dentro da tela quando nao cabe
                        float boxX = cursor.x + 18.0f;
                        float boxY = cursor.y + 18.0f;

                        if (boxX + boxWidth > GAME.screen.Width)
                            boxX = cursor.x - 18.0f - boxWidth;
                        if (boxY + boxHeight > GAME.screen.Height)
                            boxY = cursor.y - 18.0f - boxHeight;

                        DrawRectangle((int)boxX, (int)boxY, boxWidth, boxHeight, Fade(BLACK, 0.9f));
                        DrawRectangleLines((int)boxX, (int)boxY, boxWidth, boxHeight, WHITE);

                        int textX = (int)boxX + padding;
                        int textY = (int)boxY + padding;

                        DrawText(title, textX, textY, titleSize, WHITE);
                        textY += titleSize + lineGap;
                        DrawText(description, textX, textY, textSize, LIGHTGRAY);
                        textY += textSize + lineGap;
                        DrawText(status, textX, textY, textSize, statusColor);
                    }
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

                    // ---- PROPS RASTEIROS (lago, arbusto) ----
                    // Sao chao: ficam sempre embaixo de todo mundo.
                    drawGroundProps(worldProps, visibleWorld, GAME.debugModeON);

                    // Grid de colisao (F1): e a peneira rapida, nao a colisao em si.
                    if (GAME.debugModeON) {
                        Vector2 beg = GetScreenToWorld2D({0, 0}, camera);
                        Vector2 end = GetScreenToWorld2D({(float)GAME.screen.Width, (float)GAME.screen.Height}, camera);

                        int x0 = (int)(beg.x / GAME.TILE) - 1,  x1 = (int)(end.x / GAME.TILE) + 1;
                        int y0 = (int)(beg.y / GAME.TILE) - 1,  y1 = (int)(end.y / GAME.TILE) + 1;

                        if (x0 < 0) x0 = 0;  if (x1 >= GAME.MAP_W) x1 = GAME.MAP_W - 1;
                        if (y0 < 0) y0 = 0;  if (y1 >= GAME.MAP_H) y1 = GAME.MAP_H - 1;

                        for (int y = y0; y <= y1; y++) {
                            for (int x = x0; x <= x1; x++) {
                                if (tileAt(x, y) != FLOOR)
                                    DrawRectangleLines(x * GAME.TILE, y * GAME.TILE,
                                        GAME.TILE, GAME.TILE, Fade(WHITE, 0.25f));
                            }
                        }
                    }

                    const bool drawEntities =
                        GAME.gameState == PLAYING || GAME.gameState == PAUSED ||
                        GAME.gameState == SETTINGS || GAME.gameState == PLAYER_DYING ||
                        GAME.gameState == GAME_OVER;

                    // ---- OURO NO CHAO (PLACEHOLDER: quadrado amarelo) ----
                    // Tambem e chao: fica embaixo das entidades.
                    // Trocar este bloco por um DrawTexturePro quando o sprite existir.
                    if (drawEntities) {
                        for (const GoldDrop& drop : GAME.goldDrops) {
                            float bob = sinf(drop.bobTimer * 4.0f) * 2.0f;
                            int goldX = (int)(drop.position.x - goldSize / 2.0f);
                            int goldY = (int)(drop.position.y - goldSize / 2.0f + bob);

                            DrawRectangle(goldX, goldY, (int)goldSize, (int)goldSize, GOLD);
                            DrawRectangleLines(goldX, goldY, (int)goldSize, (int)goldSize, ORANGE);

                            if (GAME.debugModeON)
                                DrawCircleLines((int)drop.position.x, (int)drop.position.y,
                                    goldMagnetRadius, YELLOW);
                        }
                    }

                    // ---------------- ORDENACAO POR Y ----------------
                    // Quem esta mais embaixo na tela esta mais perto da camera, e
                    // por isso e desenhado por ultimo. E isto que faz o player
                    // sumir atras da copa da arvore quando passa por tras dela, e
                    // aparecer na frente quando passa na frente - sem isso, ou os
                    // props cobrem sempre, ou nunca.
                    //
                    // A chave de ordenacao e o PE de cada objeto (a base), nunca o
                    // topo: e o pe que diz quem esta mais perto da camera.
                    drawOrder.clear();

                    for (int i = 0; i < (int)worldProps.size(); i++) {
                        const Prop& prop = worldProps[i];

                        if (isGroundProp(prop.type))
                            continue;   // ja saiu no chao

                        Rectangle rect = {
                            prop.position.x, prop.position.y, prop.size.x, prop.size.y
                        };

                        if (!CheckCollisionRecs(rect, visibleWorld))
                            continue;   // fora da tela

                        drawOrder.push_back({ prop.position.y + prop.size.y, DRAW_PROP, i });
                    }

                    if (drawEntities) {
                        for (int i = 0; i < (int)GAME.enemies.size(); i++) {
                            // +50 fixo, e nao o collider: o do inimigo e zerado
                            // quando ele morre, e o corpo caido ainda precisa de
                            // uma chave de ordenacao coerente.
                            drawOrder.push_back({
                                GAME.enemies[i].body.position.y + 50.0f, DRAW_ENEMY, i });
                        }

                        drawOrder.push_back({
                            player.body.position.y + player.body.collider.offset.y +
                                player.body.collider.size.y / 2.0f,
                            DRAW_PLAYER, 0 });
                    }

                    sort(drawOrder.begin(), drawOrder.end(),
                        [](const DrawItem& a, const DrawItem& b) { return a.sortY < b.sortY; });

                    for (const DrawItem& item : drawOrder) {
                        if (item.kind == DRAW_PROP)
                            drawProp(worldProps[item.index], GAME.debugModeON);
                        else if (item.kind == DRAW_ENEMY)
                            drawEnemySprite(item.index);
                        else
                            drawPlayerSprite();
                    }

                    // ---- BALAS ----
                    // Estao no ar: passam por cima de tudo, sem ordenacao.
                    if (drawEntities) {
                        for (const auto& bullet : GAME.projectiles) {
                            Rectangle bulletSprite = {bullet.body.position.x, bullet.body.position.y, 8.0f, 4.0f};
                            DrawRectanglePro(bulletSprite, (Vector2){4.0f, 2.0f}, (atan2f(bullet.direction.y, bullet.direction.x) * RAD2DEG), YELLOW);

                            if (GAME.debugModeON)
                                DrawRectangleLinesEx(getHitbox(bullet.body.position, bullet.body.collider), 1.0f, ORANGE);
                        }
                    }

                    if (GAME.gameState == INVENTORY) {
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
                    DrawText(
                        settingsReturnState == MENU ? "ESC: voltar para o menu"
                                                    : "ESC: voltar para a pausa",
                        520, 590, 18, LIGHTGRAY);
                }
            }
            if (GAME.gameState == GAME_OVER) {
                // escurecimento e menu entram juntos, em fade, sobre o mundo congelado
                float fade = Clamp(gameOverFadeTimer / gameOverFadeDuration, 0.0f, 1.0f);

                DrawRectangle(0, 0, GAME.screen.Width, GAME.screen.Height, Fade(BLACK, 0.7f * fade));

                DrawRectangleRec(gameOverWindow, Fade(DARKGRAY, fade));
                DrawRectangleLinesEx(gameOverWindow, 3.0f, Fade(WHITE, fade));

                DrawText("GAME OVER",
                    (int)GAME.screen.Center.x - MeasureText("GAME OVER", 50) / 2,
                    285, 50, Fade(RED, fade));

                bool gameOverHovered = CheckCollisionPointRec(GetMousePosition(), gameOverMenuButton);
                DrawRectangleRec(gameOverMenuButton, Fade(gameOverHovered ? BLUE : DARKBLUE, fade));
                DrawRectangleLinesEx(gameOverMenuButton, 2.0f, Fade(WHITE, fade));
                DrawText("VOLTAR AO MENU",
                    (int)(gameOverMenuButton.x + gameOverMenuButton.width / 2.0f) -
                        MeasureText("VOLTAR AO MENU", 20) / 2,
                    (int)gameOverMenuButton.y + 17, 20, Fade(WHITE, fade));
            }
            if (GAME.gameState == MENU || GAME.gameState == STATUS_TREE)
                DrawText(TextFormat("GOLD: %d", GAME.gold), GAME.screen.Width - 180, 20, 24, GOLD);
            if (GAME.gameState == PLAYING || GAME.gameState == PAUSED ||
                GAME.gameState == PLAYER_DYING) {
                DrawRectangle(7, 7, 75, 25, WHITE);
                DrawText(TextFormat("HP: %u", player.healthPoints), 10, 10, 20, RED);

                const char* goldText = TextFormat("OURO: %d", GAME.gold);
                DrawRectangle(7, 37, MeasureText(goldText, 20) + 10, 25, WHITE);
                DrawText(goldText, 10, 40, 20, ORANGE);
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
    saveMusicVolume(musicVolume);   // garante o volume salvo mesmo fechando pelo X

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
