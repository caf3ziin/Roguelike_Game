#include "skillTreeDescriptions.h"
#include "raylib.h"   // TextFormat

// -----------------------------------------------------------------------------
// Tabelas de texto. Para editar uma descricao, mexa so aqui.
// Lembrete: nada de acento (fonte padrao da raylib = so ASCII).
// O %d das descricoes e preenchido com as constantes de balanceamento do main.
// -----------------------------------------------------------------------------

static const char* attackNames[SKILL_NODES_PER_BRANCH] = {
    "ATAQUE I",
    "ATAQUE II",
    "ATAQUE III"
};

static const char* attackDescriptions[SKILL_NODES_PER_BRANCH] = {
    "+%d de dano em cada tiro",
    "+%d de dano em cada tiro",
    "+%d de dano em cada tiro"
};

static const char* defenseNames[SKILL_NODES_PER_BRANCH] = {
    "DEFESA I",
    "DEFESA II",
    "DEFESA III"
};

static const char* defenseDescriptions[SKILL_NODES_PER_BRANCH] = {
    "-%d%% do dano que voce recebe",
    "-%d%% do dano que voce recebe",
    "-%d%% do dano que voce recebe"
};

static const char* movementNames[SKILL_NODES_PER_BRANCH] = {
    "VELOCIDADE I",
    "VELOCIDADE II",
    "VELOCIDADE III"
};

static const char* movementDescriptions[SKILL_NODES_PER_BRANCH] = {
    "+%d de velocidade de movimento",
    "+%d de velocidade de movimento",
    "+%d de velocidade de movimento"
};

static const char* weaponsNames[SKILL_NODES_PER_BRANCH] = {
    "ARMA I",
    "ARMA II",
    "ARMA III"
};

static const char* weaponsDescriptions[SKILL_NODES_PER_BRANCH] = {
    "Novas armas ainda nao foram implementadas",
    "Novas armas ainda nao foram implementadas",
    "Novas armas ainda nao foram implementadas"
};

static const char* goldNames[SKILL_NODES_PER_BRANCH] = {
    "SORTE",
    "GANANCIA",
    "FORTUNA"
};

static const char* goldDescriptions[SKILL_NODES_PER_BRANCH] = {
    "+%d%% de chance do inimigo largar ouro",
    "+%d de ouro em cada drop",
    "+%d%% de chance e +%d de ouro em cada drop"
};

// -----------------------------------------------------------------------------

const char* getSkillName(SkillBranch branch, int level) {
    if (level < 0 || level >= SKILL_NODES_PER_BRANCH)
        return "???";

    switch (branch) {
        case SKILL_ATTACK:   return attackNames[level];
        case SKILL_DEFENSE:  return defenseNames[level];
        case SKILL_MOVEMENT: return movementNames[level];
        case SKILL_WEAPONS:  return weaponsNames[level];
        case SKILL_GOLD:     return goldNames[level];
    }

    // Sem default no switch de proposito: assim o -Wswitch avisa se um ramo
    // novo entrar no enum e esquecerem de tratar aqui.
    return "???";
}

const char* getSkillDescription(SkillBranch branch, int level, const SkillValues& values) {
    if (level < 0 || level >= SKILL_NODES_PER_BRANCH)
        return "Descricao indisponivel";

    switch (branch) {
        case SKILL_ATTACK:
            return TextFormat(attackDescriptions[level], values.attackPerLevel);

        case SKILL_DEFENSE:
            return TextFormat(defenseDescriptions[level], values.defensePerLevel);

        case SKILL_MOVEMENT:
            return TextFormat(movementDescriptions[level], values.speedPerLevel);

        case SKILL_WEAPONS:
            return weaponsDescriptions[level];

        case SKILL_GOLD:
            // o ultimo no do ramo mexe nas duas coisas ao mesmo tempo
            if (level == 0)
                return TextFormat(goldDescriptions[0], values.goldChancePerLevel);
            if (level == 1)
                return TextFormat(goldDescriptions[1], values.goldAmountPerLevel);

            return TextFormat(goldDescriptions[2],
                values.goldChancePerLevel, values.goldAmountPerLevel);
    }

    return "Descricao indisponivel";
}