#pragma once

#include "raylib.h"
#include <vector>

// -----------------------------------------------------------------------------
// Geracao procedural do mundo (fase 1).
//
// Como funciona, em duas camadas:
//
//   1. PROPS sao objetos livres (posicao e tamanho em pixels), bons para
//      desenhar: arvore e alta, arbusto e baixinho, lago e largo. Nada fica
//      preso a grade de 32px, que e o que faz mundo gerado parecer gerado.
//
//   2. O GRID de tiles recebe um "carimbo" da area que cada prop ocupa. E ele
//      quem responde a colisao: isWall(x, y) vira uma conta e um acesso a
//      vetor, igual para 50 ou 5000 props, em vez de testar a entidade contra
//      todos os props todo frame.
//
// Regra contra entidade presa: nenhum prop e carimbado a menos de
// clearanceTiles de qualquer outro solido ou da borda do mundo. Com 2 tiles
// (64px) e a hitbox do player com 32px de largura, todo corredor que existir
// cabe - nao da para formar bolso fechado.
// -----------------------------------------------------------------------------

// FLOOR = andavel. WALL e WATER = solidos (veja isWall no main.cpp).
enum TileType { FLOOR, WALL, WATER };

enum PropType { PROP_TREE, PROP_ROCK, PROP_BUSH, PROP_POND };

struct Prop {
    PropType type = PROP_TREE;
    Vector2 position = { 0, 0 };   // canto superior esquerdo, em pixels
    Vector2 size = { 32, 32 };     // PLACEHOLDER retangular
};

// Tudo que o gerador precisa saber do jogo. Vem por parametro para este
// arquivo nao depender do GAME nem do player.
struct WorldGenConfig {
    int tileSize = 32;
    int mapWidthInTiles = 157;    // 157 * 32 = 5024px
    int mapHeightInTiles = 157;
    Vector2 playerSpawn = { 0, 0 };
    float spawnClearRadius = 300.0f;  // nenhum prop nasce dentro desse raio
    int clearanceTiles = 2;           // folga minima entre solidos, em tiles
};

// Gera os props e carimba a area solida deles no grid.
// Limpa props e tiles antes de comecar, e chama SetRandomSeed(seed): o mesmo
// seed sempre produz o mesmo mundo, desde que a geracao aconteca sempre no
// mesmo ponto da partida.
void generateWorld(std::vector<Prop>& props,
                   std::vector<TileType>& tiles,
                   const WorldGenConfig& config,
                   unsigned int seed);

// Prop rasteiro (lago, arbusto) e chao: fica SEMPRE embaixo das entidades.
// Prop alto (arvore, pedra) entra na ordenacao por Y junto com player e
// inimigos, para dar para passar por tras dele.
bool isGroundProp(PropType type);

// Desenha so os props rasteiros que aparecem na area visivel.
// Chamar dentro do BeginMode2D, logo depois do fundo.
void drawGroundProps(const std::vector<Prop>& props, Rectangle visibleArea,
                     bool showCollision = false);

// Desenha UM prop. E o que a ordenacao por Y do main usa para intercalar os
// props altos com as entidades. Trocar retangulo por sprite acontece aqui.
void drawProp(const Prop& prop, bool showCollision = false);

// A parte SOLIDA do prop: a base dele, e nao o desenho inteiro.
//
// Num jogo visto de cima, o que bloqueia a passagem e o tronco da arvore, nao a
// copa - o personagem passa por tras dela. Testar a colisao contra este
// retangulo (e nao contra os tiles carimbados, que arredondam para 32px) e o
// que tira a sensacao de esbarrar numa parede de ar em volta do objeto.
// Largura/altura zero = prop atravessavel.
Rectangle getPropCollisionRect(const Prop& prop);

// true se a caixa dada encosta na parte solida de algum prop.
bool propsBlock(const std::vector<Prop>& props, Rectangle box);

// Cor do placeholder de cada tipo.
Color getPropColor(PropType type);