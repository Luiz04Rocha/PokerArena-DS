#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "screens.h"
#include "graphics.h"
#include "audio.h"

void sceneCycleLighting(int direction);
static Game game;
static Game previousGame;
static volatile uint32_t displayClock;
static void displayTick(void) {++displayClock;}

static uint32_t mapKeys(uint32_t keys) {
    uint32_t input = 0;
    const uint32_t ds[] = {KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R, KEY_START, KEY_SELECT, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN};
    for (unsigned i = 0; i < sizeof(ds) / sizeof(ds[0]); ++i)
        if (keys & ds[i]) input |= 1u << i;
    return input;
}
int main(void) {
    powerOn(POWER_ALL);
    lcdMainOnTop();
    videoSetMode(MODE_5_3D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankB(VRAM_B_MAIN_BG_0x06000000);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    graphicsInit();
    audioInit();

    gameInit(&game);
    previousGame=game;
    irqSet(IRQ_VBLANK,displayTick);
    irqEnable(IRQ_VBLANK);
    uint32_t pending = 0;
    uint32_t touchButton = 0;
    uint32_t previousDisplay=displayClock;
    unsigned logicPhase=0;
    while (1) {
        swiWaitForVBlank();
        graphicsPresent();
        unsigned elapsed=displayClock-previousDisplay;previousDisplay=displayClock;
        if(elapsed>4)elapsed=4;
        if(!elapsed)elapsed=1;
        scanKeys();
        uint32_t down = keysDown();
        uint32_t dsHeld = keysHeld();
        uint32_t held = mapKeys(dsHeld);
        pending |= mapKeys(down);
        if (dsHeld & KEY_TOUCH) {
            touchPosition touch;
            touchRead(&touch);
            uint32_t hit = game.state==STATE_FIGHT?hudHitTest(touch.px,touch.py):0;
            if(down&KEY_TOUCH) hit=screensTouchInput(&game,touch.px,touch.py);
            if (down & KEY_TOUCH) { touchButton = hit; pending |= hit; }
            /* Basic repeats while held inside its own button; specials stay edge-triggered. */
            if (touchButton == IN_A && hit == IN_A) held |= IN_A;
        } else touchButton = 0;
        /* 30 Hz rules, 60 Hz drawing. Interpolation never changes the hit boxes. */
        logicPhase+=elapsed;
        bool update=logicPhase>=2;
        while(logicPhase>=2) {
            logicPhase-=2;
            previousGame=game;
            if(game.state==STATE_PAUSE && (pending&(IN_L|IN_R)))
                sceneCycleLighting((pending&IN_R)?1:-1);
            gameUpdateInput(&game,pending,held);
            if(previousGame.state!=game.state||previousGame.wave!=game.wave||game.frameCounter<previousGame.frameCounter)previousGame=game;
            pending=0;
        }
        if(update)audioUpdate(&game);
        graphicsDraw(&game,&previousGame,(int)logicPhase,(int)elapsed);
    }
}
