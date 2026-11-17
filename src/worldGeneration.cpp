#include "worldGeneration.h"

// -----------------------------------------------------------------------------
// DENSIDADE DOS PROPS
//
// Contada por megapixel de mundo (1.000.000 px²), e NAO em numero fixo: assim,
// se o mundo mudar de tamanho, a quantidade se ajusta sozinha e o mapa continua
// com a mesma cara. Uma tela de 1280x720 tem 0,92 megapixel, entao "8 por
// megapixel" da mais ou menos 7 por tela.
//
// Mundo atual (157x157 tiles de 32px = 5024px) = 25,2 megapixels.
// -----------------------------------------------------------------------------
static const float treesPerMegapixel  = 14.0f;   // ~350 no mundo atual
static const float rocksPerMegapixel  = 6.0f;    // ~150
static const float bushesPerMegapixel = 12.0f;   // ~300
static const float pondsPerMegapixel  = 0.4f;    // ~10

// Agrupamento: quantos props por grupo e qual o raio do grupo em pixels.
// E isso que vira bosque e pedral em vez de salpicado por igual.
static const int treesPerCluster  = 6;
static const int treeClusterRadius = 220;
static const int rocksPerCluster  = 4;
static const int rockClusterRadius = 160;
static const int bushesPerCluster = 5;
static const int bushClusterRadius = 180;

// Quantas posicoes tentar antes de desistir de um prop (grupo que caiu em cima
// de um lago, por exemplo). Evita travar o jogo procurando lugar que nao existe.
static const int placementAttempts = 12;

// -----------------------------------------------------------------------------

static Vector2 randomPropSize(PropType type) {
    switch (type) {
        case PROP_TREE: return { (float)GetRandomValue(40, 56), (float)GetRandomValue(56, 72) };
        case PROP_ROCK: return { (float)GetRandomValue(32, 48), (float)GetRandomValue(24, 40) };
        case PROP_BUSH: return { (float)GetRandomValue(24, 32), (float)GetRandomValue(20, 28) };
        case PROP_POND: return { (float)GetRandomValue(96, 192), (float)GetRandomValue(96, 160) };
    }

    // Sem default no switch de proposito: assim o -Wswitch avisa se um tipo
    // novo entrar no enum e esquecerem de dar tamanho a ele aqui.
    return { 32.0f, 32.0f };
}

// Quanto do desenho e solido, por tipo.
// x = fracao da largura (centralizada), y = fracao da altura (ancorada embaixo).
// {0, 0} significa que da para atravessar o prop.
static Vector2 propCollisionFraction(PropType type) {
    switch (type) {
        case PROP_TREE: return { 0.45f, 0.22f };   // so o tronco
        case PROP_ROCK: return { 0.90f, 0.70f };   // quase a pedra inteira
        case PROP_BUSH: return { 0.00f, 0.00f };   // arbusto da para atravessar
        case PROP_POND: return { 1.00f, 1.00f };   // a agua inteira
    }

    return { 1.0f, 1.0f };
}

Rectangle getPropCollisionRect(const Prop& prop) {
    Vector2 fraction = propCollisionFraction(prop.type);

    float width  = prop.size.x * fraction.x;
    float height = prop.size.y * fraction.y;

    return {
        prop.position.x + (prop.size.x - width) / 2.0f,  // centralizado na largura
        prop.position.y + prop.size.y - height,          // encostado na base
        width,
        height
    };
}

bool propsBlock(const std::vector<Prop>& props, Rectangle box) {
    for (const Prop& prop : props) {
        Rectangle solid = getPropCollisionRect(prop);

        if (solid.width <= 0.0f || solid.height <= 0.0f)
            continue;   // prop atravessavel

        if (CheckCollisionRecs(box, solid))
            return true;
    }

    return false;
}

static Color getPropOutlineColor(PropType type) {
    switch (type) {
        case PROP_TREE: return DARKBROWN;
        case PROP_ROCK: return DARKGRAY;
        case PROP_BUSH: return DARKGREEN;
        case PROP_POND: return BLUE;
    }

    return BLACK;
}

Color getPropColor(PropType type) {
    switch (type) {
        case PROP_TREE: return DARKGREEN;
        case PROP_ROCK: return GRAY;
        case PROP_BUSH: return LIME;
        case PROP_POND: return SKYBLUE;
    }

    return MAGENTA;   // tipo sem cor definida aparece berrante de proposito
}

// Tenta encaixar um prop na posicao pedida. So aceita se a area dele MAIS a
// folga de clearanceTiles estiver inteira livre e dentro do mundo; se aceitar,
// carimba os tiles como solidos e guarda o prop.
static bool tryPlaceProp(std::vector<Prop>& props,
                         std::vector<TileType>& tiles,
                         const WorldGenConfig& config,
                         PropType type,
                         Vector2 position,
                         Vector2 size) {
    // area limpa em volta do ponto onde o player nasce
    float centerX = position.x + size.x / 2.0f;
    float centerY = position.y + size.y / 2.0f;
    float spawnX = centerX - config.playerSpawn.x;
    float spawnY = centerY - config.playerSpawn.y;

    if (spawnX * spawnX + spawnY * spawnY <
        config.spawnClearRadius * config.spawnClearRadius)
        return false;

    const int firstTileX = (int)(position.x / config.tileSize);
    const int firstTileY = (int)(position.y / config.tileSize);
    const int lastTileX  = (int)((position.x + size.x - 1) / config.tileSize);
    const int lastTileY  = (int)((position.y + size.y - 1) / config.tileSize);

    // A folga e checada em cima do carimbo (tiles inteiros), e nao da medida em
    // pixels: e o carimbo que a colisao enxerga, entao e ele que precisa sobrar.
    for (int tileY = firstTileY - config.clearanceTiles;
         tileY <= lastTileY + config.clearanceTiles; tileY++) {
        for (int tileX = firstTileX - config.clearanceTiles;
             tileX <= lastTileX + config.clearanceTiles; tileX++) {

            if (tileX < 0 || tileY < 0 ||
                tileX >= config.mapWidthInTiles || tileY >= config.mapHeightInTiles)
                return false;   // encostaria na borda do mundo

            if (tiles[tileY * config.mapWidthInTiles + tileX] != FLOOR)
                return false;   // encostaria em outro solido
        }
    }

    const TileType stamp = (type == PROP_POND) ? WATER : WALL;

    for (int tileY = firstTileY; tileY <= lastTileY; tileY++)
        for (int tileX = firstTileX; tileX <= lastTileX; tileX++)
            tiles[tileY * config.mapWidthInTiles + tileX] = stamp;

    props.push_back({ type, position, size });
    return true;
}

// Espalha props em grupos: sorteia o centro do grupo e joga os props em volta.
// perCluster = 1 com raio 0 faz o prop cair sozinho, espalhado pelo mapa.
static void placeClusteredProps(std::vector<Prop>& props,
                                std::vector<TileType>& tiles,
                                const WorldGenConfig& config,
                                PropType type,
                                int totalCount,
                                int perCluster,
                                int clusterRadius) {
    if (totalCount <= 0)
        return;

    if (perCluster < 1)
        perCluster = 1;

    const int worldWidth = config.mapWidthInTiles * config.tileSize;
    const int worldHeight = config.mapHeightInTiles * config.tileSize;
    const int clusterCount = (totalCount + perCluster - 1) / perCluster;

    int placed = 0;

    for (int cluster = 0; cluster < clusterCount && placed < totalCount; cluster++) {
        Vector2 clusterCenter = {
            (float)GetRandomValue(0, worldWidth),
            (float)GetRandomValue(0, worldHeight)
        };

        for (int i = 0; i < perCluster && placed < totalCount; i++) {
            for (int attempt = 0; attempt < placementAttempts; attempt++) {
                Vector2 size = randomPropSize(type);
                Vector2 position = {
                    clusterCenter.x + GetRandomValue(-clusterRadius, clusterRadius),
                    clusterCenter.y + GetRandomValue(-clusterRadius, clusterRadius)
                };

                // o prop inteiro precisa caber dentro do mundo
                if (position.x < 0.0f || position.y < 0.0f ||
                    position.x + size.x >= (float)worldWidth ||
                    position.y + size.y >= (float)worldHeight)
                    continue;

                if (tryPlaceProp(props, tiles, config, type, position, size)) {
                    placed++;
                    break;
                }
            }
        }
    }
}

void generateWorld(std::vector<Prop>& props,
                   std::vector<TileType>& tiles,
                   const WorldGenConfig& config,
                   unsigned int seed) {
    // Reiniciar o gerador aqui e o que garante mesmo seed = mesmo mundo,
    // independente de quantos sorteios o jogo fez antes.
    SetRandomSeed(seed);

    props.clear();
    tiles.assign((size_t)config.mapWidthInTiles * config.mapHeightInTiles, FLOOR);

    const float worldMegapixels =
        ((float)config.mapWidthInTiles * config.tileSize) *
        ((float)config.mapHeightInTiles * config.tileSize) / 1000000.0f;

    // Lagos primeiro: sao os maiores e precisam do espaco ainda vazio.
    // Depois arvores, pedras e arbustos preenchem o que sobrou.
    placeClusteredProps(props, tiles, config, PROP_POND,
        (int)(pondsPerMegapixel * worldMegapixels), 1, 0);
    placeClusteredProps(props, tiles, config, PROP_TREE,
        (int)(treesPerMegapixel * worldMegapixels), treesPerCluster, treeClusterRadius);
    placeClusteredProps(props, tiles, config, PROP_ROCK,
        (int)(rocksPerMegapixel * worldMegapixels), rocksPerCluster, rockClusterRadius);
    placeClusteredProps(props, tiles, config, PROP_BUSH,
        (int)(bushesPerMegapixel * worldMegapixels), bushesPerCluster, bushClusterRadius);
}

bool isGroundProp(PropType type) {
    switch (type) {
        case PROP_POND:
        case PROP_BUSH:
            return true;   // agua e mato baixo: o personagem passa por cima

        case PROP_TREE:
        case PROP_ROCK:
            return false;  // tem altura: da para ficar atras
    }

    return false;
}

void drawProp(const Prop& prop, bool showCollision) {
    Rectangle rect = { prop.position.x, prop.position.y, prop.size.x, prop.size.y };

    // ---- PLACEHOLDER: trocar por DrawTexturePro quando os sprites existirem ----
    DrawRectangleRec(rect, getPropColor(prop.type));
    DrawRectangleLinesEx(rect, 2.0f, getPropOutlineColor(prop.type));

    if (showCollision) {
        Rectangle solid = getPropCollisionRect(prop);

        if (solid.width > 0.0f && solid.height > 0.0f)
            DrawRectangleLinesEx(solid, 2.0f, RED);
    }
}

void drawGroundProps(const std::vector<Prop>& props, Rectangle visibleArea,
                     bool showCollision) {
    for (const Prop& prop : props) {
        if (!isGroundProp(prop.type))
            continue;   // os altos sao desenhados pela ordenacao por Y

        Rectangle rect = { prop.position.x, prop.position.y, prop.size.x, prop.size.y };

        if (!CheckCollisionRecs(rect, visibleArea))
            continue;   // fora da tela: nem desenha

        drawProp(prop, showCollision);
    }
}