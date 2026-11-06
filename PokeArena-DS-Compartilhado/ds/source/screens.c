#include "screens.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Pixel bounds and printed labels share this table (8x8 console font). */
const HudButton hudButtons[HUD_BUTTONS] = {
    {4, 72, 122, 36, IN_A}, {130, 72, 122, 36, IN_B},
    {4, 112, 122, 36, IN_X}, {130, 112, 122, 36, IN_Y},
    {4, 154, 78, 34, IN_L}, {88, 154, 78, 34, IN_R},
    {145, 31, 34, 12, IN_SELECT}, {172, 154, 80, 34, IN_START}
};
const HudButton titleTiles[TITLE_TILE_COUNT] = {
    {12,12,112,75,IN_A}, {132,12,112,75,0},
    {12,94,112,75,IN_A}, {132,94,112,75,IN_A}
};
const HudButton optionTiles[OPTION_COUNT] = {
    {24,32,208,34,IN_A}, {24,75,208,34,IN_A}, {24,118,208,34,IN_A}
};
const HudButton modeTiles[MODE_COUNT] = {
    [MODE_VS]={8,32,240,35,IN_A},
    [MODE_DUO]={8,72,240,35,IN_A},
    [MODE_SOLO]={8,112,240,35,IN_A},
    [MODE_TEAM]={8,72,240,35,IN_A}
};
const HudButton pickerTiles[STARTER_COUNT] = {
    {4,30,122,39,0}, {130,30,122,39,0},
    {4,73,122,39,0}, {130,73,122,39,0}
};
const HudButton menuBack={24,166,208,22,IN_B};
const HudButton pauseTiles[4]={
    {20,38,216,34,IN_START}, {20,81,216,34,IN_B},
    {20,134,33,27,IN_L}, {203,134,33,27,IN_R}
};
const HudButton resultTiles[2]={
    {20,99,216,34,IN_START}, {20,143,216,34,IN_B}
};

static bool contains(const HudButton *b,int x,int y) {
    return x>=b->x && x<b->x+b->w && y>=b->y && y<b->y+b->h;
}
uint32_t screensTouchInput(Game *g,int x,int y) {
    if(x<0||x>=256||y<0||y>=192) return 0;
    if(g->state==STATE_FIGHT) return hudHitTest(x,y);
    if(g->state==STATE_TITLE) {
        for(int i=0;i<TITLE_TILE_COUNT;++i) if(contains(&titleTiles[i],x,y)) {
            g->menuCursor=i;
            return titleTiles[i].input;
        }
        return 0;
    }
    if(g->state==STATE_MODE) {
        if(contains(&menuBack,x,y)) return menuBack.input;
        bool horde=g->menuCursor==TITLE_HORDE;
        for(int i=0;i<MODE_COUNT;++i) if(gameIsHorde((GameMode)i)==horde && contains(&modeTiles[i],x,y)) {
            g->mode=(GameMode)i;
            return modeTiles[i].input;
        }
        return 0;
    }
    if(g->state==STATE_OPTIONS) {
        if(contains(&menuBack,x,y)) return menuBack.input;
        for(int i=0;i<OPTION_COUNT;++i) if(contains(&optionTiles[i],x,y)) {
            g->menuCursor=i;
            return optionTiles[i].input;
        }
        return 0;
    }
    if(g->state==STATE_SELECT || g->state==STATE_OPPONENT) {
        if(y>=165) return x<128?IN_B:IN_A;
        for(int n=0;n<STARTER_COUNT;++n) if(contains(&pickerTiles[n],x,y)) {
            if(g->state==STATE_SELECT) g->selectedPokemon=n;
            else {
                int j=0;
                for(int i=0;i<4;++i) if(i!=g->selectedPokemon) { if(j++==n) {g->opponentPokemon=i;break;} }
            }
            break;
        }
    }
    if(g->state==STATE_HELP||g->state==STATE_CREDITS) return contains(&menuBack,x,y)?menuBack.input:0;
    if(g->state==STATE_PAUSE)
        for(int i=0;i<4;++i) if(contains(&pauseTiles[i],x,y)) return pauseTiles[i].input;
    if(g->state==STATE_RESULT)
        for(int i=0;i<2;++i) if(contains(&resultTiles[i],x,y)) return resultTiles[i].input;
    return 0;
}
uint32_t hudHitTest(int x, int y) {
    for (int i = 0; i < HUD_BUTTONS; ++i) {
        const HudButton *b = &hudButtons[i];
        if (contains(b,x,y)) return b->input;
    }
    return 0;
}
static void line(TextScreen *s, int row, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s->lines[row], sizeof(s->lines[row]), format, args);
    va_end(args);
}
static void bar(TextScreen *s, int row, const char *label, int value, int max) {
    char ticks[13];
    int filled = max > 0 ? value * 12 / max : 0;
    for (int i = 0; i < 12; ++i) ticks[i] = i < filled ? '#' : '.';
    ticks[12] = '\0';
    line(s, row, "%s[%s] %3d/%3d", label, ticks, value, max);
}
static void button(TextScreen *s, int index, const char *name, const char *status) {
    int row=10+(index/2)*3, col=(index%2)*16;
    char text[16];
    snprintf(text, sizeof(text), "%-15.15s", name);
    /* Space padding keeps the second column at the same x as the hit box. */
    for (int r = row; r <= row + 1; ++r) {
        size_t length = strlen(s->lines[r]);
        while (length < (size_t)col) s->lines[r][length++] = ' ';
        snprintf(s->lines[r] + col, sizeof(s->lines[r]) - (size_t)col, "%-15.15s", r == row ? text : status);
    }
}
void screensBuild(const Game *g, TextScreen *top, TextScreen *bottom) {
    memset(top, 0, sizeof(*top));
    memset(bottom, 0, sizeof(*bottom));
    const PokemonDef *p = &starters[g->selectedPokemon];
    if (g->state == STATE_TITLE) {
        line(top, 1, GAME_NAME);
        line(top, 6, "Arena 2D e HUD de toque");
        line(top, 9, "A ou START: escolher");
        line(bottom, 2, "VS, ONDAS SOLO E DUPLA");
        line(bottom, 5, "HUD e toque na tela inferior");
        line(bottom, 8, "DIRECIONAL: mover na arena");
        line(bottom, 10, "SELECT confirma a evolucao");
        line(bottom, 11, "A ou START para continuar");
        return;
    }
    if (g->state == STATE_SELECT) {
        line(top, 1, "ESCOLHA SEU POKEMON");
        line(top, 4, "< %-12s >", p->name);
        line(top, 6, "HP BASE: %d", p->hp);
        for (int i = 0; i < MOVE_COUNT; ++i) {
            int tenths = p->moves[i].cooldownTicks * 10 / GAME_HZ;
            line(top, 9+i, "%c: %-14.14s %d.%ds", "ABXY"[i], gameMoveDef(g,i)->name, tenths/10, tenths%10);
        }
        line(top, 16, "ESQ/DIR: trocar Pokemon");
        if (!gameIsHorde(g->mode)) {
            line(top,18,"CPU: %s",starters[g->opponentPokemon].name);
            line(top,20,"L/R: escolher adversario");
        }
        line(bottom, 1, "MODO DE JOGO");
        line(bottom, 4, "%c VS 1x1", g->mode == MODE_VS ? '>' : ' ');
        line(bottom, 6, "%c ONDAS SOLO", g->mode == MODE_SOLO ? '>' : ' ');
        line(bottom, 8, "%c ONDAS DUPLA", g->mode == MODE_DUO ? '>' : ' ');
        line(bottom, 12, "CIMA/BAIXO: modo");
        line(bottom, 15, "A/START: iniciar   B: voltar");
        return;
    }
    if (g->state == STATE_RESULT) {
        line(top, 1, "RESULTADO");
        line(top, 4, "%s", g->result == RESULT_WIN ? "VITORIA!" : g->result == RESULT_TIME ? "TEMPO ESGOTADO" : "DERROTA");
        line(top, 7, "%s", gamePlayerName(g));
        line(top, 9, "%s - ONDA %d", gameModeName(g), g->wave);
        line(bottom, 3, "START: jogar novamente");
        line(bottom, 6, "B: menu inicial");
        line(bottom, 10, "Combate nativo com audio");
        return;
    }
    if (g->state == STATE_PAUSE) {
        line(top, 3, "PAUSA");
        line(bottom, 5, "START: continuar");
        line(bottom, 8, "B: voltar a selecao");
        return;
    }
    int seconds = gameSecondsLeft(g);
    line(top, 0, "%s  %02d:%02d", gameModeName(g), seconds / 60, seconds % 60);
    if(gameIsHorde(g->mode)) line(top,1,"ONDA %d/%d",g->wave,WAVE_COUNT);
    line(top,2,"%s %s",starters[gameEnemySpecies(g)].name,g->cpuWindup > 0 ? "ATENCAO!" : g->enemyDodgeTimer > 0 ? "ESQUIVANDO" : "");
    bar(top, 3, "CPU ", g->enemyHp, g->enemyMaxHp);
    line(bottom, 0, "%s", gamePlayerName(g));
    line(bottom, 1, "%s", g->dodgeTimer > 0 ? "(o o) ESQUIVA" : g->ultimateCharge == ULT_MAX ? "(O O) ULT PRONTA" : "(O O) PRONTO");
    bar(bottom, 3, "HP  ", g->playerHp, gameMaxHp(g));
    bar(bottom, 4, "ULT ", g->ultimateCharge, ULT_MAX);
    bar(bottom, 5, "EVO ", gameEvolutionPercent(g), 100);
    if (g->mode == MODE_DUO || g->mode == MODE_TEAM) line(bottom, 6, "PARCEIRO: %d/%d", g->partnerHp,PARTNER_HP);
    line(bottom, 8, "%s", g->roundDelay > 0 ? "NOVA ONDA - PREPARE-SE" : "A SEGURO: ATAQUE CONTINUO");
    for (int i = 0; i < MOVE_COUNT; ++i) {
        char label[32], status[32];
        snprintf(label, sizeof(label), "[%c] %.11s", "ABXY"[i], gameMoveDef(g,i)->name);
        int tenths = (g->moveCooldowns[i] * 10 + GAME_HZ-1) / GAME_HZ;
        if (g->roundDelay > 0) snprintf(status,sizeof(status),"NOVA ONDA");
        else if (tenths > 0) snprintf(status,sizeof(status),"RECARGA %d.%ds",tenths/10,tenths%10);
        else snprintf(status,sizeof(status),"PRONTO D:%d",gameAttackDamage(g,i));
        button(bottom, i, label, status);
    }
    button(bottom, 4, "[L] ESQUIVA", g->dodgeCooldown > 0 ? "RECARREGANDO" : "PRONTA");
    button(bottom, 5, "[R] ULTIMATE", g->ultimateCharge == ULT_MAX ? "PRONTA" : "CARREGANDO");
    button(bottom, 6, "[SEL] EVOLUIR", gameStage(g)==gameFinalStage(g) ? "FORMA FINAL" : gameEvolutionPercent(g)==100 ? "PRONTA" : "CARREGANDO");
    button(bottom, 7, "[START] PAUSA", "PAUSAR A LUTA");
    line(bottom, 23, "DIRECIONAL: mover / mirar");
}
