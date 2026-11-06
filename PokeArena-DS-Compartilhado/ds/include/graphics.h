#ifndef ARENA_GRAPHICS_H
#define ARENA_GRAPHICS_H
#include "game.h"
void graphicsInit(void);
void graphicsPresent(void);
void graphicsDraw(const Game *game,const Game *previous,int half,int displayTicks);
#endif
