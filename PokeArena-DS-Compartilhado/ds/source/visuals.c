#include "visuals.h"
#include "game.h"
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
static int min(int a,int b) {return a<b?a:b;}
static int max(int a,int b) {return a>b?a:b;}
static int fitZoom(int width,int height,int viewW,int viewH) {
    return min(viewW*256/width,viewH*256/height);
}
void visualCameraStep(VisualCamera *c,const VisualCameraPoint *fighters,int count,
                      int introTicks,bool frozen,int ultimateTicks) {
    if(frozen) return;
    if(!count) {
        c->x=approach(c->x,0,8);
        c->y=approach(c->y,0,8);
        c->zoom=c->zoom?approach(c->zoom,288,12):288;
        return;
    }

    int leadX=fighters[0].x,leadY=visualGroundY(fighters[0].y);
    int left=leadX,right=leadX,top=leadY,bottom=leadY;
    for(int i=1;i<count;++i) {
        int y=visualGroundY(fighters[i].y);
        left=min(left,fighters[i].x);
        right=max(right,fighters[i].x);
        top=min(top,y);
        bottom=max(bottom,y);
    }

    /* A compact fight follows the leader. Wider formations shift the focus
     * toward the whole group so the player can see a flanking rival. */
    int weightX=clamp((right-left-64)*4,0,256);
    int weightY=clamp((bottom-top-20)*5,0,256);
    int x=(leadX-128)*256+((left+right)/2-leadX)*weightX;
    int y=(leadY-119)*256+((top+bottom)/2-leadY)*weightY;
    /* Largest evolved poses extend beyond the compact starters. */
    left-=34;right+=34;top-=62;bottom+=13;
    int width=right-left,height=bottom-top;
    int targetZoom=304-introTicks*52/INTRO_TICKS+clamp(ultimateTicks,0,12)*2;
    targetZoom=min(targetZoom,fitZoom(width,height,242,178));

    /* Zoom out quickly enough to retain fast dashes, and ease in more slowly. */
    if(!c->zoom) c->zoom=targetZoom;
    c->zoom=approach(c->zoom,targetZoom,c->zoom>targetZoom?5:16);
    c->zoom=min(c->zoom,fitZoom(width,height,252,188));

    int halfX=126*256*256/c->zoom,halfY=94*256*256/c->zoom;
    int lowX=(right-128)*256-halfX,highX=(left-128)*256+halfX;
    int lowY=(bottom-96)*256-halfY,highY=(top-96)*256+halfY;
    /* Keep a little more scenery around the group during normal tracking. */
    int paddingX=min(3*256,(highX-lowX)/2),paddingY=min(3*256,(highY-lowY)/2);
    x=clamp(x,lowX+paddingX,highX-paddingX);
    y=clamp(y,lowY+paddingY,highY-paddingY);
    c->x=clamp(approach(c->x,x,8),lowX,highX);
    c->y=clamp(approach(c->y,y,8),lowY,highY);

    /* The meadow texture covers x=-64..320 and y=-60..270 in world space. */
    int panX=192*256-128*256*256/c->zoom;
    int panTop=(visualGroundY(-60)-96)*256+96*256*256/c->zoom;
    int panBottom=(visualGroundY(270)-96)*256-96*256*256/c->zoom;
    c->x=clamp(c->x,-panX,panX);
    c->y=clamp(c->y,panTop,panBottom);
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
        p->idleBlend = 0;
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
    p->idleBlend = clamp(p->idleBlend+(p->anim==0?delta:-2*delta),0,10);
    int *clock = p->anim == 1 ? &p->walkTime : &p->idleTime;
    p->elapsed = *clock;
    *clock += delta;
    p->facing = facing;
}
