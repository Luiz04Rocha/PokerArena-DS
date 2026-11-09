#ifndef ARENA_GAME_H
#define ARENA_GAME_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_NAME "PokeArena DS"

enum { GAME_HZ = 30, ULT_MAX = 100, WAVE_COUNT = 3, STARTER_COUNT = 4,
       EVOLUTION_TICKS = 25 * GAME_HZ, DODGE_TICKS = 8, DODGE_COOLDOWN = 24,
       POS_SCALE = 16, PROJECTILE_CAP = 16, MOVE_COUNT = 4, ACTION_LOCK_TICKS = 3, ACTION_BUFFER_TICKS = 6,
       CAST_CAP = 8, DASH_TICKS = 6, IMPACT_CAP = 4, IMPACT_TICKS = 10,
       INTRO_TICKS = 3 * GAME_HZ };
typedef enum { STATE_TITLE, STATE_SELECT, STATE_FIGHT, STATE_RESULT, STATE_PAUSE,
               STATE_MODE, STATE_OPPONENT, STATE_HELP, STATE_CREDITS, STATE_OPTIONS } GameState;
typedef enum { MODE_VS, MODE_SOLO, MODE_DUO, MODE_TEAM, MODE_COUNT } GameMode;
static inline bool gameIsHorde(GameMode mode) { return mode==MODE_SOLO || mode==MODE_DUO; }
typedef enum { TITLE_PLAY, TITLE_MULTIPLAYER, TITLE_HORDE, TITLE_OPTIONS, TITLE_TILE_COUNT } TitleItem;
typedef enum { OPTION_SOUND, OPTION_HELP, OPTION_CREDITS, OPTION_COUNT } OptionItem;
typedef enum { RESULT_NONE, RESULT_WIN, RESULT_KO, RESULT_TIME } Result;
typedef enum {
    IN_A = 1u << 0, IN_B = 1u << 1, IN_X = 1u << 2, IN_Y = 1u << 3,
    IN_L = 1u << 4, IN_R = 1u << 5, IN_START = 1u << 6, IN_SELECT = 1u << 7,
    IN_LEFT = 1u << 8, IN_RIGHT = 1u << 9, IN_UP = 1u << 10, IN_DOWN = 1u << 11
} GameInput;
typedef enum { MOVE_PROJECTILE, MOVE_AREA, MOVE_DASH, MOVE_MELEE } MoveKind;
typedef enum { ELEMENT_FIRE, ELEMENT_WATER, ELEMENT_GRASS, ELEMENT_ELECTRIC, ELEMENT_NORMAL, ELEMENT_COUNT } Element;
typedef struct {
    const char *name;
    MoveKind kind;
    int damage, cooldownTicks, range, speed, dashDistance;
    Element element;
} MoveDef;
typedef struct {
    const char *name, *form;
    MoveDef moves[MOVE_COUNT];
    MoveDef ultimate;
    int hp, attackBonus;
} PokemonDef;
typedef struct { int x, y, vx, vy, damage, life; bool mine, ultimate; Element element; int style, age,prevX,prevY; uint32_t serial; int species; } Projectile;
typedef struct { int x,y,originX,originY,facing,style,kind,range,ticks,totalTicks,source; int age,motionTicks; uint32_t serial; int species; } VisualCast;
typedef struct { int x,y,ticks; Element element; bool hit; } VisualImpact;
typedef struct { int ticks,startX,startY,targetX,targetY,damage,range; bool hit; } DashAction;
typedef struct {
    int x,y,hp,cooldown,windup,hurt;
    bool moving,aimAtPartner;
    uint32_t attackSerial;
} SupportRival;
enum { TEAM_RIVAL_HP=90, PARTNER_HP=90 };
typedef struct {
    GameState state;
    GameMode mode;
    Result result;
    int selectedPokemon, opponentPokemon;
    int menuCursor;
    bool muted;
    int playerHp, partnerHp, enemyHp, enemyMaxHp, ultimateCharge;
    int wave, fightTicks, moveCooldowns[MOVE_COUNT], actionLock, roundDelay, enemyCooldown, regenDelay;
    int introTicks;
    int bufferedMove, bufferTicks;
    int dodgeTimer, dodgeCooldown, enemyGuard, partnerCooldown, evolutionTicks;
    int dodgeX, dodgeY, dodgePower;
    int enemyDodgeTimer, enemyDodgeCooldown, enemyDodgeX, enemyDodgeY;
    bool evolved;
    int evolutionStage, cpuWindup, cpuMove, cpuAimX, cpuAimY, cpuCycle;
    uint32_t soundSerial;
    int soundEvent;
    int playerX, playerY, enemyX, enemyY, partnerX, partnerY;
    SupportRival rival;
    int faceX, faceY, moving, enemyMoving, partnerMoving;
    int attackAnim, hurtAnim, enemyHurtAnim, lastAttack, lastHit;
    VisualCast casts[2];
    VisualCast castEvents[CAST_CAP];
    DashAction dashes[2];
    VisualImpact impacts[IMPACT_CAP];
    int evolutionAnim;
    uint32_t partnerAttackSerial;
    uint32_t projectileSerial,visualSerial;
    Projectile projectiles[PROJECTILE_CAP];
    uint32_t frameCounter;
} Game;

extern const PokemonDef starters[STARTER_COUNT];
extern const uint16_t elementColors[ELEMENT_COUNT];
const MoveDef *gameMoveDef(const Game *g, int slot);
const MoveDef *gameUltimateDef(const Game *g);
int gameEnemySpecies(const Game *g);
int gameRivalSpecies(const Game *g);
bool gameAimsAtRival(const Game *g);
int gamePlayerSpecies(const Game *g);
int gameCpuWindupTicks(const Game *g);
int gameDashProgress(int halfTicks);
const char *gameWaveName(const Game *g);
int gameAttackDamage(const Game *g, int slot);
int gameStage(const Game *g);
int gameFinalStage(const Game *g);
const char *gamePlayerName(const Game *g);
typedef struct { int hp, speed, distance, cooldown; } CpuProfile;
extern const CpuProfile cpuProfiles[STARTER_COUNT];
void gameInit(Game *g);
void gameStart(Game *g);
void gameUpdate(Game *g, uint32_t down);
void gameUpdateInput(Game *g, uint32_t down, uint32_t held);
void gameAttack(Game *g, int slot);
void gameUseUltimate(Game *g);
void gameEvolve(Game *g);
bool gameFire(Game *g, bool mine, int damage, bool ultimate);
int gameMaxHp(const Game *g);
int gameEvolutionPercent(const Game *g);
int gameSecondsLeft(const Game *g);
int gameIntroCount(const Game *g);
const char *gameModeName(const Game *g);

#endif
