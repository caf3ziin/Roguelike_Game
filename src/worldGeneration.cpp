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
static const float pondsPerMegapixel  = 0.0f;    // desligado ate ter sprite de lago

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

// ----------------------------- SPRITES -----------------------------

static PropSprites propSprites;   // zerado: id = 0 em tudo ate o main entregar

void setPropSprites(const PropSprites& sprites) {
    propSprites = sprites;
}

// Quantas artes diferentes cada tipo tem. Suba junto com a struct PropSprites
// quando adicionar arte nova.
static int propVariantCount(PropType type) {
    switch (type) {
        case PROP_TREE: return 2;
        case PROP_ROCK: return 1;
        case PROP_BUSH: return 1;
        case PROP_POND: return 0;   // sem arte ainda
    }

    return 0;
}

// Textura do prop. id == 0 significa "nao carregada": quem chama cai no
// retangulo de placeholder.
static Texture2D getPropSprite(PropType type, int variant) {
    switch (type) {
        case PROP_TREE: return (variant == 1) ? propSprites.tree2 : propSprites.tree1;
        case PROP_ROCK: return propSprites.rock;
        case PROP_BUSH: return propSprites.bush;
        case PROP_POND: break;   // sem arte ainda
    }

    return Texture2D{};
}

// Onde a arte REALMENTE esta dentro da imagem, em fracao de 0 a 1.
//
// As imagens sao 128x128 mas o desenho nao preenche tudo: a tree2 ocupa so 37%
// da largura. Sem isto, pedir "arvore de 90px" daria uma tree2 de 33px perdida
// no meio de um vazio de 90px - e a geracao ainda reservaria o vazio inteiro.
// Com isto, prop.size e o tamanho da ARTE, e o desenho estica a imagem para
// que a arte caia exatamente ali.
//
// Medido nos PNGs atuais; refaca a conta se trocar a arte.
static Rectangle propContentRect(PropType type, int variant) {
    switch (type) {
        case PROP_TREE:
            return (variant == 1)
                ? (Rectangle){ 0.3125f, 0.1250f, 0.3672f, 0.8750f }   // tree2
                : (Rectangle){ 0.1484f, 0.0703f, 0.6875f, 0.9297f };  // tree1

        case PROP_ROCK: return { 0.1016f, 0.4609f, 0.7891f, 0.5391f };
        case PROP_BUSH: return { 0.1484f, 0.4609f, 0.7188f, 0.5391f };
        case PROP_POND: break;
    }

    return { 0.0f, 0.0f, 1.0f, 1.0f };   // imagem inteira
}

// Proporcao altura/largura da ARTE (nao da imagem). Sai do sprite carregado,
// entao trocar o PNG por outro de formato diferente reajusta o mundo sozinho.
static float propAspectRatio(PropType type, int variant) {
    Texture2D sprite = getPropSprite(type, variant);
    Rectangle content = propContentRect(type, variant);

    if (sprite.id > 0 && sprite.width > 0 && content.width > 0.0f)
        return (content.height * sprite.height) / (content.width * sprite.width);

    // sem textura: proporcao medida na arte, para a geracao nao mudar de cara
    switch (type) {
        case PROP_TREE: return (variant == 1) ? 2.38f : 1.35f;
        case PROP_ROCK: return 0.68f;
        case PROP_BUSH: return 0.75f;
        case PROP_POND: return 0.90f;
    }

    return 1.0f;
}

// Sorteia so a LARGURA da arte; a altura vem da proporcao do sprite. E isso que
// impede a arte de esticar torta: props do mesmo tipo variam de tamanho, nunca
// de formato.
static Vector2 randomPropSize(PropType type, int variant) {
    float width = 32.0f;

    switch (type) {
        case PROP_TREE:
            width = (variant == 1)
                ? (float)GetRandomValue(48, 62)     // tronco alto e fino
                : (float)GetRandomValue(78, 102);   // pinheiro largo
            break;

        case PROP_ROCK: width = (float)GetRandomValue(50, 70); break;
        case PROP_BUSH: width = (float)GetRandomValue(36, 52); break;
        case PROP_POND: width = (float)GetRandomValue(96, 192); break;
    }

    // Sem default no switch de proposito: assim o -Wswitch avisa se um tipo
    // novo entrar no enum e esquecerem de dar tamanho a ele aqui.

    return { width, width * propAspectRatio(type, variant) };
}

// Quanto do desenho e solido, por tipo.
// x = fracao da largura (centralizada), y = fracao da altura (ancorada embaixo).
// {0, 0} significa que da para atravessar o prop.
// Medido na arte: o pinheiro so mostra tronco nos 5% de baixo (os galhos
// descem quase ate o chao), enquanto a tree2 tem tronco limpo por 30% da
// altura. Por isso a fracao e por VARIANTE, nao so por tipo.
static Vector2 propCollisionFraction(PropType type, int variant) {
    switch (type) {
        case PROP_TREE:
            return (variant == 1)
                ? (Vector2){ 0.34f, 0.18f }    // tree2: tronco fino e alto
                : (Vector2){ 0.38f, 0.09f };   // tree1: so a base do pinheiro

        case PROP_ROCK: return { 0.90f, 0.55f };   // a pedra e macica
        case PROP_BUSH: return { 0.00f, 0.00f };   // atravessavel
        case PROP_POND: return { 1.00f, 1.00f };   // a agua inteira
    }

    return { 1.0f, 1.0f };
}

Rectangle getPropCollisionRect(const Prop& prop) {
    Vector2 fraction = propCollisionFraction(prop.type, prop.variant);

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
                         Vector2 size,
                         int variant) {
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

    props.push_back({ type, position, size, variant });
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
                // a arte e sorteada antes do tamanho: e ela que da a proporcao
                int variants = propVariantCount(type);
                int variant = (variants > 1) ? GetRandomValue(0, variants - 1) : 0;

                Vector2 size = randomPropSize(type, variant);
                Vector2 position = {
                    clusterCenter.x + GetRandomValue(-clusterRadius, clusterRadius),
                    clusterCenter.y + GetRandomValue(-clusterRadius, clusterRadius)
                };

                // o prop inteiro precisa caber dentro do mundo
                if (position.x < 0.0f || position.y < 0.0f ||
                    position.x + size.x >= (float)worldWidth ||
                    position.y + size.y >= (float)worldHeight)
                    continue;

                if (tryPlaceProp(props, tiles, config, type, position, size, variant)) {
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
            return true;   // agua: plana, sempre embaixo de todo mundo

        case PROP_TREE:
        case PROP_ROCK:
        case PROP_BUSH:
            return false;  // tem altura: da para ficar atras
    }

    return false;
}

void drawProp(const Prop& prop, bool showCollision) {
    Rectangle rect = { prop.position.x, prop.position.y, prop.size.x, prop.size.y };

    Texture2D sprite = getPropSprite(prop.type, prop.variant);

    if (sprite.id > 0) {
        // rect e o tamanho da ARTE. Como a imagem tem padding transparente em
        // volta, o destino e esticado para fora, de modo que a arte caia
        // exatamente em rect - inclusive com a base encostando embaixo, que e
        // o que a ordenacao por Y e a colisao assumem.
        Rectangle content = propContentRect(prop.type, prop.variant);

        float fullWidth  = rect.width  / content.width;
        float fullHeight = rect.height / content.height;

        Rectangle destination = {
            rect.x - content.x * fullWidth,
            rect.y - content.y * fullHeight,
            fullWidth,
            fullHeight
        };

        DrawTexturePro(
            sprite,
            (Rectangle){ 0, 0, (float)sprite.width, (float)sprite.height },
            destination,
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
    } else {
        // Tipo ainda sem arte: retangulo colorido de placeholder.
        DrawRectangleRec(rect, getPropColor(prop.type));
        DrawRectangleLinesEx(rect, 2.0f, getPropOutlineColor(prop.type));
    }

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
