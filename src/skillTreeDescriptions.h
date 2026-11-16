#pragma once

// -----------------------------------------------------------------------------
// Textos da arvore de status.
//
// Fica separado do main.cpp porque é conteudo, nao logica: cresce a cada ramo
// novo e é editado sozinho, sem precisar rolar mil linhas de jogo.
//
// Nao use acento aqui dentro: a fonte padrao da raylib so desenha ASCII, e por
// isso o resto do jogo tambem escreve "CONFIGURACOES", "ARVORE DE STATUS", etc.
// -----------------------------------------------------------------------------

// Cada ramo da árvore. Serve para escolher a tabela de textos certa.
enum SkillBranch {
    SKILL_ATTACK,
    SKILL_DEFENSE,
    SKILL_MOVEMENT,
    SKILL_WEAPONS,
    SKILL_GOLD
};

// Quantos nós cada ramo tem.
const int SKILL_NODES_PER_BRANCH = 3;

// Valores de balanceamento que aparecem escritos dentro das descrições.
// O main preenche isso com as constantes de verdade do jogo, então o texto
// nunca fica mentindo quando o balanceamento mudar.
struct SkillValues {
    int attackPerLevel = 0;      // dano a mais por nivel
    int defensePerLevel = 0;     // pontos percentuais de reducao de dano por nivel
    int speedPerLevel = 0;       // velocidade a mais por nivel
    int goldChancePerLevel = 0;  // pontos percentuais de chance de drop por nivel
    int goldAmountPerLevel = 0;  // ouro a mais em cada drop por nivel
};

// Nome curto do nó, usado como título do tooltip.
const char* getSkillName(SkillBranch branch, int level);

// Frase explicando o efeito, ja com os numeros preenchidos.
//
// ATENCAO: o retorno sai do TextFormat da raylib, que trabalha com um punhado
// de buffers rotativos internos. Desenhe o texto logo em seguida; nao guarde o
// ponteiro para usar varios frames depois.
const char* getSkillDescription(SkillBranch branch, int level, const SkillValues& values);