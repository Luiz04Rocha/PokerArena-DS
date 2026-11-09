#include "visuals.h"
#include <stdlib.h>
int visualGroundY(int y) { return 116+(y-117)*210/256; }
int visualRandomPeriod(uint32_t *state) {
    uint32_t value=*state?*state:0x51a7e39du;
    value^=value<<13;value^=value>>17;value^=value<<5;
    *state=value;return (int)(value>>30);
}
int visualLerp(int oldValue,int newValue,int half) {return oldValue+(newValue-oldValue)*half/2;}
static int approach(int value,int target,int divisor) {
    int change=(target-value)/divisor;
    if(!change&&value!=target)change=target>value?1:-1;
    return value+change;
}
static int clamp(int value,int low,int high) {
    return value < low ? low : value > high ? high : value;
}
void visualCameraStep(VisualCamera *c,int px,int py,int ex,int ey,bool fight,bool frozen,int ultimateTicks) {
    if(frozen)return;
    int x=0,y=0,zoom=296;
    if(fight) {
        int separation=abs(px-ex);
        int weight=clamp((separation-80)*2,0,128);
        int left=px<ex?px:ex,right=px>ex?px:ex;
        int feetY=visualGroundY(py),enemyY=visualGroundY(ey);
        int top=(feetY<enemyY?feetY:enemyY)-57;
        int bottom=(feetY>enemyY?feetY:enemyY)+9;
        int fitX=204*256/(separation?separation:1);
        int fitY=172*256/(bottom-top);
        int fit=fitX<fitY?fitX:fitY;
        zoom=clamp(210*256/(separation+36),218,307);
        zoom=clamp(zoom+clamp(ultimateTicks,0,12)*2,fit<218?fit:218,fit);
        int marginX=102*256*256/zoom,marginY=86*256*256/zoom;
        x=clamp((px-128)*256+(ex-px)*weight,(right-128)*256-marginX,(left-128)*256+marginX);
        y=clamp((visualGroundY((py*3+ey)/4)-116)*256,(bottom-96)*256-marginY,(top-96)*256+marginY);
        x=clamp(x,-48*256,48*256);
        y=clamp(y,-44*256,44*256);
    }
    if(!c->zoom)c->zoom=zoom;
    c->x=approach(c->x,x,8);c->y=approach(c->y,y,8);c->zoom=approach(c->zoom,zoom,12);
}
int visualDirection(int x,int y) {
    if(abs(x)*2<abs(y)) return y<0?4:0;
    if(abs(y)*2<abs(x)) return x>0?2:6;
    if(x>0) return y<0?3:1;
    if(x<0) return y<0?5:7;
    return 0;
}
int visualTargetFacing(int current,int dx,int dy) {
    static const int8_t x[8]={0,1,1,1,0,-1,-1,-1};
    static const int8_t y[8]={1,1,0,-1,-1,-1,0,1};
    /* Retain the last pose while overlapping and near the eight-way boundaries. */
    if(abs(dx)<5 && abs(dy)<5) return current;
    int bias=(abs(dx)+abs(dy))/8;
    return visualDirection(dx+x[current]*bias,dy+y[current]*bias);
}
int visualStyle(int species,int slot) {
    static const int styles[4][5]={
        {FX_FIRE_BOLT,FX_FIRE_BURST,FX_FIRE_DASH,FX_FIRE_BEAM,FX_FIRE_ULT},
        {FX_WATER_BOLT,FX_WATER_BURST,FX_WATER_DASH,FX_WATER_BEAM,FX_WATER_ULT},
        {FX_VINE,FX_LEAF,FX_DUST,FX_PETALS,FX_SOLAR},
        {FX_SPARK,FX_DISCHARGE,FX_QUICK,FX_THUNDER,FX_STORM}};
    return species>=0&&species<4&&slot>=0&&slot<5?styles[species][slot]:FX_IMPACT;
}
int visualFrame(const uint8_t *durations,int count,int elapsed,bool loop) {
    int total=0;
    for(int i=0;i<count;++i) total+=durations[i];
    if(count<=0||total<=0) return 0;
    if(elapsed<0) elapsed=0;
    if(loop) elapsed%=total;
    for(int i=0;i<count;++i) { if(elapsed<durations[i]) return i; elapsed-=durations[i]; }
    return count-1;
}
void visualPoseStep(VisualPose *p,int species,int requested,int facing,uint32_t serial,int delta,int duration) {
    if(p->species!=species) {
        *p=(VisualPose){.species=species,.facing=facing,.serial=p->serial};
    }
    if(!delta) return;
    bool attack = serial != p->serial;
    bool newHurt = requested == 3 && !p->hurtRequested;
    p->serial = serial;
    p->hurtRequested = requested == 3;
    /* Keep the current authored strike intact. Its effects and hits are timed independently. */
    if(p->once && p->anim==2 && p->elapsed<duration) {
        attack=false;
        newHurt=false;
    }
    if(newHurt || attack) {
        p->anim = newHurt ? 3 : 2;
        p->elapsed = 0;
        p->once = true;
        p->facing = facing;
        p->rateRemainder = 0;
        p->walkGrace = 0;
        return;
    }
    if(p->once && p->elapsed<duration) {
        int advance=delta*(p->anim==2?3:2)+p->rateRemainder;
        p->elapsed+=advance/2;p->rateRemainder=advance%2;return;
    }
    p->once = false;
    if(requested == 1) p->walkGrace = 2;
    else if(p->walkGrace > 0) p->walkGrace -= delta;
    p->anim = requested == 1 || p->walkGrace > 0 ? 1 : 0;
    int *clock = p->anim == 1 ? &p->walkTime : &p->idleTime;
    p->elapsed = *clock;
    *clock += delta;
    p->facing = facing;
}
