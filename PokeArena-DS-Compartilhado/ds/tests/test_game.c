#include "game.h"
#include "arena.h"
#include "screens.h"
#include "visuals.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Game fight(GameMode mode) {
    Game g;
    gameInit(&g);
    g.mode = mode;
    gameStart(&g);
    return g;
}
static void testWaves(void) {
    for (int mode = MODE_VS; mode <= MODE_DUO; ++mode) {
        for (int ultimate = 0; ultimate <= 1; ++ultimate) {
            Game g = fight((GameMode)mode);
            g.partnerHp = 0;
            int count = mode == MODE_VS ? 1 : WAVE_COUNT;
            for (int wave = 1; wave <= count; ++wave) {
                g.enemyHp = 1;
                g.enemyX = 176*POS_SCALE;
                g.enemyY = g.playerY;
                memset(g.moveCooldowns,0,sizeof(g.moveCooldowns));
                g.actionLock = g.roundDelay = 0;
                g.enemyCooldown = g.enemyDodgeCooldown = 10000;
                if (ultimate) { g.ultimateCharge = 100; gameUseUltimate(&g); }
                else gameAttack(&g, 0);
                for (int tick = 0; tick < 40 && g.state == STATE_FIGHT && g.wave == wave; ++tick) gameUpdate(&g,0);
                if (wave < count) {
                    assert(g.state == STATE_FIGHT && g.wave == wave + 1 && g.enemyHp > 0);
                    assert(g.enemyGuard == 0 && g.enemyCooldown > 0);
                } else {
                    assert(g.state == STATE_RESULT && g.result == RESULT_WIN);
                    if(gameIsHorde(g.mode)) {
                        assert(!gameDuelContinues(&g) && !g.duelRound && !g.playerRounds && !g.rivalRounds);
                        gameUpdate(&g,IN_START);
                        assert(g.state==STATE_FIGHT && g.wave==1 && !g.duelRound);
                    }
                }
            }
        }
    }
}
static void testWaveBreakAndTactics(void) {
    Game g=fight(MODE_SOLO);
    g.enemyHp=1;
    g.playerHp=50;
    g.enemyDodgeCooldown=g.enemyCooldown=10000;
    gameAttack(&g,0);
    while(g.wave==1 && g.frameCounter<40) gameUpdate(&g,0);
    assert(g.wave==2 && gameEnemySpecies(&g)==1 && g.roundDelay>0 && g.playerHp>=70);
    int clock=g.fightTicks,experience=g.evolutionTicks;
    uint32_t cast=g.casts[0].serial;
    while(g.roundDelay) gameUpdateInput(&g,IN_B|IN_L,IN_A);
    assert(g.fightTicks==clock && g.evolutionTicks==experience && g.casts[0].serial==cast && !g.dodgeTimer);

    const ArenaProp *rock=&arenaProps[0];
    g.playerX=rock->x*POS_SCALE;g.playerY=(rock->y+32)*POS_SCALE;
    g.enemyX=(rock->x+17)*POS_SCALE;g.enemyY=(rock->y-19)*POS_SCALE;
    g.enemyCooldown=46;
    int x=g.enemyX;
    for(int i=0;i<12;++i) gameUpdate(&g,0);
    assert(g.enemyX<x); /* Squirtle retreats behind the northwest rock to reload. */
}
static void testEvolution(void) {
    for (int pokemon = 0; pokemon < STARTER_COUNT; ++pokemon) {
        Game g = fight(MODE_VS);
        g.selectedPokemon = pokemon;
        gameStart(&g);
        int base = gameMaxHp(&g);
        gameEvolve(&g);
        assert(!g.evolved);
        g.enemyCooldown = g.enemyDodgeCooldown = 10000;
        for (int i = 0; i < EVOLUTION_TICKS; ++i) gameUpdate(&g, 0);
        assert(gameEvolutionPercent(&g) == 100 && !g.evolved);
        gameUpdate(&g, IN_SELECT);
        assert(g.evolved && gameMaxHp(&g) == base + 35 && g.playerHp == base + 35);
        gameUpdate(&g, IN_A);
        assert(g.evolved && gameMaxHp(&g) == base + 35);
        g.playerHp = base;
        gameEvolve(&g);
        assert(g.playerHp == base); /* no repeat healing */
        gameStart(&g);
        assert(!g.evolved && g.evolutionTicks == 0 && gameMaxHp(&g) == base);
    }
}
static void testResultsAndClock(void) {
    Game g = fight(MODE_VS);
    g.enemyCooldown = g.enemyDodgeCooldown = 10000;
    for (int i = 0; i < GAME_HZ - 1; ++i) gameUpdate(&g, 0);
    assert(gameSecondsLeft(&g) == 240);
    gameUpdate(&g, 0);
    assert(gameSecondsLeft(&g) == 239);
    g.playerHp = 1;
    g.enemyCooldown = 0;
    g.enemyHp = 360;
    g.frameCounter = 1; /* CPU pattern 7 would guard; 360 / 23 = 15 -> 7 */
    g.enemyHp = 300;
    g.cpuWindup=1; g.cpuMove=1; g.cpuAimX=g.playerX; g.cpuAimY=g.playerY;
    g.enemyX = g.playerX; g.enemyY = g.playerY;
    gameUpdate(&g, 0);
    assert(g.state == STATE_RESULT && g.result == RESULT_KO);
    int charge = g.ultimateCharge;
    gameAttack(&g, 3); gameUseUltimate(&g); gameEvolve(&g);
    assert(g.ultimateCharge == charge && !g.evolved);
    g = fight(MODE_VS);
    g.fightTicks = 1;
    g.enemyHp = 1;
    g.enemyX = g.playerX; g.enemyY = g.playerY;
    gameUpdate(&g, IN_B);
    assert(g.state == STATE_RESULT && g.result == RESULT_WIN);
    g = fight(MODE_VS);
    g.fightTicks = 1;
    gameUpdate(&g, 0);
    assert(g.state == STATE_RESULT && g.result == RESULT_TIME && gameSecondsLeft(&g) == 0);
    assert(gameDuelContinues(&g) && !g.playerRounds && !g.rivalRounds);
    gameUpdate(&g,IN_START);
    assert(g.state==STATE_FIGHT && g.duelRound==1 && g.introTicks==INTRO_TICKS);
}
static void testDuelRounds(void) {
    Game g=fight(MODE_VS);
    assert(g.duelRound==1 && !g.playerRounds && !g.rivalRounds);
    g.evolutionTicks=EVOLUTION_TICKS;
    gameEvolve(&g);
    assert(gameStage(&g)==1);
    g.evolutionTicks=123;
    g.playerHp=gameMaxHp(&g)-10;
    g.enemyHp=1;
    g.enemyX=g.playerX;
    g.enemyY=g.playerY;
    gameAttack(&g,1);
    assert(g.state==STATE_RESULT && g.result==RESULT_WIN && g.playerRounds==1 && gameDuelContinues(&g));

    gameUpdate(&g,IN_START);
    assert(g.state==STATE_FIGHT && g.duelRound==2 && g.introTicks==INTRO_TICKS);
    assert(g.playerRounds==1 && !g.rivalRounds && g.duelContinuation);
    assert(gameStage(&g)==1 && g.evolutionTicks==123 && g.playerHp==gameMaxHp(&g));
    assert(g.enemyHp==g.enemyMaxHp && !g.moveCooldowns[1] && !g.ultimateCharge);
    while(g.introTicks) gameUpdate(&g,0);
    g.fightTicks=1;
    g.enemyHp=1;
    gameUpdate(&g,0);
    assert(g.state==STATE_RESULT && g.result==RESULT_WIN && g.playerRounds==2 && !gameDuelContinues(&g));
    gameUpdate(&g,IN_START);
    assert(g.duelRound==1 && !g.playerRounds && !g.rivalRounds && !g.duelContinuation && !gameStage(&g));

    g=fight(MODE_TEAM);
    g.playerHp=0;
    gameUpdate(&g,0);
    assert(g.state==STATE_RESULT && g.rivalRounds==1 && gameDuelContinues(&g));
    gameUpdate(&g,IN_START);
    assert(g.duelRound==2 && g.partnerHp==PARTNER_HP && g.rival.hp==TEAM_RIVAL_HP);
    while(g.introTicks) gameUpdate(&g,0);
    g.fightTicks=1;
    g.rival.hp=0;
    gameUpdate(&g,0);
    assert(g.state==STATE_RESULT && g.playerRounds==1 && g.rivalRounds==1 && gameDuelContinues(&g));
    gameUpdate(&g,IN_START);
    assert(g.state==STATE_FIGHT && g.duelRound==3 && g.playerRounds==1 && g.rivalRounds==1);
    while(g.introTicks) gameUpdate(&g,0);
    g.fightTicks=1;
    gameUpdate(&g,0);
    assert(g.state==STATE_RESULT && g.result==RESULT_TIME && gameDuelContinues(&g));
    gameUpdate(&g,IN_START);
    assert(g.duelRound==3 && g.playerRounds==1 && g.rivalRounds==1);
    while(g.introTicks) gameUpdate(&g,0);
    g.playerHp=0;
    gameUpdate(&g,0);
    assert(g.state==STATE_RESULT && g.rivalRounds==2 && !gameDuelContinues(&g));
}
static void testCombat(void) {
    Game g = fight(MODE_SOLO);
    int hp = g.enemyHp;
    gameAttack(&g, 4); gameAttack(&g, -1);
    assert(g.enemyHp == hp);
    gameAttack(&g, 0);
    int hit = g.enemyHp;
    gameAttack(&g, 3);
    assert(g.enemyHp == hit && g.ultimateCharge == 0);
    g.enemyCooldown = g.enemyDodgeCooldown = 10000;
    for (int tick = 0; tick < 20; ++tick) gameUpdate(&g,0);
    assert(g.enemyHp < hp && g.ultimateCharge == 9);
    g.playerHp -= 10;
    g.frameCounter = 11;
    memset(g.moveCooldowns,0,sizeof(g.moveCooldowns));
    g.actionLock = 0;
    gameUpdate(&g, IN_A);
    assert(g.playerHp == gameMaxHp(&g) - 9); /* attacking does not reset regeneration */
    gameUpdate(&g, IN_L);
    assert(g.dodgeTimer == DODGE_TICKS);
    gameUpdate(&g, IN_L);
    assert(g.dodgeTimer == DODGE_TICKS - 1);
    g = fight(MODE_DUO);
    g.partnerCooldown = 1;
    g.enemyHp = 1;
    gameUpdate(&g, 0);
    assert(g.wave == 2 && g.state == STATE_FIGHT);
}
static void testMovementPauseProjectiles(void) {
    Game g = fight(MODE_VS);
    g.enemyCooldown = g.enemyDodgeCooldown = 10000;
    int x = g.playerX;
    gameUpdateInput(&g,0,IN_RIGHT);
    assert(g.playerX > x);
    for (int i = 0; i < 200; ++i) gameUpdateInput(&g,0,IN_RIGHT|IN_DOWN);
    assert(g.playerX <= ARENA_RIGHT*POS_SCALE && g.playerY <= ARENA_BOTTOM*POS_SCALE);
    gameUpdate(&g,IN_START);
    assert(g.state == STATE_PAUSE);
    int time = g.fightTicks; x = g.playerX;
    gameUpdateInput(&g,IN_A,IN_LEFT);
    assert(g.fightTicks == time && g.playerX == x);
    gameUpdate(&g,IN_START);
    assert(g.state == STATE_FIGHT);
    g = fight(MODE_VS);
    g.enemyCooldown = g.enemyDodgeCooldown = 10000;
    g.projectiles[0] = (Projectile){60*POS_SCALE,100*POS_SCALE,4*POS_SCALE,0,10,20,true,false,ELEMENT_FIRE};
    g.projectiles[1] = (Projectile){72*POS_SCALE,100*POS_SCALE,-4*POS_SCALE,0,10,20,false,false,ELEMENT_NORMAL};
    gameUpdate(&g,0);
    assert(g.projectiles[0].life == 0 && g.projectiles[1].life == 0);
    g.projectiles[0] = (Projectile){g.playerX,g.playerY,0,0,10,20,false,false,ELEMENT_NORMAL};
    g.dodgeTimer = 2;
    int hp = g.playerHp;
    gameUpdate(&g,0);
    assert(g.playerHp == hp); /* dodge avoids the projectile */
    g.projectiles[0].x = g.playerX; g.projectiles[0].y = g.playerY;
    gameUpdate(&g,0);
    assert(g.playerHp == hp-10);
}
static void testTouchAndScreens(void) {
    assert(hudHitTest(0,0) == 0 && hudHitTest(-1,90) == 0);
    assert(hudHitTest(256,90) == 0 && hudHitTest(0,192) == 0);
    for (int b = 0; b < HUD_BUTTONS; ++b) {
        const HudButton *r = &hudButtons[b];
        for (int y = r->y; y < r->y + r->h; ++y)
            for (int x = r->x; x < r->x + r->w; ++x)
                assert(hudHitTest(x,y) == r->input);
    }
    for (int state = STATE_TITLE; state <= STATE_PAUSE; ++state)
        for (int mode = MODE_VS; mode <= MODE_DUO; ++mode)
            for (int pokemon = 0; pokemon < STARTER_COUNT; ++pokemon) {
                Game g = fight((GameMode)mode);
                g.state = (GameState)state;
                g.selectedPokemon = pokemon;
                g.evolved = true;
                TextScreen top, bottom;
                screensBuild(&g, &top, &bottom);
                for (int row = 0; row < TEXT_ROWS; ++row) {
                    assert(strlen(top.lines[row]) <= TEXT_COLS);
                    assert(strlen(bottom.lines[row]) <= TEXT_COLS);
                }
                if (state == STATE_FIGHT) {
                    assert(strstr(bottom.lines[10], "[A]") && strstr(bottom.lines[10], "[B]"));
                    assert(strstr(bottom.lines[13], "[X]") && strstr(bottom.lines[13], "[Y]"));
                }
            }
}
static void testIndividualCooldowns(void) {
    for (int pokemon = 0; pokemon < STARTER_COUNT; ++pokemon) {
        Game g = fight(MODE_VS);
        g.selectedPokemon = pokemon;
        gameStart(&g);
        g.enemyCooldown = g.enemyDodgeCooldown = 10000;
        g.enemyHp = g.enemyMaxHp = 10000;
        g.enemyY = g.playerY - 15*POS_SCALE;
        gameAttack(&g,0);
        assert(g.moveCooldowns[0] == gameMoveDef(&g,0)->cooldownTicks);
        assert(g.moveCooldowns[1] == 0 && g.moveCooldowns[2] == 0 && g.moveCooldowns[3] == 0);
        for (int tick = 0; tick < ACTION_LOCK_TICKS; ++tick) gameUpdate(&g,0);
        gameAttack(&g,1);
        assert(g.moveCooldowns[0] > 0 && g.moveCooldowns[1] == gameMoveDef(&g,1)->cooldownTicks);
        for (int tick = 0; tick < ACTION_LOCK_TICKS; ++tick) gameUpdate(&g,0);
        gameAttack(&g,3);
        assert(g.moveCooldowns[0] > 0 && g.moveCooldowns[1] > 0 && g.moveCooldowns[3] > 0);
        gameUpdate(&g,IN_START);
        int cooldown = g.moveCooldowns[3];
        for (int tick = 0; tick < 60; ++tick) gameUpdateInput(&g,0,IN_A);
        assert(g.state == STATE_PAUSE && g.moveCooldowns[3] == cooldown);
        gameUpdate(&g,IN_START);
        gameUpdate(&g,0);
        assert(g.moveCooldowns[3] == cooldown-1);
        gameStart(&g);
        for (int slot = 0; slot < MOVE_COUNT; ++slot) assert(g.moveCooldowns[slot] == 0);
    }
}
static void testHeldBasicAndPriority(void) {
    for (int pokemon = 0; pokemon < STARTER_COUNT; ++pokemon) {
        Game g = fight(MODE_VS);
        g.selectedPokemon = pokemon;
        gameStart(&g);
        g.enemyCooldown = g.enemyDodgeCooldown = 10000;
        int starts = 0, previous = 0;
        for (int tick = 0; tick < 60; ++tick) {
            gameUpdateInput(&g,0,IN_A);
            if (g.moveCooldowns[0] > previous) ++starts;
            previous = g.moveCooldowns[0];
        }
        assert(starts >= 4);
        assert(g.moveCooldowns[1] == 0 && g.moveCooldowns[2] == 0 && g.moveCooldowns[3] == 0);
    }
    Game g = fight(MODE_VS);
    gameUpdateInput(&g,IN_B|IN_X|IN_Y,IN_A);
    assert(g.lastAttack == 1 && g.moveCooldowns[1] > 0);
    assert(g.moveCooldowns[0] == 0 && g.moveCooldowns[2] == 0 && g.moveCooldowns[3] == 0);
    g = fight(MODE_VS);
    gameUpdateInput(&g,0,IN_A);
    gameUpdateInput(&g,IN_B,IN_A);
    assert(g.bufferedMove == 1 && g.moveCooldowns[1] == 0);
    for (int tick = 0; tick < ACTION_LOCK_TICKS; ++tick) gameUpdateInput(&g,0,IN_A);
    assert(g.moveCooldowns[1] > 0 && g.lastAttack == 1 && g.bufferTicks == 0);
    gameUpdate(&g,IN_START);
    assert(g.bufferTicks == 0 && g.bufferedMove == -1);
}
static void testProjectilePoolAndGuard(void) {
    Game g = fight(MODE_VS);
    for (int i = 0; i < PROJECTILE_CAP; ++i) g.projectiles[i].life = 100;
    gameAttack(&g,0);
    assert(g.moveCooldowns[0] == 0 && g.actionLock == 0 && g.attackAnim == 0);
    g.ultimateCharge = ULT_MAX;
    gameUseUltimate(&g);
    assert(g.ultimateCharge == ULT_MAX && g.actionLock == 0);
    for (int guardedAtImpact = 0; guardedAtImpact <= 1; ++guardedAtImpact) {
        g = fight(MODE_VS);
        g.enemyCooldown = g.enemyDodgeCooldown = 10000;
        g.enemyGuard = guardedAtImpact ? 0 : 1000;
        int hp = g.enemyHp;
        int damage = gameAttackDamage(&g,0);
        gameAttack(&g,0);
        g.enemyGuard = guardedAtImpact ? 1000 : 0;
        for (int tick = 0; tick < 25; ++tick) gameUpdate(&g,0);
        assert(g.enemyHp == hp - (guardedAtImpact ? damage*2/3 : damage));
    }
    g = fight(MODE_VS);
    g.enemyX = g.playerX + 60*POS_SCALE;
    g.enemyY = g.playerY - 60*POS_SCALE;
    assert(gameFire(&g,true,7,false));
    Projectile *p = &g.projectiles[0];
    int speedSquared = p->vx*p->vx+p->vy*p->vy;
    int intended = gameMoveDef(&g,0)->speed*POS_SCALE;
    assert(speedSquared <= intended*intended+intended*2);
    assert(speedSquared >= intended*intended-intended*4);
    g = fight(MODE_VS);
    g.playerX = 20*POS_SCALE; g.playerY = 100*POS_SCALE;
    g.enemyX = (20+gameMoveDef(&g,0)->range+12)*POS_SCALE;
    g.enemyY = g.playerY;
    g.enemyCooldown = g.enemyGuard = 10000;
    int hp = g.enemyHp;
    gameAttack(&g,0);
    for (int tick = 0; tick < gameMoveDef(&g,0)->range/gameMoveDef(&g,0)->speed; ++tick) gameUpdate(&g,0);
    assert(g.enemyHp == hp-gameAttackDamage(&g,0)*2/3); /* hit on the last movement tick */
}
static void testMoveDataAndHud(void) {
    for (int pokemon = 0; pokemon < STARTER_COUNT; ++pokemon) {
        Game g = fight(MODE_VS);
        g.selectedPokemon = pokemon;
        gameStart(&g);
        assert(!gameMoveDef(&g,-1) && !gameMoveDef(&g,MOVE_COUNT));
        for (int slot = 0; slot < MOVE_COUNT; ++slot) {
            const MoveDef *m = gameMoveDef(&g,slot);
            assert(m->name && m->name[0] && m->damage > 0 && m->cooldownTicks >= ACTION_LOCK_TICKS);
            assert(m->range > 0 && m->element >= 0 && m->element < ELEMENT_COUNT);
            if (m->kind == MOVE_PROJECTILE) assert(m->speed > 0);
            if (m->kind == MOVE_DASH) assert(m->dashDistance > 0);
        }
        const char *baseNames[MOVE_COUNT];
        for(int slot=0;slot<MOVE_COUNT;++slot) baseNames[slot]=gameMoveDef(&g,slot)->name;
        const char *baseUltimate=gameUltimateDef(&g)->name;
        const char *previousNames[MOVE_COUNT];
        memcpy(previousNames,baseNames,sizeof(previousNames));
        const char *previousUltimate=baseUltimate;
        for(int stage=1;stage<=gameFinalStage(&g);++stage) {
            g.evolutionTicks=stage==1?EVOLUTION_TICKS:900;
            gameEvolve(&g);
            assert(gameStage(&g)==stage);
            for(int slot=0;slot<MOVE_COUNT;++slot) {
                const MoveDef *move=gameMoveDef(&g,slot);
                assert(strcmp(move->name,baseNames[slot]) && strcmp(move->name,previousNames[slot]));
                assert(gameAttackDamage(&g,slot)==move->damage+starters[pokemon].attackBonus+stage*3);
                previousNames[slot]=move->name;
            }
            const MoveDef *ultimate=gameUltimateDef(&g);
            assert(strcmp(ultimate->name,baseUltimate) && strcmp(ultimate->name,previousUltimate));
            assert(ultimate->kind==MOVE_PROJECTILE && ultimate->speed>0);
            previousUltimate=ultimate->name;
        }
        g.moveCooldowns[3] = 37;
        TextScreen top,bottom;
        screensBuild(&g,&top,&bottom);
        assert(strstr(bottom.lines[14],"RECARGA 1.3s"));
        assert(strstr(bottom.lines[11],"PRONTO D:"));
    }
    assert(starters[2].moves[0].kind == MOVE_MELEE);
    assert(starters[2].moves[1].kind == MOVE_PROJECTILE);
    assert(starters[0].moves[3].cooldownTicks != starters[1].moves[3].cooldownTicks);
}
static void testLongRuns(void) {
    for (int mode = MODE_VS; mode <= MODE_DUO; ++mode)
        for (int pokemon = 0; pokemon < STARTER_COUNT; ++pokemon) {
            Game g = fight((GameMode)mode);
            g.selectedPokemon = pokemon;
            gameStart(&g);
            uint32_t seed = 12345u;
            for (int tick = 0; tick < 10000 && g.state == STATE_FIGHT; ++tick) {
                seed = seed * 1664525u + 1013904223u;
                uint32_t input = (seed >> 24u) & 63u;
                if (tick % 60 == 0) input |= IN_SELECT;
                gameUpdate(&g, input);
                assert(g.playerHp >= 0 && g.playerHp <= gameMaxHp(&g));
                assert(g.enemyHp >= 0 && g.enemyHp <= g.enemyMaxHp);
                assert(g.ultimateCharge >= 0 && g.ultimateCharge <= 100);
                assert(g.wave >= 1 && g.wave <= WAVE_COUNT);
                assert(g.fightTicks >= 0);
            }
            assert(g.state == STATE_RESULT);
        }
}
static void testOpponentAndReactiveDodge(void) {
    Game g; gameInit(&g); gameUpdate(&g,IN_A);
    assert(g.state==STATE_MODE);gameUpdate(&g,IN_A);
    assert(g.opponentPokemon == 1);
    gameUpdate(&g,IN_A);assert(g.state==STATE_OPPONENT);
    gameUpdate(&g,IN_R); assert(g.opponentPokemon == 2);
    gameUpdate(&g,IN_L); assert(g.opponentPokemon == 1);
    gameUpdate(&g,IN_A); assert(gameEnemySpecies(&g) == 1);
    gameStart(&g); assert(g.opponentPokemon == 1);
    g.enemyCooldown = 10000;
    g.projectiles[0] = (Projectile){g.enemyX,g.enemyY+24*POS_SCALE,0,-6*POS_SCALE,7,20,true,false,ELEMENT_FIRE};
    int x=g.enemyX;
    gameUpdate(&g,0);
    assert(g.enemyDodgeTimer > 0 && g.enemyDodgeCooldown == 90 && g.enemyX != x);
    gameUpdate(&g,IN_START); int timer=g.enemyDodgeTimer;
    gameUpdate(&g,0); assert(g.enemyDodgeTimer == timer);
    gameUpdate(&g,IN_START);
    for(int tick=0;tick<10;++tick) gameUpdate(&g,0);
    assert(g.enemyDodgeTimer == 0 && g.enemyDodgeCooldown > 0);
    g.projectiles[0] = (Projectile){g.enemyX,g.enemyY+24*POS_SCALE,0,6*POS_SCALE,7,20,true,false,ELEMENT_FIRE};
    g.enemyDodgeCooldown=0; g.frameCounter=30;
    gameUpdate(&g,0); assert(g.enemyDodgeTimer == 0);
}
static void testFullEvolutionAndCpuStyles(void) {
    for(int pokemon=0;pokemon<4;++pokemon) {
        Game g=fight(MODE_VS); g.selectedPokemon=pokemon; gameStart(&g);
        int base=gameMaxHp(&g);
        g.evolutionTicks=750; gameEvolve(&g); assert(gameStage(&g)==1);
        g.evolutionTicks=900; gameEvolve(&g);
        assert(gameStage(&g)==(pokemon==3?1:2));
        assert(gameMaxHp(&g)==base+35*gameFinalStage(&g));
        assert(gameEvolutionPercent(&g)==100);
        int hp=g.playerHp; gameEvolve(&g); assert(g.playerHp==hp);
        assert(gamePlayerName(&g)[0]);
        g.opponentPokemon=pokemon; gameStart(&g);
        assert(g.enemyMaxHp==cpuProfiles[pokemon].hp);
        g.enemyCooldown=0; g.frameCounter=37; g.enemyHp=300;
        for(int t=0;t<100 && g.cpuWindup==0;++t) gameUpdate(&g,0);
        assert(g.cpuWindup>0);
        gameUpdate(&g,IN_START); int windup=g.cpuWindup;
        gameUpdate(&g,0); assert(g.cpuWindup==windup);
    }
    assert(cpuProfiles[3].speed>cpuProfiles[1].speed);
    assert(cpuProfiles[1].distance>cpuProfiles[2].distance);
}
static void testVisualsAndMenuFlow(void) {
    assert(visualDirection(0,1)==0&&visualDirection(1,1)==1&&visualDirection(1,0)==2);
    assert(visualDirection(1,-1)==3&&visualDirection(0,-1)==4&&visualDirection(-1,-1)==5);
    assert(visualDirection(-1,0)==6&&visualDirection(-1,1)==7&&visualDirection(1,100)==0);
    assert(visualTargetFacing(2,-4,0)==2 && visualTargetFacing(2,-8,0)==6);
    assert(visualTargetFacing(2,100,51)==2 && visualTargetFacing(1,100,49)==1);
    assert(visualTargetFacing(2,100,75)==1);
    bool seen[FX_COUNT]={false};
    for(int species=0;species<4;++species)for(int slot=0;slot<5;++slot) {
        int style=visualStyle(species,slot);assert(style<FX_IMPACT&&!seen[style]);seen[style]=true;
        Game g=fight(MODE_VS);g.selectedPokemon=species;gameStart(&g);
        if(slot==4){g.ultimateCharge=100;gameUseUltimate(&g);}else gameAttack(&g,slot);
        assert(g.casts[0].serial==1&&g.casts[0].style==style&&g.casts[0].ticks>0);
        int ticks=g.casts[0].ticks;gameUpdate(&g,IN_START);gameUpdate(&g,0);
        assert(g.casts[0].ticks==ticks);
    }
    uint8_t durations[3]={2,4,2};
    assert(visualFrame(durations,3,0,false)==0&&visualFrame(durations,3,5,false)==1);
    assert(visualFrame(durations,3,8,false)==2&&visualFrame(durations,3,8,true)==0);
    VisualPose p={0};visualPoseStep(&p,0,0,2,1,1,8);assert(p.anim==2&&p.elapsed==0);
    visualPoseStep(&p,0,1,4,1,1,8);assert(p.anim==2&&p.facing==2&&p.elapsed==1);
    visualPoseStep(&p,0,1,4,1,0,8);assert(p.elapsed==1);
    visualPoseStep(&p,0,0,4,2,1,8);assert(p.elapsed==3&&p.facing==2);
    assert(p.serial==2); /* The follow-up is consumed without flickering back to frame zero. */
    Game g;gameInit(&g);
    assert(screensTouchInput(&g,45,55)==IN_A);gameUpdate(&g,IN_A);assert(g.state==STATE_MODE);
    gameUpdate(&g,screensTouchInput(&g,100,40));assert(g.state==STATE_SELECT);
    screensTouchInput(&g,190,89);assert(g.selectedPokemon==3);
    gameUpdate(&g,screensTouchInput(&g,190,177));assert(g.state==STATE_OPPONENT);
    assert(g.opponentPokemon!=g.selectedPokemon);
    gameUpdate(&g,screensTouchInput(&g,190,177));assert(g.state==STATE_FIGHT);
}
static void testSmoothDashAndSweptHits(void) {
    Game g=fight(MODE_VS);g.opponentPokemon=0;gameStart(&g);
    g.enemyX=g.playerX+60*POS_SCALE;g.enemyY=g.playerY;
    g.enemyCooldown=g.enemyDodgeCooldown=g.enemyGuard=10000;
    int origin=g.playerX,hp=g.enemyHp,damage=gameAttackDamage(&g,2)*2/3;
    gameAttack(&g,2);assert(g.playerX==origin&&g.dashes[0].ticks==DASH_TICKS);
    for(int tick=0;tick<DASH_TICKS;++tick) {
        int x=g.playerX;gameUpdate(&g,0);
        assert(g.playerX>x&&g.playerX-x<=8*POS_SCALE);
    }
    assert(g.playerX==origin+starters[0].moves[2].dashDistance*POS_SCALE);
    assert(g.enemyHp==hp-damage&&g.ultimateCharge==9);
    for(int tick=0;tick<5;++tick)gameUpdate(&g,0);
    assert(g.enemyHp==hp-damage);
    g=fight(MODE_VS);g.opponentPokemon=0;gameStart(&g);
    g.enemyX=g.playerX+34*POS_SCALE;g.enemyY=g.playerY;
    g.cpuWindup=1;g.cpuMove=2;g.cpuAimX=g.playerX;g.cpuAimY=g.playerY;
    origin=g.enemyX;hp=g.playerHp;gameUpdate(&g,0);
    assert(g.enemyX<origin&&g.enemyX>origin-8*POS_SCALE&&g.dashes[1].ticks==DASH_TICKS-1);
    for(int tick=1;tick<DASH_TICKS;++tick)gameUpdate(&g,0);
    assert(g.playerHp==hp-(starters[0].moves[2].damage/2+2));
    g=fight(MODE_VS);g.enemyX=176*POS_SCALE;g.enemyY=130*POS_SCALE;
    g.enemyCooldown=g.enemyGuard=g.enemyDodgeCooldown=10000;hp=g.enemyHp;
    g.projectiles[0]=(Projectile){100*POS_SCALE,130*POS_SCALE,100*POS_SCALE,0,15,4,true,false,ELEMENT_FIRE};
    gameUpdate(&g,0);assert(g.enemyHp==hp-10&&g.projectiles[0].life==0);
    g=fight(MODE_VS);g.enemyCooldown=g.enemyGuard=g.enemyDodgeCooldown=10000;
    g.projectiles[0]=(Projectile){100*POS_SCALE,100*POS_SCALE,60*POS_SCALE,0,7,4,true,false,ELEMENT_FIRE};
    g.projectiles[1]=(Projectile){140*POS_SCALE,100*POS_SCALE,-60*POS_SCALE,0,7,4,false,false,ELEMENT_WATER};
    gameUpdate(&g,0);assert(!g.projectiles[0].life&&!g.projectiles[1].life);
    g=fight(MODE_VS);g.enemyX=236*POS_SCALE;g.enemyY=130*POS_SCALE;
    g.enemyCooldown=g.enemyGuard=g.enemyDodgeCooldown=10000;hp=g.enemyHp;
    g.projectiles[0]=(Projectile){220*POS_SCALE,130*POS_SCALE,50*POS_SCALE,0,15,4,true,false,ELEMENT_FIRE};
    gameUpdate(&g,0);assert(g.enemyHp==hp-10);
}
static void testDodgeDirection(void) {
    Game g=fight(MODE_VS);
    g.enemyCooldown=10000;
    g.playerX=128*POS_SCALE;
    int x=g.playerX,y=g.playerY;
    gameUpdateInput(&g,IN_L,IN_DOWN);
    for(int i=1;i<DODGE_TICKS;++i) gameUpdateInput(&g,0,IN_RIGHT);
    assert(g.playerX==x && g.playerY==y+32*POS_SCALE);
    gameUpdateInput(&g,0,IN_RIGHT);
    assert(g.playerX>x && g.playerY==y+32*POS_SCALE);
    assert(gameDashProgress(2)<gameDashProgress(6)-gameDashProgress(4));
    assert(256-gameDashProgress(10)<gameDashProgress(2));
}
static void testEffectsBufferAndCamera(void) {
    Game g=fight(MODE_VS);gameAttack(&g,1);
    uint32_t first=g.castEvents[0].serial;
    for(int t=0;t<ACTION_LOCK_TICKS;++t)gameUpdateInput(&g,0,IN_A);
    int active=0;bool original=false;
    for(int i=0;i<CAST_CAP;++i)if(g.castEvents[i].ticks) {++active;if(g.castEvents[i].serial==first)original=true;}
    assert(active>=2&&original);
    g=fight(MODE_VS);g.moveCooldowns[1]=2;
    gameUpdate(&g,IN_B);assert(g.bufferedMove==1&&g.casts[0].serial==0);
    gameUpdate(&g,0);assert(g.casts[0].serial==1&&g.bufferedMove==-1);
    g=fight(MODE_VS);g.actionLock=2;
    gameUpdate(&g,IN_A);assert(g.bufferedMove==0&&g.moveCooldowns[0]==0);
    gameUpdate(&g,0);assert(g.moveCooldowns[0]>0&&g.bufferedMove==-1);
    g=fight(MODE_VS);g.enemyX=g.playerX;g.enemyY=g.playerY;g.enemyDodgeTimer=10;
    int hp=g.enemyHp;gameAttack(&g,1);assert(g.enemyHp==hp&&!g.lastHit&&!g.ultimateCharge);
    g=fight(MODE_VS);gameAttack(&g,2);gameUpdate(&g,0);gameUpdate(&g,IN_L);
    assert(!g.dashes[0].ticks&&g.dodgeTimer==DODGE_TICKS);
    bool trail=false;
    for(int i=0;i<CAST_CAP;++i)if(g.castEvents[i].ticks) {
        trail=true;
        assert(g.castEvents[i].motionTicks==1 && g.castEvents[i].age==2);
        assert(g.castEvents[i].ticks==4 && g.castEvents[i].x>g.playerX);
    }
    assert(trail);
    uint32_t randomState=123456u;unsigned periods=0;
    for(int i=0;i<128;++i){int t=visualRandomPeriod(&randomState);assert(t>=0&&t<4);periods|=1u<<t;}
    assert(periods==15);
    VisualCamera c={0};
    for(int i=0;i<120;++i)visualCameraStep(&c,80,130,176,130,true,false,0);
    assert(c.zoom>=305&&c.zoom<=307&&c.x<0);
    VisualCamera steady={0};
    for(int i=0;i<120;++i)visualCameraStep(&steady,128,130,140+i%20,130,true,false,0);
    assert(steady.x==0); /* Nearby CPU movement does not swing the camera. */
    VisualCamera wide={0};
    for(int i=0;i<120;++i)visualCameraStep(&wide,20,130,236,130,true,false,0);
    assert(wide.x==0&&wide.zoom==218);
    VisualCamera frozen=c;visualCameraStep(&c,20,58,236,176,true,true,12);
    assert(c.x==frozen.x&&c.y==frozen.y&&c.zoom==frozen.zoom);
    for(int i=0;i<18;++i)visualCameraStep(&c,80,130,176,130,true,false,12);
    assert(c.zoom>frozen.zoom && c.zoom<=331);
    for(int i=0;i<120;++i)visualCameraStep(&c,ARENA_LEFT,ARENA_TOP,ARENA_RIGHT,ARENA_BOTTOM,true,false,12);
    int top=96+((visualGroundY(ARENA_TOP)-57-96)*256-c.y)*c.zoom/65536;
    int bottom=96+((visualGroundY(ARENA_BOTTOM)+9-96)*256-c.y)*c.zoom/65536;
    assert(top>=9 && bottom<=183 && (ARENA_RIGHT-ARENA_LEFT)*c.zoom/256<=204);
    int edgeX=128*POS_SCALE,edgeY=130*POS_SCALE;
    arenaMove(&edgeX,&edgeY,200*POS_SCALE,0);
    assert(edgeX==ARENA_RIGHT*POS_SCALE);
    edgeX=128*POS_SCALE;
    arenaMove(&edgeX,&edgeY,0,100*POS_SCALE);
    assert(edgeY==ARENA_BOTTOM*POS_SCALE);
    assert(visualGroundY(137)-visualGroundY(117)==16);
    assert(visualLerp(100,104,0)==100&&visualLerp(100,104,1)==102&&visualLerp(100,104,2)==104);
}
static void testSolidCover(void) {
    const ArenaProp *rock=&arenaProps[0];
    assert(rock->kind==PROP_ROCK);
    const int south=(rock->y+ARENA_ACTOR_MARGIN)*POS_SCALE;
    const int sideY=(rock->y-rock->depth/2)*POS_SCALE;
    const int impactX=rock->x-rock->halfWidth-2;
    assert(arenaSweep(ARENA_LEFT*POS_SCALE,130*POS_SCALE,ARENA_RIGHT*POS_SCALE,130*POS_SCALE,
                      ARENA_ACTOR_MARGIN,NULL)==ARENA_PATH_FULL);

    Game g=fight(MODE_VS);
    g.enemyCooldown=10000;
    g.playerX=rock->x*POS_SCALE;
    for(int t=0;t<32;++t) gameUpdateInput(&g,0,IN_UP);
    assert(g.playerY==south && !g.moving);
    for(int t=0;t<14;++t) gameUpdateInput(&g,0,IN_UP|IN_RIGHT);
    assert(g.playerX>(rock->x+rock->halfWidth+ARENA_ACTOR_MARGIN)*POS_SCALE && g.playerY<south);

    g=fight(MODE_VS);
    g.enemyCooldown=g.enemyDodgeCooldown=10000;
    g.playerX=g.enemyX=rock->x*POS_SCALE;
    g.playerY=(rock->y+22)*POS_SCALE;
    g.enemyY=(rock->y-rock->depth-11)*POS_SCALE;
    g.faceX=0;
    g.faceY=-1;
    int hp=g.enemyHp;
    gameAttack(&g,2);
    for(int t=0;t<DASH_TICKS;++t) gameUpdate(&g,0);
    assert(g.playerY>=south && g.playerY<=south+1);
    assert(g.enemyHp==hp);

    g=fight(MODE_VS);
    g.enemyCooldown=g.enemyDodgeCooldown=g.enemyGuard=10000;
    g.enemyX=(rock->x+32)*POS_SCALE;
    g.enemyY=sideY;
    hp=g.enemyHp;
    g.projectiles[0]=(Projectile){ARENA_LEFT*POS_SCALE,sideY,90*POS_SCALE,0,30,4,true,false,ELEMENT_WATER};
    gameUpdate(&g,0);
    assert(!g.projectiles[0].life && g.enemyHp==hp);
    bool splash=false;
    for(int i=0;i<IMPACT_CAP;++i) {
        const VisualImpact *p=&g.impacts[i];
        if(p->ticks && p->element==ELEMENT_WATER && !p->hit &&
           p->x/POS_SCALE>=impactX-1 && p->x/POS_SCALE<=impactX) splash=true;
    }
    assert(splash);
    g.enemyX=(impactX-6)*POS_SCALE;
    g.projectiles[0]=(Projectile){ARENA_LEFT*POS_SCALE,sideY,90*POS_SCALE,0,30,4,true,false,ELEMENT_FIRE};
    gameUpdate(&g,0);
    assert(g.enemyHp==hp-20); /* Cover behind the target does not eat the hit. */

    g=fight(MODE_DUO);
    g.enemyCooldown=10000;
    g.playerX=(rock->x+67)*POS_SCALE;
    g.playerY=(rock->y-14)*POS_SCALE;
    g.partnerX=(rock->x-18)*POS_SCALE;
    g.partnerY=sideY;
    for(int t=0;t<90;++t) gameUpdate(&g,0);
    assert(g.partnerX>(rock->x+rock->halfWidth+ARENA_ACTOR_MARGIN)*POS_SCALE);
}

static void testPoseTransitions(void) {
    VisualPose p={.species=-1};
    for(int t=0;t<6;++t) visualPoseStep(&p,0,1,2,0,1,28);
    assert(p.anim==1 && p.elapsed==5);
    visualPoseStep(&p,0,0,2,0,1,28);
    assert(p.anim==1);
    visualPoseStep(&p,0,0,2,0,1,28);
    assert(p.anim==0);
    visualPoseStep(&p,0,1,2,0,1,28);
    assert(p.anim==1 && p.elapsed==7);
    visualPoseStep(&p,0,3,2,1,1,28);
    assert(p.anim==3);
    for(int t=0;t<9;++t) visualPoseStep(&p,0,3,2,1,1,3);
    assert(p.anim==0); /* A held damage flag cannot restart a finished reaction. */
}

static void testMatchOpening(void) {
    Game g;
    gameInit(&g);
    g.state=STATE_OPPONENT;
    gameUpdate(&g,IN_START);
    assert(g.state==STATE_FIGHT && gameIntroCount(&g)==3);
    int startX=g.playerX,clock=g.fightTicks;
    for(int t=0;t<GAME_HZ;++t) gameUpdateInput(&g,IN_A|IN_X|IN_L,IN_A|IN_RIGHT);
    assert(gameIntroCount(&g)==2 && g.playerX==startX && g.fightTicks==clock);
    assert(g.frameCounter==0 && !g.casts[0].serial && !g.dodgeTimer && !g.evolutionTicks);
    gameUpdate(&g,IN_START);
    for(int t=0;t<60;++t) gameUpdate(&g,0);
    assert(g.state==STATE_PAUSE && g.introTicks==2*GAME_HZ);
    gameUpdate(&g,IN_START);
    for(int t=0;t<2*GAME_HZ;++t) gameUpdateInput(&g,IN_A,IN_A|IN_RIGHT);
    assert(!g.introTicks && !g.casts[0].serial && g.fightTicks==clock);
    gameUpdateInput(&g,IN_A,IN_RIGHT);
    assert(g.playerX>startX && g.fightTicks==clock-1 && g.casts[0].serial==1);
}

static void testTeamDuel(void) {
    Game g;
    gameInit(&g);
    gameUpdate(&g,screensTouchInput(&g,65,48));
    assert(g.state==STATE_MODE && g.mode==MODE_VS);
    assert(!screensTouchInput(&g,120,129));
    gameUpdate(&g,screensTouchInput(&g,120,87));
    assert(g.state==STATE_SELECT && g.mode==MODE_TEAM);
    gameUpdate(&g,IN_A);
    assert(g.state==STATE_OPPONENT);
    gameUpdate(&g,IN_A);
    assert(g.introTicks==INTRO_TICKS && g.partnerHp==PARTNER_HP && g.rival.hp==TEAM_RIVAL_HP);

    g=fight(MODE_TEAM);
    g.playerX=100*POS_SCALE;g.playerY=100*POS_SCALE;
    g.enemyX=122*POS_SCALE;g.enemyY=g.playerY;
    g.rival.x=164*POS_SCALE;g.rival.y=g.playerY;
    g.enemyHp=1;
    gameAttack(&g,1);
    assert(g.state==STATE_FIGHT && g.enemyHp==0 && g.wave==1 && g.rival.hp==TEAM_RIVAL_HP);
    assert(gameAimsAtRival(&g));
    g.rival.hp=1;
    for(int i=0;i<24 && g.state==STATE_FIGHT;++i) gameUpdateInput(&g,0,IN_A);
    assert(g.state==STATE_RESULT && g.result==RESULT_WIN && g.rival.hp==0);

    g=fight(MODE_TEAM);
    g.playerX=100*POS_SCALE;g.playerY=100*POS_SCALE;
    g.enemyX=164*POS_SCALE;g.enemyY=g.playerY;
    g.rival.x=122*POS_SCALE;g.rival.y=g.playerY;
    g.rival.hp=1;
    gameAttack(&g,1);
    assert(g.state==STATE_FIGHT && g.rival.hp==0 && g.enemyHp==g.enemyMaxHp);
    assert(!gameAimsAtRival(&g));
}

int main(void) {
    testDodgeDirection();
    testMatchOpening();
    testTeamDuel();
    testDuelRounds();
    testWaveBreakAndTactics();
    testSolidCover();testPoseTransitions();
    testSmoothDashAndSweptHits();testEffectsBufferAndCamera();
    testVisualsAndMenuFlow();
    testWaves(); testEvolution(); testResultsAndClock(); testCombat(); testTouchAndScreens(); testLongRuns(); testMovementPauseProjectiles();
    testFullEvolutionAndCpuStyles(); testOpponentAndReactiveDodge(); testIndividualCooldowns(); testHeldBasicAndPriority(); testProjectilePoolAndGuard(); testMoveDataAndHud();
    puts("OK: solid cover, impact reactions, pose transitions, combat, evolution, pause, input buffering and camera");
    return 0;
}
