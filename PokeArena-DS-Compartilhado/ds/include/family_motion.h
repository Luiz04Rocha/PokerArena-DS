#ifndef ARENA_FAMILY_MOTION_H
#define ARENA_FAMILY_MOTION_H

#include "game.h"

typedef struct {
    int squash;  /* Pixels removed from the sprite height, relative to the feet. */
    int width;   /* Pixels added around the sprite's center. */
    int advance; /* Signed pixels toward the target. */
} FamilyMotion;

static inline int familyMotionScale(int pixels,int weight) {
    return (pixels*weight+(pixels<0?-128:128))/256;
}

static inline FamilyMotion familyMotion(int species,int kind,int progress) {
    /* Species follow the native atlas order: starters, then their evolutions. */
    static const struct {
        FamilyMotion pose;
        int peak;
    } profiles[]={
        [0]={{ 2, 0,-2},146}, /* Charmander draws back the tail flame. */
        [1]={{ 5, 2,-2},176}, /* Squirtle tucks behind the shell. */
        [2]={{ 3, 2,-1},102}, /* Bulbasaur plants its front feet early. */
        [3]={{-2,-1, 1},184}, /* Pikachu straightens as the cheeks charge. */
        [4]={{ 1, 2,-3},165}, /* Charmeleon coils and releases a sharper jet. */
        [5]={{ 3, 3,-3},148}, /* Wartortle winds up farther back. */
        [6]={{ 2, 1, 2},196}, /* Ivysaur leans into the vine release. */
        [7]={{-3, 2,-2},136}, /* Raichu raises its shoulders into the discharge. */
        [8]={{-2, 4, 1},116}, /* Charizard opens its stance and lifts its wings. */
        [9]={{ 3, 4,-2},116}, /* Blastoise sets a wide cannon stance. */
        [10]={{4, 4,-1},96},  /* Venusaur settles low to carry the flower. */
    };
    if((unsigned)species>=sizeof(profiles)/sizeof(profiles[0]) || progress<=0 || progress>=256)
        return (FamilyMotion){0};
    int peak=profiles[species].peak;
    if(!peak) return (FamilyMotion){0};

    FamilyMotion pose=profiles[species].pose;
    if(kind==MOVE_DASH) --pose.advance;
    if(kind==MOVE_AREA) pose.advance/=2;
    int weight=progress<peak?progress*256/peak:(256-progress)*256/(256-peak);
    return (FamilyMotion){familyMotionScale(pose.squash,weight),
                          familyMotionScale(pose.width,weight),
                          familyMotionScale(pose.advance,weight)};
}

#endif
