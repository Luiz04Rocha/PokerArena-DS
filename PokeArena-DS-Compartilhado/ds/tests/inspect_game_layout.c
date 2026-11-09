/* ARM-only metadata object used by check_emulator.py. Never linked into the ROM.
 * Compiler-produced offsets also handle the toolchain's short-enum ABI. */
#include "game.h"
#include "arena.h"
#include <stddef.h>
typedef struct { char name[24]; uint32_t offset, size; } LayoutRecord;
const LayoutRecord arenaLayout[] __attribute__((section(".arena_layout"),used)) = {
    {"$game",0,sizeof(Game)},
    {"$arenaMin",ARENA_LEFT,ARENA_TOP},
    {"$arenaMax",ARENA_RIGHT,ARENA_BOTTOM},
    {"$arenaProp",sizeof(ArenaProp),0},
#define FIELD(name) {#name,offsetof(Game,name),sizeof(((Game *)0)->name)}
    FIELD(evolutionStage), FIELD(cpuWindup), FIELD(soundSerial), FIELD(opponentPokemon), FIELD(enemyDodgeTimer), FIELD(state), FIELD(mode), FIELD(selectedPokemon), FIELD(playerHp), FIELD(enemyHp),
    FIELD(ultimateCharge), FIELD(moveCooldowns), FIELD(actionLock), FIELD(roundDelay),
    FIELD(enemyX), FIELD(enemyY), FIELD(enemyMaxHp), FIELD(wave),
    FIELD(bufferedMove), FIELD(bufferTicks), FIELD(fightTicks), FIELD(evolutionTicks), FIELD(introTicks), FIELD(result),
    FIELD(evolved), FIELD(playerX), FIELD(playerY), FIELD(lastAttack), FIELD(frameCounter),
    FIELD(dodgeTimer), FIELD(dodgeCooldown), FIELD(menuCursor), FIELD(muted)
    ,{"playerDashTicks",offsetof(Game,dashes)+offsetof(DashAction,ticks),sizeof(int)}
    ,{"playerCastSerial",offsetof(Game,casts)+offsetof(VisualCast,serial),sizeof(uint32_t)}
    ,{"playerCastAge",offsetof(Game,casts)+offsetof(VisualCast,age),sizeof(int)}
    ,{"rivalHp",offsetof(Game,rival)+offsetof(SupportRival,hp),sizeof(int)}
    ,{"rivalAttacks",offsetof(Game,rival)+offsetof(SupportRival,attackSerial),sizeof(uint32_t)}
    ,FIELD(partnerHp)
    ,FIELD(duelRound), FIELD(playerRounds), FIELD(rivalRounds)
#undef FIELD
};
