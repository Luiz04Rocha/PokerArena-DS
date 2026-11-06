#ifndef ARENA_SCREENS_H
#define ARENA_SCREENS_H
#include "game.h"
enum { TEXT_ROWS = 24, TEXT_COLS = 31, HUD_BUTTONS = 8 };
typedef struct { char lines[TEXT_ROWS][TEXT_COLS + 1]; } TextScreen;
typedef struct { int x, y, w, h; uint32_t input; } HudButton;
extern const HudButton hudButtons[HUD_BUTTONS];
extern const HudButton titleTiles[TITLE_TILE_COUNT];
extern const HudButton optionTiles[OPTION_COUNT];
extern const HudButton modeTiles[MODE_COUNT];
extern const HudButton pickerTiles[STARTER_COUNT];
extern const HudButton menuBack;
extern const HudButton pauseTiles[4];
extern const HudButton resultTiles[2];
uint32_t hudHitTest(int x, int y);
uint32_t screensTouchInput(Game *g,int x,int y);
void screensBuild(const Game *g, TextScreen *top, TextScreen *bottom);
#endif
