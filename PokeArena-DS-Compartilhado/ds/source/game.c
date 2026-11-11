#include "game.h"
#include "arena.h"
#include "visuals.h"
#include <string.h>
#include <stdlib.h>

static int clampInt(int n, int lo, int hi) { return n < lo ? lo : n > hi ? hi : n; }
static void decrement(int *n) { if (*n > 0) --*n; }
static void soundEvent(Game *g,int event) { g->soundEvent=event; ++g->soundSerial; }
static void styledImpact(Game *g,int x,int y,Element element,bool hit,uint8_t fireStyle) {
    int oldest=0;
    for(int i=1;i<IMPACT_CAP;++i) {
        if(g->impacts[i].ticks < g->impacts[oldest].ticks ||
           (g->impacts[i].ticks == g->impacts[oldest].ticks && !g->impacts[i].hit)) oldest=i;
    }
    if(!hit && g->impacts[oldest].hit && g->impacts[oldest].ticks==IMPACT_TICKS) return;
    g->impacts[oldest]=(VisualImpact){x,y,IMPACT_TICKS,element,hit,fireStyle};
}
static void impact(Game *g,int x,int y,Element element,bool hit) {
    styledImpact(g,x,y,element,hit,0);
}
static uint8_t fireImpactStyle(int species,int style,Element element) {
    if(element!=ELEMENT_FIRE || species%STARTER_COUNT || style>FX_FIRE_ULT) return 0;
    return (uint8_t)(1+style+5*(species/STARTER_COUNT));
}
static void cast(Game *g,int who,const MoveDef *m,int slot) {
    VisualCast *v=&g->casts[who];
    v->x=v->originX=who?g->enemyX:g->playerX;
    v->y=v->originY=who?g->enemyY:g->playerY;
    int dx=g->faceX,dy=g->faceY;
    if(who || m->kind!=MOVE_DASH) {
        bool rival=!who && gameAimsAtRival(g);
        dx=(who?g->playerX:rival?g->rival.x:g->enemyX)-v->x;
        dy=(who?g->playerY:rival?g->rival.y:g->enemyY)-v->y;
    }
    if(who && m->kind==MOVE_DASH) {dx=g->cpuAimX-v->x;dy=g->cpuAimY-v->y;}
    v->facing=visualDirection(dx,dy);
    v->style=visualStyle(who?gameEnemySpecies(g):g->selectedPokemon,slot);
    v->species=who?gameEnemySpecies(g):gamePlayerSpecies(g);
    v->kind=m->kind; v->range=m->range; v->source=who;
    v->ticks=v->totalTicks=m->kind==MOVE_PROJECTILE?4:m->kind==MOVE_AREA?8:m->kind==MOVE_DASH?10:6;
    if(slot==4)v->ticks=v->totalTicks=7;
    v->age=0;
    v->motionTicks=DASH_TICKS;
    if(m->kind==MOVE_DASH) {v->x=g->dashes[who].targetX;v->y=g->dashes[who].targetY;}
    ++v->serial;
    int oldest=0;
    for(int i=1;i<CAST_CAP;++i)if(g->castEvents[i].ticks<g->castEvents[oldest].ticks)oldest=i;
    g->castEvents[oldest]=*v;
    g->castEvents[oldest].serial=++g->visualSerial;
    if(m->kind==MOVE_DASH) impact(g,v->originX,v->originY,ELEMENT_NORMAL,false);
    if(m->kind==MOVE_AREA || m->kind==MOVE_MELEE) {
        static const int stepX[8]={0,181,256,181,0,-181,-256,-181};
        static const int stepY[8]={256,181,0,-181,-256,-181,0,181};
        int reach=m->range*POS_SCALE/2;
        int tx=v->x+stepX[v->facing]*reach/256;
        int ty=v->y+stepY[v->facing]*reach/256;
        int stop=arenaSweep(v->x,v->y,tx,ty,0,NULL);
        impact(g,arenaAt(v->x,tx,stop),arenaAt(v->y,ty,stop),m->element,false);
    }
}
int gameMaxHp(const Game *g) { return starters[g->selectedPokemon].hp + gameStage(g)*35; }
int gameEvolutionPercent(const Game *g) {
    return gameStage(g)==gameFinalStage(g) ? 100 : g->evolutionTicks * 100 / (gameStage(g) ? 900 : EVOLUTION_TICKS);
}
int gameSecondsLeft(const Game *g) { return (g->fightTicks + GAME_HZ - 1) / GAME_HZ; }
int gameIntroCount(const Game *g) { return (g->introTicks + GAME_HZ - 1) / GAME_HZ; }
const char *gameModeName(const Game *g) {
    static const char *const names[MODE_COUNT]={"VS 1x1","ONDAS SOLO","ONDAS DUPLA","VS 2x2"};
    return names[g->mode];
}
void gameInit(Game *g) {
    memset(g, 0, sizeof(*g));
    g->state = STATE_TITLE;
    g->opponentPokemon = 1;
    g->bufferedMove = -1;
}
static int enemyMaxHp(const Game *g) {
    static const int waveHp[WAVE_COUNT]={125,205,290};
    if(!gameIsHorde(g->mode)) return cpuProfiles[g->opponentPokemon].hp;
    return waveHp[g->wave-1]+(g->mode==MODE_DUO?20:0);
}
void gameStart(Game *g) {
    GameMode mode = g->mode;
    int selected = g->selectedPokemon;
    int opponent = g->opponentPokemon;
    bool muted=g->muted;
    uint32_t soundSerial=g->soundSerial;
    memset(g, 0, sizeof(*g));
    g->mode = mode;
    g->selectedPokemon = selected;
    g->opponentPokemon = opponent;
    g->muted=muted;
    g->soundSerial=soundSerial;
    g->state = STATE_FIGHT;
    g->bufferedMove = -1;
    g->playerHp = gameMaxHp(g);
    g->partnerHp = mode == MODE_DUO || mode == MODE_TEAM ? PARTNER_HP : 0;
    g->partnerCooldown = 45;
    g->wave = 1;
    if(!gameIsHorde(mode)) g->duelRound=1;
    g->enemyMaxHp = enemyMaxHp(g);
    g->enemyHp = g->enemyMaxHp;
    g->enemyCooldown = 30;
    g->fightTicks = 240 * GAME_HZ;
    g->playerX = 80 * POS_SCALE; g->playerY = 130 * POS_SCALE;
    g->enemyX = 176 * POS_SCALE; g->enemyY = 130 * POS_SCALE;
    g->partnerX = 92 * POS_SCALE; g->partnerY = 148 * POS_SCALE;
    if(mode==MODE_TEAM) g->rival=(SupportRival){.x=164*POS_SCALE,.y=151*POS_SCALE,.hp=TEAM_RIVAL_HP,.cooldown=36};
    g->faceX = 1;
}
static void startPresentedFight(Game *g) {
    gameStart(g);
    g->introTicks=INTRO_TICKS;
    soundEvent(g,8);
}
bool gameDuelContinues(const Game *g) {
    return g->state==STATE_RESULT && !gameIsHorde(g->mode) &&
           g->playerRounds<2 && g->rivalRounds<2 && (g->duelRound<3 || g->result==RESULT_TIME);
}
static void nextDuelRound(Game *g) {
    int round=g->duelRound+(g->result!=RESULT_TIME);
    int wins=g->playerRounds,losses=g->rivalRounds;
    int stage=gameStage(g),progress=g->evolutionTicks;
    startPresentedFight(g);
    g->duelRound=round;
    g->playerRounds=wins;
    g->rivalRounds=losses;
    g->duelContinuation=true;
    g->evolutionStage=stage;
    g->evolved=stage>0;
    g->evolutionTicks=progress;
    g->playerHp=gameMaxHp(g);
}
static bool inRange(int ax, int ay, int bx, int by, int radius) {
    int dx = (ax - bx) / POS_SCALE, dy = (ay - by) / POS_SCALE;
    return dx * dx + dy * dy <= radius * radius;
}
bool gameAimsAtRival(const Game *g) {
    if(!g->rival.hp) return false;
    if(!g->enemyHp) return true;
    int dx=g->rival.x-g->playerX,dy=g->rival.y-g->playerY;
    int ex=g->enemyX-g->playerX,ey=g->enemyY-g->playerY;
    bool rivalClear=arenaSweep(g->playerX,g->playerY,g->rival.x,g->rival.y,0,NULL)==ARENA_PATH_FULL;
    bool enemyClear=arenaSweep(g->playerX,g->playerY,g->enemyX,g->enemyY,0,NULL)==ARENA_PATH_FULL;
    return rivalClear==enemyClear ? dx*dx+dy*dy<ex*ex+ey*ey : rivalClear;
}
/* Swept collision in fixed-point coordinates; no floating point or tunnelling. */
static bool segmentHit(int ax,int ay,int bx,int by,int cx,int cy,int radius) {
    int64_t dx=bx-ax,dy=by-ay,px=cx-ax,py=cy-ay;
    int64_t length=dx*dx+dy*dy,dot=px*dx+py*dy,r=(int64_t)radius*POS_SCALE;
    if(!length||dot<=0)return px*px+py*py<=r*r;
    if(dot>=length) {px=cx-bx;py=cy-by;return px*px+py*py<=r*r;}
    int64_t cross=px*dy-py*dx;return cross*cross<=r*r*length;
}
static bool canAct(const Game *g) {
    return g->state == STATE_FIGHT && !g->introTicks && g->playerHp > 0 && (g->enemyHp > 0 || g->rival.hp > 0);
}
static void finish(Game *g, Result result) {
    if(!gameIsHorde(g->mode)) {
        if(result==RESULT_TIME) {
            int playerHp=g->playerHp,playerMax=gameMaxHp(g);
            int rivalHp=g->enemyHp,rivalMax=g->enemyMaxHp;
            if(g->mode==MODE_TEAM) {
                playerHp+=g->partnerHp;playerMax+=PARTNER_HP;
                rivalHp+=g->rival.hp;rivalMax+=TEAM_RIVAL_HP;
            }
            int lead=playerHp*rivalMax-rivalHp*playerMax;
            if(lead) result=lead>0?RESULT_WIN:RESULT_KO;
        }
        if(result==RESULT_WIN) ++g->playerRounds;
        if(result==RESULT_KO) ++g->rivalRounds;
    }
    g->result = result;
    g->state = STATE_RESULT;
    soundEvent(g,result==RESULT_WIN ? 6 : 7);
}
/* Every damage source goes through the same wave/result transition. */
static void damageEnemy(Game *g, int damage) {
    if(g->roundDelay>0||g->enemyDodgeTimer>0)return;
    g->enemyHp = clampInt(g->enemyHp - damage, 0, g->enemyMaxHp);
    if (g->enemyHp > 0) return;
    if (gameIsHorde(g->mode) && g->wave < WAVE_COUNT) {
        ++g->wave;
        g->enemyMaxHp = enemyMaxHp(g);
        g->enemyHp = g->enemyMaxHp;
        g->roundDelay = 45;
        g->enemyCooldown = 30;
        g->playerHp=clampInt(g->playerHp+20,0,gameMaxHp(g));
        if(g->partnerHp>0) g->partnerHp=clampInt(g->partnerHp+12,0,PARTNER_HP);
        g->bufferedMove=-1;
        g->bufferTicks=g->dodgeTimer=0;
        g->enemyGuard = 0;
        g->cpuWindup=g->cpuCycle=0;
        g->enemyHurtAnim=0;g->casts[1].ticks=0;g->lastHit=0;
        for(int i=0;i<2;++i) g->casts[i].age=30;
        g->enemyDodgeTimer = g->enemyDodgeCooldown = 0;
        g->partnerCooldown = 45;
        g->enemyX = (g->wave==2?46:176) * POS_SCALE;
        g->enemyY = (g->wave==2?68:117) * POS_SCALE;
        memset(g->projectiles,0,sizeof(g->projectiles));
        memset(g->castEvents,0,sizeof(g->castEvents));
        memset(g->dashes,0,sizeof(g->dashes));
    } else if(g->rival.hp>0) {
        g->cpuWindup=g->enemyMoving=g->dashes[1].ticks=0;
    } else finish(g, RESULT_WIN);
}
static void damageRival(Game *g,int damage,Element element,uint8_t fireStyle) {
    g->rival.hp=clampInt(g->rival.hp-damage,0,TEAM_RIVAL_HP);
    g->rival.hurt=5;
    styledImpact(g,g->rival.x,g->rival.y,element,true,fireStyle);
    if(!g->rival.hp && !g->enemyHp) finish(g,RESULT_WIN);
}
static int guardedDamage(const Game *g, int damage) {
    return g->enemyGuard > 0 ? damage * 2 / 3 : damage;
}
static bool fireMove(Game *g, bool mine, const MoveDef *move, int damage, bool ultimate);
static void beginDash(Game *g,int who,const MoveDef *m,int damage);
void gameAttack(Game *g, int slot) {
    const MoveDef *move = gameMoveDef(g, slot);
    if (!canAct(g) || !move || g->moveCooldowns[slot] > 0 || g->actionLock > 0 || g->roundDelay > 0 || g->dodgeTimer > 0 || g->dashes[0].ticks>0) return;
    int damage = gameAttackDamage(g, slot);
    /* A full projectile pool must not consume a move that was never fired. */
    if (move->kind == MOVE_PROJECTILE && !fireMove(g,true,move,damage,false)) return;
    g->moveCooldowns[slot] = move->cooldownTicks;
    g->actionLock = ACTION_LOCK_TICKS;
    g->attackAnim = 7;
    if(move->kind==MOVE_DASH)beginDash(g,0,move,damage);
    cast(g,0,move,slot);
    soundEvent(g,1);
    g->lastAttack = slot;
    if (move->kind == MOVE_PROJECTILE) {
        g->lastHit = 0;
        return;
    }
    if(move->kind==MOVE_DASH) {g->lastHit=0;return;}
    bool hitRival=g->rival.hp && inRange(g->playerX,g->playerY,g->rival.x,g->rival.y,move->range) &&
                  arenaSweep(g->playerX,g->playerY,g->rival.x,g->rival.y,0,NULL)==ARENA_PATH_FULL;
    bool hitEnemy = g->enemyHp && g->enemyDodgeTimer==0 &&
                 inRange(g->playerX,g->playerY,g->enemyX,g->enemyY,move->range) &&
                 arenaSweep(g->playerX,g->playerY,g->enemyX,g->enemyY,0,NULL)==ARENA_PATH_FULL;
    g->lastHit=hitEnemy || hitRival;
    if(!g->lastHit) return;
    uint8_t fireStyle=fireImpactStyle(g->casts[0].species,g->casts[0].style,move->element);
    g->ultimateCharge = clampInt(g->ultimateCharge + 9, 0, ULT_MAX);
    if(hitRival) damageRival(g,damage,move->element,fireStyle);
    if(hitEnemy) {
        g->enemyHurtAnim=5;
        styledImpact(g,g->enemyX,g->enemyY,move->element,true,fireStyle);
        damageEnemy(g,guardedDamage(g,damage));
    }
}
void gameUseUltimate(Game *g) {
    if (!canAct(g) || g->ultimateCharge < ULT_MAX || g->actionLock > 0 || g->roundDelay > 0 || g->dodgeTimer > 0 || g->dashes[0].ticks>0) return;
    const MoveDef *move = gameUltimateDef(g);
    if (!fireMove(g,true,move,move->damage + (gameStage(g)*8),true)) return;
    g->ultimateCharge = 0;
    g->actionLock = 10;
    g->attackAnim = 12;
    cast(g,0,move,4);
    soundEvent(g,4);
    g->lastAttack = 4;
    g->lastHit = 0;
}
void gameEvolve(Game *g) {
    if (!canAct(g) || gameStage(g)>=gameFinalStage(g) || g->evolutionTicks < (gameStage(g) ? 900 : EVOLUTION_TICKS)) return;
    g->evolutionStage=gameStage(g)+1;
    g->evolutionTicks=0;
    g->evolved = true;
    g->evolutionAnim=36;
    soundEvent(g,5);
    g->playerHp = clampInt(g->playerHp + 35, 0, gameMaxHp(g));
}
static void damagePlayer(Game *g, int damage,Element element,uint8_t fireStyle) {
    if(g->dodgeTimer>0||g->hurtAnim>0||g->roundDelay>0)return;
    g->playerHp = clampInt(g->playerHp - damage,0,gameMaxHp(g));
    g->ultimateCharge = clampInt(g->ultimateCharge + 3,0,ULT_MAX);
    g->regenDelay = 63;
    g->hurtAnim = 5;
    styledImpact(g,g->playerX,g->playerY,element,true,fireStyle);
    soundEvent(g,2);
    if (g->playerHp == 0) finish(g, RESULT_KO);
}
static uint32_t integerSqrt(uint32_t n) {
    uint32_t result = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= result + bit) { n -= result + bit; result = (result >> 1) + bit; }
        else result >>= 1;
        bit >>= 2;
    }
    return result;
}
static DashAction dashAction(const Game *g,int who,const MoveDef *m,int damage) {
    int x=who?g->enemyX:g->playerX,y=who?g->enemyY:g->playerY;
    int dx=who?g->cpuAimX-x:g->faceX*256,dy=who?g->cpuAimY-y:g->faceY*256;
    int length=(int)integerSqrt((uint32_t)(dx*dx+dy*dy));
    int distance=m->dashDistance*POS_SCALE;
    if(who&&distance>length)distance=length;
    int tx=x,ty=y;
    if(length) {tx+=dx*distance/length;ty+=dy*distance/length;}
    tx=clampInt(tx,ARENA_LEFT*POS_SCALE,ARENA_RIGHT*POS_SCALE);
    ty=clampInt(ty,ARENA_TOP*POS_SCALE,ARENA_BOTTOM*POS_SCALE);
    int stop=arenaSweep(x,y,tx,ty,ARENA_ACTOR_MARGIN,NULL);
    return (DashAction){DASH_TICKS,x,y,arenaAt(x,tx,stop),arenaAt(y,ty,stop),damage,m->range,false};
}
static void beginDash(Game *g,int who,const MoveDef *m,int damage) {
    g->dashes[who]=dashAction(g,who,m,damage);
}
void gameCpuDashPreview(const Game *g,const MoveDef *move,int *x,int *y) {
    DashAction dash=dashAction(g,1,move,0);
    *x=dash.targetX;
    *y=dash.targetY;
}
/* Progress in 256ths of the full path, shared by collision and the trail. */
int gameDashProgress(int halfTicks) {
    static const uint16_t progress[DASH_TICKS+1]={0,30,76,132,185,230,256};
    if(halfTicks<=0) return 0;
    if(halfTicks>=DASH_TICKS*2) return 256;
    int step=halfTicks/2;
    return (halfTicks&1)?(progress[step]+progress[step+1])/2:progress[step];
}
static void updateDashes(Game *g) {
    for(int who=0;who<2;++who) {
        DashAction *d=&g->dashes[who];if(!d->ticks||g->roundDelay)continue;
        int *x=who?&g->enemyX:&g->playerX,*y=who?&g->enemyY:&g->playerY;
        int oldX=*x,oldY=*y,elapsed=DASH_TICKS-d->ticks+1;
        int progress=gameDashProgress(elapsed*2);
        *x=d->startX+(d->targetX-d->startX)*progress/256;
        *y=d->startY+(d->targetY-d->startY)*progress/256;
        --d->ticks;
        int tx=who?g->playerX:g->enemyX,ty=who?g->playerY:g->enemyY;
        bool invulnerable=who?(g->dodgeTimer>0||g->hurtAnim>0):!g->enemyHp || g->enemyDodgeTimer>0;
        if(!d->ticks && (*x!=d->startX || *y!=d->startY)) impact(g,*x,*y,ELEMENT_NORMAL,false);
        if(!d->hit&&!invulnerable&&segmentHit(oldX,oldY,*x,*y,tx,ty,d->range) &&
           arenaSweep(*x,*y,tx,ty,0,NULL)==ARENA_PATH_FULL) {
            d->hit=true;
            Element element=who?starters[gameEnemySpecies(g)].moves[g->cpuMove].element:gameMoveDef(g,2)->element;
            uint8_t fireStyle=fireImpactStyle(g->casts[who].species,g->casts[who].style,element);
            if(who)damagePlayer(g,d->damage,element,fireStyle);
            else {
                g->lastHit=1;g->enemyHurtAnim=5;styledImpact(g,g->enemyX,g->enemyY,element,true,fireStyle);
                g->ultimateCharge=clampInt(g->ultimateCharge+9,0,ULT_MAX);soundEvent(g,2);
                damageEnemy(g,guardedDamage(g,d->damage));
            }
            if(g->state!=STATE_FIGHT||g->roundDelay) return;
        }
        if(!who && !d->hit && g->rival.hp && segmentHit(oldX,oldY,*x,*y,g->rival.x,g->rival.y,d->range) &&
           arenaSweep(*x,*y,g->rival.x,g->rival.y,0,NULL)==ARENA_PATH_FULL) {
            d->hit=true;
            g->lastHit=1;
            g->ultimateCharge=clampInt(g->ultimateCharge+9,0,ULT_MAX);
            soundEvent(g,2);
            Element element=gameMoveDef(g,2)->element;
            damageRival(g,d->damage,element,fireImpactStyle(g->casts[0].species,g->casts[0].style,element));
            if(g->state!=STATE_FIGHT) return;
        }
    }
}
static bool fireMove(Game *g, bool mine, const MoveDef *move, int damage, bool ultimate) {
    if (!canAct(g)) return false;
    for (int i = 0; i < PROJECTILE_CAP; ++i) {
        Projectile *p = &g->projectiles[i];
        if (p->life > 0) continue;
        int x = mine ? g->playerX : g->enemyX, y = mine ? g->playerY : g->enemyY;
        bool rival=mine && gameAimsAtRival(g);
        int dx = (mine ? rival?g->rival.x:g->enemyX : g->playerX) - x;
        int dy = (mine ? rival?g->rival.y:g->enemyY : g->playerY) - y;
        int scale = (int)integerSqrt((uint32_t)(dx*dx + dy*dy));
        if (scale == 0) { dy = mine ? -POS_SCALE : POS_SCALE; scale = POS_SCALE; }
        int speed = move->speed * POS_SCALE;
        int life = (move->range + move->speed - 1) / move->speed;
        int species=mine?g->selectedPokemon:gameEnemySpecies(g),slot=ultimate?4:0;
        if(!ultimate) for(int s=0;s<4;++s) if(move==(mine?gameMoveDef(g,s):&starters[species].moves[s])) slot=s;
        *p = (Projectile){x,y,dx*speed/scale,dy*speed/scale,damage,life,mine,ultimate,move->element,
                          visualStyle(species,slot),0,x,y,++g->projectileSerial,mine?gamePlayerSpecies(g):species};
        return true;
    }
    return false;
}
bool gameFire(Game *g, bool mine, int damage, bool ultimate) {
    static const MoveDef cpuShot = {"CPU",MOVE_PROJECTILE,0,0,224,4,0,ELEMENT_NORMAL};
    const MoveDef *move = mine ? (ultimate ? gameUltimateDef(g) : gameMoveDef(g,0)) : &cpuShot;
    /* This utility is for projectiles; a melee basic uses the generic shot profile. */
    if (move->kind != MOVE_PROJECTILE) move = &cpuShot;
    return fireMove(g,mine,move,damage,ultimate);
}
static void updateProjectiles(Game *g,int playerOldX,int playerOldY,int enemyOldX,int enemyOldY,int rivalOldX,int rivalOldY) {
    int stop[PROJECTILE_CAP];
    for (int i = 0; i < PROJECTILE_CAP; ++i) {
        Projectile *p = &g->projectiles[i];
        if (p->life <= 0) continue;
        p->prevX=p->x;p->prevY=p->y;p->x += p->vx; p->y += p->vy;
        stop[i]=arenaSweep(p->prevX,p->prevY,p->x,p->y,2,NULL);
        ++p->age;
    }
    /* Resolve opposing projectiles before fighter hits so crossing shots cancel. */
    for (int i = 0; i < PROJECTILE_CAP; ++i) {
        Projectile *p = &g->projectiles[i];
        if (p->life <= 0) continue;
        for (int j = i+1; j < PROJECTILE_CAP; ++j) {
            Projectile *q = &g->projectiles[j];
            if (q->life <= 0 || p->mine == q->mine) continue;
            int time=stop[i]<stop[j]?stop[i]:stop[j];
            int dx=p->prevX-q->prevX,dy=p->prevY-q->prevY;
            if (time && segmentHit(dx,dy,arenaAt(dx,p->x-q->x,time),arenaAt(dy,p->y-q->y,time),0,0,9)) {
                p->life = q->life = 0;
                impact(g,arenaAt((p->prevX+q->prevX)/2,(p->x+q->x)/2,time),
                         arenaAt((p->prevY+q->prevY)/2,(p->y+q->y)/2,time),p->element,true);
                break;
            }
        }
    }
    for (int i = 0; i < PROJECTILE_CAP; ++i) {
        Projectile *p = &g->projectiles[i];
        if (p->life <= 0) continue;
        int x=arenaAt(p->prevX,p->x,stop[i]),y=arenaAt(p->prevY,p->y,stop[i]);
        int oldX=p->mine?enemyOldX:playerOldX,oldY=p->mine?enemyOldY:playerOldY;
        int tx=arenaAt(oldX,p->mine?g->enemyX:g->playerX,stop[i]);
        int ty=arenaAt(oldY,p->mine?g->enemyY:g->playerY,stop[i]);
        bool targetHit=stop[i] && segmentHit(p->prevX-oldX,p->prevY-oldY,x-tx,y-ty,0,0,12) &&
                       arenaSweep(x,y,tx,ty,0,NULL)==ARENA_PATH_FULL;
        uint8_t fireStyle=fireImpactStyle(p->species,p->style,p->element);
        if (p->mine && g->enemyHp && g->enemyDodgeTimer == 0 && targetHit) {
            p->life = 0;
            g->lastHit = 1; g->enemyHurtAnim = 5;
            styledImpact(g,g->enemyX,g->enemyY,p->element,true,fireStyle);
            soundEvent(g,2);
            if (!p->ultimate) g->ultimateCharge = clampInt(g->ultimateCharge+9,0,ULT_MAX);
            damageEnemy(g,guardedDamage(g,p->damage));
            if (g->state != STATE_FIGHT) return;
            continue;
        }
        if(p->mine && g->rival.hp && stop[i]) {
            int rx=arenaAt(rivalOldX,g->rival.x,stop[i]),ry=arenaAt(rivalOldY,g->rival.y,stop[i]);
            if(segmentHit(p->prevX-rivalOldX,p->prevY-rivalOldY,x-rx,y-ry,0,0,12) &&
               arenaSweep(x,y,rx,ry,0,NULL)==ARENA_PATH_FULL) {
                p->life=0;
                g->lastHit=1;
                soundEvent(g,2);
                if(!p->ultimate) g->ultimateCharge=clampInt(g->ultimateCharge+9,0,ULT_MAX);
                damageRival(g,p->damage,p->element,fireStyle);
                if(g->state!=STATE_FIGHT) return;
                continue;
            }
        }
        if (!p->mine && g->dodgeTimer == 0 && g->hurtAnim == 0 && targetHit) {
            p->life = 0;
            damagePlayer(g,p->damage,p->element,fireStyle);
            if (g->state != STATE_FIGHT) return;
        } else if(stop[i]<ARENA_PATH_FULL) {
            p->life=0;
            styledImpact(g,x,y,p->element,false,fireStyle);
        } else if(p->life==1 && x>=ARENA_LEFT*POS_SCALE && x<=ARENA_RIGHT*POS_SCALE &&
                  y>=ARENA_TOP*POS_SCALE && y<=ARENA_BOTTOM*POS_SCALE) {
            styledImpact(g,x,y,p->element,false,fireStyle);
        }
    }
    /* The final movement tick can still hit; expire only after collision checks. */
    for (int i = 0; i < PROJECTILE_CAP; ++i) {
        Projectile *p=&g->projectiles[i];
        if(p->x<0||p->x>256*POS_SCALE||p->y<(ARENA_TOP-4)*POS_SCALE||p->y>(ARENA_BOTTOM+2)*POS_SCALE)p->life=0;
        else decrement(&p->life);
    }
}
static void cpu(Game *g) {
    if(!g->enemyHp || g->dashes[1].ticks>0)return;
    if (g->cpuWindup>0) {
        --g->cpuWindup;
        if (g->cpuWindup==0) {
            const MoveDef *m=&starters[gameEnemySpecies(g)].moves[g->cpuMove];
            int damage=m->damage/2+2;
            if(gameIsHorde(g->mode) && g->wave==1) damage=m->damage/2;
            if(gameIsHorde(g->mode) && g->wave==3) damage+=2;
            if(m->kind==MOVE_PROJECTILE&&!fireMove(g,false,m,damage,false)) {g->enemyCooldown=10;return;}
            if(m->kind==MOVE_DASH)beginDash(g,1,m,damage);
            cast(g,1,m,g->cpuMove);
            if(m->kind!=MOVE_PROJECTILE&&m->kind!=MOVE_DASH&&
               inRange(g->playerX,g->playerY,g->cpuAimX,g->cpuAimY,m->range) &&
               arenaSweep(g->enemyX,g->enemyY,g->playerX,g->playerY,0,NULL)==ARENA_PATH_FULL)
                damagePlayer(g,damage,m->element,fireImpactStyle(g->casts[1].species,g->casts[1].style,m->element));
            g->enemyCooldown=cpuProfiles[gameEnemySpecies(g)].cooldown;
            if(gameIsHorde(g->mode)) {
                if(g->wave==1) g->enemyCooldown+=14;
                else if(g->wave==2) g->enemyCooldown+=4;
                else g->enemyCooldown-=g->enemyHp<=g->enemyMaxHp/2?10:5;
            }
        }
        return;
    }
    if (!canAct(g) || g->roundDelay > 0 || g->enemyDodgeTimer > 0 || g->enemyCooldown > 0 || g->dodgeTimer > 0) return;
    int pattern = (int)((g->frameCounter / 37u + (uint32_t)g->enemyHp / 23u) % 8u);
    if (!gameIsHorde(g->mode) && (pattern == 0 || pattern == 4 || pattern == 7)) {
        g->enemyGuard = 24 + (int)(g->frameCounter % 18u);
        g->enemyCooldown = 32 + (int)(g->frameCounter % 20u);
        return;
    }
    int species=gameEnemySpecies(g);
    if(arenaSweep(g->enemyX,g->enemyY,g->playerX,g->playerY,0,NULL)<ARENA_PATH_FULL) return;
    static const uint8_t order[STARTER_COUNT][MOVE_COUNT]={{0,1,3,2},{0,3,1,2},{0,2,1,3},{2,0,3,1}};
    static const uint8_t finalPressure[MOVE_COUNT]={2,3,2,1};
    const uint8_t *moveOrder=gameIsHorde(g->mode) && g->wave==3 && g->enemyHp<=g->enemyMaxHp/2?
                             finalPressure:order[species];
    int choices=(gameIsHorde(g->mode) && g->wave==1)?1:MOVE_COUNT;
    bool found=false;
    for(int i=0;i<choices;++i) {
        int turn=g->cpuCycle+i;
        int slot=choices==1?0:moveOrder[turn%MOVE_COUNT];
        const MoveDef *move=&starters[species].moves[slot];
        if(!inRange(g->enemyX,g->enemyY,g->playerX,g->playerY,move->range+move->dashDistance)) continue;
        g->cpuMove=slot;
        g->cpuCycle=turn+1;
        found=true;
        break;
    }
    if(!found) return;
    const MoveDef *m=&starters[species].moves[g->cpuMove];
    g->cpuAimX=g->enemyX; g->cpuAimY=g->enemyY;
    if (m->kind==MOVE_DASH) { g->cpuAimX=g->playerX; g->cpuAimY=g->playerY; }
    g->cpuWindup=gameCpuWindupTicks(g);
    if (g->mode == MODE_DUO && g->partnerHp > 0 && pattern == 5)
        g->partnerHp = clampInt(g->partnerHp - 4, 0, PARTNER_HP);
    g->enemyCooldown = 42 + (int)((g->frameCounter / 11u) % 30u);
    if (g->playerHp == 0) finish(g, RESULT_KO);
}
static void supportCpu(Game *g) {
    SupportRival *r=&g->rival;
    if(!r->hp) return;
    decrement(&r->cooldown);
    decrement(&r->hurt);
    bool partner=r->windup?r->aimAtPartner:g->partnerHp>0;
    int x=partner?g->partnerX:g->playerX,y=partner?g->partnerY:g->playerY;
    bool reach=inRange(r->x,r->y,x,y,32) && arenaSweep(r->x,r->y,x,y,0,NULL)==ARENA_PATH_FULL;
    r->moving=false;
    if(r->windup) {
        if(--r->windup || !reach) return;
        Element element=starters[gameRivalSpecies(g)].moves[0].element;
        if(!partner) damagePlayer(g,8,element,0);
        else if(g->partnerHp) {
            g->partnerHp=clampInt(g->partnerHp-8,0,PARTNER_HP);
            impact(g,x,y,element,true);
            soundEvent(g,2);
        }
    } else if(!reach) {
        int oldX=r->x,oldY=r->y;
        arenaPursue(&r->x,&r->y,x,y,24);
        r->moving=r->x!=oldX || r->y!=oldY;
    } else if(!r->cooldown) {
        r->windup=12;
        r->cooldown=42;
        r->aimAtPartner=partner;
        ++r->attackSerial;
    }
}
static int direction(int n) { return n > 0 ? 1 : n < 0 ? -1 : 0; }
static void reactToProjectile(Game *g) {
    if(!g->enemyHp) return;
    if(gameIsHorde(g->mode) && g->wave==1) return;
    if (g->roundDelay > 0 || g->cpuWindup > 0 || g->enemyDodgeCooldown > 0 || g->enemyGuard > 0 || g->frameCounter%3u != 0) return;
    for (int i = 0; i < PROJECTILE_CAP; ++i) {
        const Projectile *p = &g->projectiles[i];
        if (p->life <= 0 || !p->mine || !inRange(p->x,p->y,g->enemyX,g->enemyY,32)) continue;
        int dx = g->enemyX-p->x, dy = g->enemyY-p->y;
        if (dx*p->vx+dy*p->vy <= 0) continue; /* Ignore shots moving away. */
        g->enemyDodgeX = -direction(p->vy);
        g->enemyDodgeY = direction(p->vx);
        if (g->enemyX < (ARENA_LEFT+20)*POS_SCALE && g->enemyDodgeX < 0) { g->enemyDodgeX *= -1; g->enemyDodgeY *= -1; }
        if (g->enemyX > (ARENA_RIGHT-20)*POS_SCALE && g->enemyDodgeX > 0) { g->enemyDodgeX *= -1; g->enemyDodgeY *= -1; }
        if (g->enemyY < (ARENA_TOP+20)*POS_SCALE && g->enemyDodgeY < 0) { g->enemyDodgeX *= -1; g->enemyDodgeY *= -1; }
        if (g->enemyY > (ARENA_BOTTOM-20)*POS_SCALE && g->enemyDodgeY > 0) { g->enemyDodgeX *= -1; g->enemyDodgeY *= -1; }
        g->enemyDodgeTimer = 8;
        g->enemyDodgeCooldown = 90;
        break;
    }
}
static bool seekCover(Game *g,int speed) {
    if(!gameIsHorde(g->mode) || g->wave!=2 || g->enemyCooldown<23) return false;
    int bestDistance=44*44,targetX=0,targetY=0;
    for(int i=0;i<arenaPropCount;++i) {
        const ArenaProp *prop=&arenaProps[i];
        if(prop->kind!=PROP_ROCK && prop->kind!=PROP_ROCK_MOSS) continue;
        int edge=ARENA_ACTOR_MARGIN+3;
        const int xs[]={prop->x-prop->halfWidth-edge,prop->x+prop->halfWidth+edge,prop->x,prop->x};
        const int ys[]={prop->y-prop->depth/2,prop->y-prop->depth/2,prop->y-prop->depth-edge,prop->y+edge};
        for(int j=0;j<4;++j) {
            int x=xs[j],y=ys[j];
            if(x<ARENA_LEFT || x>ARENA_RIGHT || y<ARENA_TOP || y>ARENA_BOTTOM) continue;
            int dx=x-g->enemyX/POS_SCALE,dy=y-g->enemyY/POS_SCALE;
            int distance=dx*dx+dy*dy;
            if(distance>=bestDistance) continue;
            if(arenaSweep(g->playerX,g->playerY,x*POS_SCALE,y*POS_SCALE,0,NULL)==ARENA_PATH_FULL) continue;
            if(arenaSweep(g->enemyX,g->enemyY,x*POS_SCALE,y*POS_SCALE,ARENA_ACTOR_MARGIN,NULL)<ARENA_PATH_FULL) continue;
            bestDistance=distance;
            targetX=x*POS_SCALE;
            targetY=y*POS_SCALE;
        }
    }
    if(!targetX) return false;
    arenaPursue(&g->enemyX,&g->enemyY,targetX,targetY,speed);
    return true;
}
static int inputX(uint32_t held) {return !!(held&IN_RIGHT)-!!(held&IN_LEFT);}
static int inputY(uint32_t held) {return !!(held&IN_DOWN)-!!(held&IN_UP);}
static int dodgeSpeed(int ticks,int dx,int dy) {
    static const uint8_t speed[DODGE_TICKS]={40,40,36,34,32,28,24,22};
    int step=speed[DODGE_TICKS-ticks];
    return dx&&dy?(step*181+128)/256:step;
}
static void moveActors(Game *g, uint32_t held) {
    int dx = inputX(held),dy = inputY(held);
    if (g->dashes[0].ticks) dx = dy = 0;
    if (!g->dodgeTimer && (dx || dy)) { g->faceX = dx; g->faceY = dy; }
    int speed = dx && dy ? 23 : 32;
    int oldX = g->playerX, oldY = g->playerY;
    int stepX = dx * speed, stepY = dy * speed;
    if (g->dodgeTimer > 0) {
        int step=dodgeSpeed(g->dodgeTimer,g->dodgeX,g->dodgeY)*g->dodgePower;
        stepX=g->dodgeX*step;
        stepY=g->dodgeY*step;
    }
    arenaMove(&g->playerX,&g->playerY,stepX,stepY);
    g->moving = g->playerX != oldX || g->playerY != oldY;
    dx = direction(g->playerX - g->enemyX);
    dy = direction(g->playerY - g->enemyY);
    const CpuProfile *profile=&cpuProfiles[gameEnemySpecies(g)];
    oldX = g->enemyX;
    oldY = g->enemyY;
    if (g->enemyHp && g->enemyDodgeTimer > 0) {
        int step = dodgeSpeed(g->enemyDodgeTimer,g->enemyDodgeX,g->enemyDodgeY);
        arenaMove(&g->enemyX,&g->enemyY,g->enemyDodgeX*step,g->enemyDodgeY*step);
    } else if (g->enemyHp && !g->roundDelay && !g->enemyGuard && !g->cpuWindup && !g->dashes[1].ticks) {
        speed = dx && dy ? profile->speed*181/256 : profile->speed;
        bool covered = arenaSweep(g->enemyX,g->enemyY,g->playerX,g->playerY,0,NULL)<ARENA_PATH_FULL;
        int species = gameEnemySpecies(g);
        int desired=species==3 && g->enemyCooldown<14?43:profile->distance;
        if(!seekCover(g,profile->speed)) {
            if(covered || !inRange(g->playerX,g->playerY,g->enemyX,g->enemyY,desired))
                arenaPursue(&g->enemyX,&g->enemyY,g->playerX,g->playerY,profile->speed);
            else if((species==1 || (species==3 && g->enemyCooldown>=14)) &&
                    inRange(g->playerX,g->playerY,g->enemyX,g->enemyY,profile->distance-16))
                arenaMove(&g->enemyX,&g->enemyY,-dx*speed,-dy*speed);
        }
    }
    g->enemyMoving = g->enemyX != oldX || g->enemyY != oldY;
    if (g->partnerHp > 0) {
        oldX = g->partnerX;
        oldY = g->partnerY;
        int targetX=clampInt(g->playerX-30*POS_SCALE,ARENA_LEFT*POS_SCALE,ARENA_RIGHT*POS_SCALE);
        int targetY=clampInt(g->playerY+10*POS_SCALE,ARENA_TOP*POS_SCALE,ARENA_BOTTOM*POS_SCALE);
        arenaPursue(&g->partnerX,&g->partnerY,targetX,targetY,20);
        g->partnerMoving = g->partnerX != oldX || g->partnerY != oldY;
    }
}
void gameUpdate(Game *g, uint32_t down) { gameUpdateInput(g, down, 0); }
void gameUpdateInput(Game *g, uint32_t down, uint32_t held) {
    if (g->state == STATE_TITLE) {
        if(down&(IN_UP|IN_DOWN)) g->menuCursor^=2;
        if(down&(IN_LEFT|IN_RIGHT)) g->menuCursor^=1;
        if(down&(IN_START|IN_A)) {
            switch((TitleItem)g->menuCursor) {
            case TITLE_MULTIPLAYER: break;
            case TITLE_HORDE: g->mode=MODE_SOLO;g->state=STATE_MODE;break;
            case TITLE_OPTIONS: g->menuCursor=OPTION_SOUND;g->state=STATE_OPTIONS;break;
            default: if(gameIsHorde(g->mode)) g->mode=MODE_VS;g->state=STATE_MODE;break;
            }
        }
        return;
    }
    if(g->state==STATE_OPTIONS) {
        if(down&IN_B) {g->menuCursor=TITLE_OPTIONS;g->state=STATE_TITLE;return;}
        if(down&IN_UP) g->menuCursor=(g->menuCursor+OPTION_COUNT-1)%OPTION_COUNT;
        if(down&IN_DOWN) g->menuCursor=(g->menuCursor+1)%OPTION_COUNT;
        if(down&(IN_A|IN_START)) {
            if(g->menuCursor==OPTION_SOUND) g->muted=!g->muted;
            else g->state=g->menuCursor==OPTION_HELP?STATE_HELP:STATE_CREDITS;
        }
        return;
    }
    if(g->state==STATE_HELP || g->state==STATE_CREDITS) {
        if(down&(IN_B|IN_A|IN_START)) g->state=STATE_OPTIONS;
        return;
    }
    if(g->state==STATE_MODE) {
        if(down&IN_B) {g->state=STATE_TITLE; return;}
        if(g->menuCursor==TITLE_HORDE) {
            if(down&(IN_UP|IN_DOWN)) g->mode=g->mode==MODE_SOLO?MODE_DUO:MODE_SOLO;
        } else if(down&(IN_UP|IN_DOWN)) g->mode=g->mode==MODE_VS?MODE_TEAM:MODE_VS;
        if(down&(IN_A|IN_START)) g->state=STATE_SELECT;
        return;
    }
    if(g->state==STATE_OPPONENT) {
        if(down&IN_B) {g->state=STATE_SELECT;return;}
        int step=(down&(IN_RIGHT|IN_DOWN|IN_R))?1:(down&(IN_LEFT|IN_UP|IN_L))?-1:0;
        if(step) do {g->opponentPokemon=(g->opponentPokemon+step+4)%4;} while(g->opponentPokemon==g->selectedPokemon);
        if(down&(IN_A|IN_START)) startPresentedFight(g);
        return;
    }
    if (g->state == STATE_SELECT) {
        if (down & IN_B) {g->menuCursor=gameIsHorde(g->mode)?TITLE_HORDE:TITLE_PLAY;g->state=STATE_MODE;return;}
        if (down & IN_LEFT) g->selectedPokemon = (g->selectedPokemon + STARTER_COUNT - 1) % STARTER_COUNT;
        if (down & IN_RIGHT) g->selectedPokemon = (g->selectedPokemon + 1) % STARTER_COUNT;
        if (!gameIsHorde(g->mode) && (down & IN_L)) g->opponentPokemon = (g->opponentPokemon+STARTER_COUNT-1)%STARTER_COUNT;
        if (!gameIsHorde(g->mode) && (down & IN_R)) g->opponentPokemon = (g->opponentPokemon+1)%STARTER_COUNT;
        if (down & (IN_UP|IN_DOWN)) g->selectedPokemon=(g->selectedPokemon+2)%4;
        if (down & (IN_A | IN_START)) {
            if(!gameIsHorde(g->mode)) {
                if(g->opponentPokemon==g->selectedPokemon) g->opponentPokemon=(g->selectedPokemon+1)%4;
                g->state=STATE_OPPONENT;
            } else startPresentedFight(g);
        }
        return;
    }
    if (g->state == STATE_RESULT) {
        if (down & IN_B) {g->menuCursor=TITLE_PLAY;g->state=STATE_TITLE;}
        else if (down & IN_START) {
            if(gameDuelContinues(g)) nextDuelRound(g);
            else startPresentedFight(g);
        }
        return;
    }
    if (g->state == STATE_PAUSE) {
        if (down & IN_B) g->state = STATE_SELECT;
        else if (down & IN_START) g->state = STATE_FIGHT;
        return;
    }
    if (down & IN_START) {
        g->state = STATE_PAUSE;
        g->bufferedMove = -1; g->bufferTicks = 0;
        return;
    }
    if (g->introTicks) {
        --g->introTicks;
        if (g->introTicks % GAME_HZ == 0) soundEvent(g,g->introTicks?8:9);
        return;
    }
    if (g->playerHp <= 0) { finish(g, RESULT_KO); return; }
    if (g->fightTicks <= 0) { finish(g, RESULT_TIME); return; }

    /* Timers advance once per 30 Hz update; inputs cannot refresh an active dodge. */
    for (int slot = 0; slot < MOVE_COUNT; ++slot) decrement(&g->moveCooldowns[slot]);
    decrement(&g->actionLock);
    decrement(&g->bufferTicks);
    if (g->bufferTicks == 0) g->bufferedMove = -1;
    decrement(&g->enemyCooldown);
    decrement(&g->enemyGuard);
    decrement(&g->enemyDodgeTimer);
    decrement(&g->enemyDodgeCooldown);
    decrement(&g->regenDelay);
    decrement(&g->dodgeTimer);
    decrement(&g->dodgeCooldown);
    decrement(&g->partnerCooldown);
    decrement(&g->attackAnim); decrement(&g->hurtAnim); decrement(&g->enemyHurtAnim);
    decrement(&g->casts[0].ticks); decrement(&g->casts[1].ticks); decrement(&g->evolutionAnim);
    for(int i=0;i<2;++i) if(g->casts[i].serial && g->casts[i].age<30) ++g->casts[i].age;
    for(int i=0;i<CAST_CAP;++i)if(g->castEvents[i].ticks>0) {
        --g->castEvents[i].ticks;
        ++g->castEvents[i].age;
    }
    for(int i=0;i<IMPACT_CAP;++i) decrement(&g->impacts[i].ticks);
    if (!g->roundDelay && gameStage(g)<gameFinalStage(g) && g->evolutionTicks < (gameStage(g) ? 900 : EVOLUTION_TICKS)) ++g->evolutionTicks;
    if (!g->roundDelay && (down & IN_L) && g->dodgeCooldown == 0) {
        g->dodgeTimer = DODGE_TICKS;
        int dx=inputX(held),dy=inputY(held);
        g->dodgePower=dx||dy?2:1;
        g->dodgeX=g->dodgePower==2?dx:g->faceX;
        g->dodgeY=g->dodgePower==2?dy:g->faceY;
        g->faceX=g->dodgeX;
        g->faceY=g->dodgeY;
        int dashTicks=g->dashes[0].ticks;
        g->dashes[0].ticks=0;
        g->casts[0].age=30;
        for(int i=0;dashTicks && i<CAST_CAP;++i)if(g->castEvents[i].source==0&&g->castEvents[i].kind==MOVE_DASH&&g->castEvents[i].ticks) {
            g->castEvents[i].motionTicks=DASH_TICKS-dashTicks;
            if(g->castEvents[i].ticks>4)g->castEvents[i].ticks=4;
        }
        g->dodgeCooldown = DODGE_COOLDOWN;
        soundEvent(g,3);
    }
    reactToProjectile(g);
    int playerOldX=g->playerX,playerOldY=g->playerY,enemyOldX=g->enemyX,enemyOldY=g->enemyY;
    int rivalOldX=g->rival.x,rivalOldY=g->rival.y;
    moveActors(g, held);
    if(g->roundDelay) {
        --g->roundDelay;
        ++g->frameCounter;
        return;
    }
    if (down & IN_SELECT) gameEvolve(g);
    /* Keep a press near the end of a dodge until the next legal attack. */
    if (g->dodgeTimer < ACTION_BUFFER_TICKS) {
        int requested = -1;
        if ((down & IN_R) && g->ultimateCharge >= ULT_MAX) requested = MOVE_COUNT;
        else for (int slot = 1; slot < MOVE_COUNT; ++slot) {
            if ((down & (IN_A << slot)) && g->moveCooldowns[slot] < ACTION_BUFFER_TICKS) { requested = slot; break; }
        }
        if(requested<0&&(down&IN_A)&&g->moveCooldowns[0]<ACTION_BUFFER_TICKS&&g->bufferedMove<1)requested=0;
        if (requested >= 0) { g->bufferedMove = requested; g->bufferTicks = ACTION_BUFFER_TICKS; }
    }
    if (g->bufferTicks > 0) {
        int slot = g->bufferedMove;
        uint32_t serial=g->casts[0].serial;
        /* Action success is an event, not a cooldown that may already be running. */
        if (slot == MOVE_COUNT) gameUseUltimate(g);
        else gameAttack(g,slot);
        bool used=g->casts[0].serial!=serial;
        if (used) { g->bufferedMove = -1; g->bufferTicks = 0; }
    }
    if ((down | held) & IN_A) gameAttack(g, 0);
    if (g->state != STATE_FIGHT) return;
    bool rivalTarget=!g->enemyHp || (g->rival.hp && g->rival.aimAtPartner);
    int targetX=rivalTarget?g->rival.x:g->enemyX,targetY=rivalTarget?g->rival.y:g->enemyY;
    if(g->partnerHp > 0 && g->partnerCooldown == 0 &&
        inRange(g->partnerX,g->partnerY,targetX,targetY,100) &&
        arenaSweep(g->partnerX,g->partnerY,targetX,targetY,0,NULL)==ARENA_PATH_FULL) {
        g->partnerCooldown = 45;
        ++g->partnerAttackSerial;
        Element element=starters[(g->selectedPokemon+1)%STARTER_COUNT].moves[0].element;
        if(rivalTarget) damageRival(g,6,element,0);
        else {
            impact(g,targetX,targetY,element,true);
            damageEnemy(g,g->enemyGuard>0?2:4);
        }
        if (g->state != STATE_FIGHT) return;
    }
    cpu(g);
    if (g->state != STATE_FIGHT) return;
    supportCpu(g);
    if (g->state != STATE_FIGHT) return;
    updateDashes(g);
    if (g->state != STATE_FIGHT) return;
    updateProjectiles(g,playerOldX,playerOldY,enemyOldX,enemyOldY,rivalOldX,rivalOldY);
    if (g->state != STATE_FIGHT) return;
    ++g->frameCounter;
    if (g->regenDelay == 0 && g->playerHp < gameMaxHp(g) && g->frameCounter % 12u == 0)
        ++g->playerHp;
    --g->fightTicks;
    if (g->fightTicks == 0) finish(g, RESULT_TIME);
}
