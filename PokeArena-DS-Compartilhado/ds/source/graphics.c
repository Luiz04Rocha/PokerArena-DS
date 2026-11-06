#include <nds.h>
#include <string.h>
#include "graphics.h"
void sceneInit(void);
void sceneDraw(const Game *g,const Game *previous,int half,int displayTicks);
void nativeHudDraw(const Game *g);
void nativeHudDrawOverlay(const Game *g,int displayTicks);
void nativeHudInit(void);
void nativeHudPresent(u16 *bottom,u16 *overlay);
void sceneCommitTextures(void);
static int hudBg,overlayBg;
int sceneLightingChoice(void);
static int previousHud[29];
static bool hudValid;
static volatile unsigned graphicsSceneUsec,graphicsHudUsec,graphicsMaxUsec;
static volatile unsigned graphicsEvenUsec,graphicsOddUsec,graphicsEvenHud;
void graphicsInit(void) {
    hudBg=bgInitSub(2,BgType_Bmp8,BgSize_B8_256x256,0,0);
    overlayBg=bgInit(2,BgType_Bmp8,BgSize_B8_256x256,0,0);
    sceneInit();
    nativeHudInit();
    bgSetPriority(hudBg,0); bgSetPriority(overlayBg,0);
    REG_BG0CNT=(REG_BG0CNT&~3)|1;
    bgShow(overlayBg);
}
void graphicsDraw(const Game *g,const Game *previous,int half,int displayTicks) {
    cpuStartTiming(0);
    sceneDraw(g,previous,half,displayTicks);
    graphicsSceneUsec=timerTicks2usec(cpuGetTiming());
    int stamp[29]={(int)g->state,(int)g->mode,g->selectedPokemon,g->opponentPokemon,g->menuCursor,g->muted,
        gameStage(g),g->playerHp,g->enemyHp,g->enemyMaxHp,g->ultimateCharge,gameEvolutionPercent(g),
        gameSecondsLeft(g),g->wave,g->partnerHp,g->dodgeTimer>0,g->dodgeCooldown>0,g->roundDelay>0,g->cpuWindup>0,(int)g->result};
    for(int i=0;i<4;++i)stamp[20+i]=(g->moveCooldowns[i]*10+GAME_HZ-1)/GAME_HZ;
    stamp[24]=sceneLightingChoice();
    stamp[25]=gameIntroCount(g);
    stamp[26]=g->state==STATE_FIGHT && g->frameCounter<15;
    stamp[27]=g->attackAnim?g->lastAttack:-1;
    stamp[28]=g->rival.hp;
    if(!hudValid||memcmp(previousHud,stamp,sizeof(stamp))) {
        nativeHudDraw(g);
        memcpy(previousHud,stamp,sizeof(stamp));hudValid=true;
    }
    nativeHudDrawOverlay(g,displayTicks);
    unsigned elapsed=timerTicks2usec(cpuEndTiming());graphicsHudUsec=elapsed-graphicsSceneUsec;
    if(half)graphicsOddUsec=elapsed;else {graphicsEvenUsec=elapsed;graphicsEvenHud=graphicsHudUsec;}
    if(elapsed>graphicsMaxUsec)graphicsMaxUsec=elapsed;
}
void graphicsPresent(void) {
    sceneCommitTextures();
    nativeHudPresent(bgGetGfxPtr(hudBg),bgGetGfxPtr(overlayBg));
}
