#include "game.h"

/* All timings are logic ticks (30 Hz), ranges/speeds are screen pixels.
 * Changing these tables changes both the combat rules and the HUD. */
#define SHOT(name, damage, cooldown, range, speed, element) \
    {name, MOVE_PROJECTILE, damage, cooldown, range, speed, 0, element}
#define AREA(name, damage, cooldown, range, element) \
    {name, MOVE_AREA, damage, cooldown, range, 0, 0, element}
#define DASH(name, damage, cooldown, range, distance, element) \
    {name, MOVE_DASH, damage, cooldown, range, 0, distance, element}
#define MELEE(name, damage, cooldown, range, element) \
    {name, MOVE_MELEE, damage, cooldown, range, 0, 0, element}
#define COLOR(r,g,b) ((r) | ((g) << 5) | ((b) << 10))

const uint16_t elementColors[ELEMENT_COUNT] = {
    COLOR(31,12,3), COLOR(7,22,31), COLOR(12,30,8), COLOR(31,28,3), COLOR(29,29,29)
};
const PokemonDef starters[STARTER_COUNT] = {
    {"CHARMANDER", "CHARMELEON", {
        SHOT("BRASAS", 7, 12, 144, 6, ELEMENT_FIRE),
        AREA("PIROTECNIA", 16, 60, 40, ELEMENT_FIRE),
        DASH("NITROCARGA", 12, 90, 34, 28, ELEMENT_FIRE),
        SHOT("LANCACHAMAS", 27, 120, 200, 8, ELEMENT_FIRE)},
        SHOT("FOGO DRACONICO", 65, 0, 240, 8, ELEMENT_FIRE), 160, 0},
    {"SQUIRTLE", "WARTORTLE", {
        SHOT("JATO DAGUA", 7, 11, 156, 7, ELEMENT_WATER),
        AREA("BOLHAS", 13, 54, 50, ELEMENT_WATER),
        DASH("AQUA JET", 16, 84, 34, 36, ELEMENT_WATER),
        SHOT("HIDROBOMBA", 31, 132, 208, 7, ELEMENT_WATER)},
        SHOT("MAREMOTO", 70, 0, 240, 7, ELEMENT_WATER), 178, -1},
    {"BULBASAUR", "IVYSAUR", {
        MELEE("CHICOTE", 8, 12, 48, ELEMENT_GRASS),
        SHOT("FOLHAS", 14, 54, 140, 6, ELEMENT_GRASS),
        DASH("INVESTIDA", 13, 78, 34, 28, ELEMENT_NORMAL),
        AREA("PETALAS", 25, 114, 64, ELEMENT_GRASS)},
        SHOT("RAIO SOLAR", 68, 0, 240, 8, ELEMENT_GRASS), 172, 1},
    {"PIKACHU", "RAICHU", {
        SHOT("CHOQUE", 5, 9, 136, 8, ELEMENT_ELECTRIC),
        AREA("FAISCA", 14, 48, 40, ELEMENT_ELECTRIC),
        DASH("ATAQUE RAPIDO", 10, 72, 34, 40, ELEMENT_NORMAL),
        SHOT("TROVAO", 24, 120, 200, 9, ELEMENT_ELECTRIC)},
        SHOT("TEMPESTADE", 63, 0, 240, 9, ELEMENT_ELECTRIC), 150, 2}
};

const MoveDef *gameMoveDef(const Game *g, int slot) {
    if (slot < 0 || slot >= MOVE_COUNT) return 0;
    static const MoveDef finals[3][4] = {
        {SHOT("CHAMA AEREA",10,12,160,7,ELEMENT_FIRE),AREA("EXPLOSAO",20,60,50,ELEMENT_FIRE),DASH("VOO RASANTE",16,90,40,36,ELEMENT_FIRE),SHOT("INFERNO",32,120,224,9,ELEMENT_FIRE)},
        {SHOT("CANHAO AGUA",10,11,180,8,ELEMENT_WATER),AREA("REDOMOINHO",18,54,56,ELEMENT_WATER),DASH("AQUA JET",20,84,40,40,ELEMENT_WATER),SHOT("HIDROCANHAO",35,132,224,8,ELEMENT_WATER)},
        {MELEE("CIPO FORTE",12,12,56,ELEMENT_GRASS),SHOT("FOLHA NAVAL",18,54,164,7,ELEMENT_GRASS),DASH("INVESTIDA",18,78,40,32,ELEMENT_NORMAL),AREA("FLORESCER",30,114,72,ELEMENT_GRASS)}
    };
    if (gameStage(g)==2 && g->selectedPokemon<3) return &finals[g->selectedPokemon][slot];
    return &starters[g->selectedPokemon].moves[slot];
}
int gameEnemySpecies(const Game *g) {
    if(!gameIsHorde(g->mode)) return g->opponentPokemon;
    if(g->wave==2) return 1;
    if(g->wave==3) return 3;
    return g->selectedPokemon==2?0:2;
}
int gameRivalSpecies(const Game *g) {return (g->opponentPokemon+1)%STARTER_COUNT;}
int gamePlayerSpecies(const Game *g) {
    return g->selectedPokemon+4*gameStage(g);
}
int gameCpuWindupTicks(const Game *g) {
    if(g->cpuMove==0) return 8;
    return gameIsHorde(g->mode) && g->wave==3?22:18;
}
const char *gameWaveName(const Game *g) {
    static const char *names[WAVE_COUNT]={"AQUECIMENTO","COBERTURA","FINAL"};
    return names[g->wave-1];
}
int gameAttackDamage(const Game *g, int slot) {
    const MoveDef *move = gameMoveDef(g, slot);
    return move ? move->damage + starters[g->selectedPokemon].attackBonus + gameStage(g)*3 : 0;
}
const CpuProfile cpuProfiles[4]={{340,18,44,38},{380,14,78,48},{365,15,30,44},{315,25,58,32}};
int gameStage(const Game *g) { return g->evolutionStage ? g->evolutionStage : g->evolved ? 1 : 0; }
int gameFinalStage(const Game *g) { return g->selectedPokemon==3 ? 1 : 2; }
const char *gamePlayerName(const Game *g) {
    static const char *finals[]={"CHARIZARD","BLASTOISE","VENUSAUR"};
    int stage=gameStage(g);
    return stage==2 ? finals[g->selectedPokemon] : stage==1 ? starters[g->selectedPokemon].form : starters[g->selectedPokemon].name;
}
