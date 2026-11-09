/* Native geometry engine: textured ground, low-poly decor and billboards.
 * The orthographic oblique camera never changes the combat hit boxes. */
#include <nds.h>
#include <string.h>
#include <stdlib.h>
#include "game.h"
#include "arena.h"
#include "visuals.h"
#include "generated_sprites.h"
#include "native_sprite_decode.h"
#include "generated_visuals.h"
extern const uint16_t forest_tree[1024],forest_palette[16];
extern const uint16_t props_atlas[2048],props_palette[16];
extern const uint16_t native_meadow[8192],meadow_palette[16];
enum {
    ACTOR_COUNT=4, PROJECTILE_TEXTURE=ACTOR_COUNT, CAST_TEXTURE=PROJECTILE_TEXTURE+PROJECTILE_CAP,
    IMPACT_TEXTURE=CAST_TEXTURE+CAST_CAP, EVOLUTION_TEXTURE=IMPACT_TEXTURE+IMPACT_CAP,
    DYNAMIC_TEXTURES=EVOLUTION_TEXTURE+1,
    GROUND_TEXTURE=2*DYNAMIC_TEXTURES
};
static int textures[GROUND_TEXTURE+3],textureBank;
static const uint16_t *cachedPixels[GROUND_TEXTURE],*pendingPixels[GROUND_TEXTURE];
static uint16_t actorStage[2][ACTOR_COUNT][1024] __attribute__((aligned(4)));
static uint16_t *textureMemory[GROUND_TEXTURE];
enum { PALETTES_PER_PERIOD=11+FX_COUNT+3 };
static int paletteTextures[4*PALETTES_PER_PERIOD];
static const uint16_t emptyTexture[1024] __attribute__((aligned(4)));
static int boundTextureSize=32;
static VisualPose poses[ACTOR_COUNT];
static GameState previousState;
static int frameDelta,renderHalf;
static int waterFrame, resultFrames, resultClock, trailWave;
typedef struct {int x[4],y[4],count;} ActorTrail;
static ActorTrail actorTrails[2];
static VisualCamera camera;
static unsigned sceneVertices;
static unsigned scenePolygons,primitiveStart;
static int primitiveMode;
static volatile unsigned sceneMaxVertices,sceneMaxPolygons,sceneTextureFailures;
static volatile unsigned sceneRenderCount,sceneTextureUploads;
static volatile unsigned sceneUploadLate,sceneUploadEndLine,sceneTailLights;
static volatile unsigned sceneGroundReactions;
static volatile unsigned sceneResultReactions;
static volatile unsigned sceneCelebrations,sceneTears,sceneDodgeGhosts,sceneImpactRings;
static uint32_t decorList[4096] __attribute__((aligned(4)));
static bool recording;
static unsigned listWords,decorVertices,decorPolygons;
static int lightingMode=4,period=1;
static int battlePeriod=1;
static uint32_t lightingSeed=0x31c67a5du;
static volatile unsigned sceneRandomSelections;
static uint16_t groundPalettes[4][16],treePalettes[4][16],propPalettes[4][16];
static uint16_t actorPalettes[4][11][16],effectPalettes[4][FX_COUNT][16];
static const int lightTint[4][3]={{250,252,226},{256,256,256},{256,191,151},{116,144,209}};
int sceneLightingChoice(void) {return lightingMode;}
const char *sceneLightingLabel(void) {
    static const char *names[]={"MANHA","TARDE","ENTARDECER","NOITE"};
    static const char *randomNames[]={"SORTEIO: MANHA","SORTEIO: TARDE","SORTEIO: ENTARDECER","SORTEIO: NOITE"};
    return lightingMode==4?randomNames[battlePeriod]:names[lightingMode];
}
void sceneCycleLighting(int direction) {lightingMode=(lightingMode+direction+5)%5;}
static int tintColor(int value) {
    int r=(value&31)*lightTint[period][0]/256;
    int g=((value>>5)&31)*lightTint[period][1]/256;
    int b=((value>>10)&31)*lightTint[period][2]/256;
    if(period==3&&value){if(b<5)b=5;}
    return RGB15(r,g,b);
}
static int spriteTint(int value,int ownLight) {
    int tint=tintColor(value),rest=256-ownLight;
    return RGB15(((value&31)*ownLight+(tint&31)*rest)/256,(((value>>5)&31)*ownLight+((tint>>5)&31)*rest)/256,(((value>>10)&31)*ownLight+((tint>>10)&31)*rest)/256);
}
static void recordCommand(uint32_t command,uint32_t a,uint32_t b,int count) {
    if(listWords+(unsigned)count+1>=4096){++sceneTextureFailures;return;}
    decorList[++listWords]=command;
    if(count>0)decorList[++listWords]=a;
    if(count>1)decorList[++listWords]=b;
}
static void color(int value) {value=tintColor(value);if(recording)recordCommand(FIFO_COLOR,(uint32_t)value,0,1);else glColor(value);}
/* Count submitted geometry, including degenerate strip triangles, conservatively.
 * Some emulator cores return zero for the hardware polygon-usage register. */
static void beginPrimitive(int mode) {
    primitiveMode=mode;primitiveStart=sceneVertices;
    if(recording)recordCommand(FIFO_BEGIN,(uint32_t)mode,0,1);else glBegin(mode);
}
static void endPrimitive(void) {
    unsigned v=sceneVertices-primitiveStart;
    if(primitiveMode==GL_TRIANGLES)scenePolygons+=v/3;
    else if(primitiveMode==GL_QUADS)scenePolygons+=v/4;
    else if(primitiveMode==GL_TRIANGLE_STRIP)scenePolygons+=v>2?v-2:0;
    else scenePolygons+=v>2?v/2-1:0;
    if(recording)recordCommand(FIFO_END,0,0,0);else glEnd();
}
static int groundY(int y) {return visualGroundY(y);}
static int spriteDepth(int y) {return (y-117)*147+4*256;}
/* Coordinates /64 leave room for camera movement within DS v16 range. */
static void projectedVertex(int x,int y,int z) {
    ++sceneVertices;
    int vx=(x-128*256)/4,vy=(96*256-y)/4,vz=z/4;
    if(recording)recordCommand(FIFO_VERTEX16,(uint16_t)vx|((uint32_t)(uint16_t)vy<<16),(uint16_t)vz,2);
    else glVertex3v16(vx,vy,vz);
}
static void vertex(int x,int y,int z) {projectedVertex(x*256,y*256,z*256);}
/* Keep subpixel height/depth: rounding height 1 to zero made the grass and dirt
 * occupy the same depth, producing alternating wedges and flickering edges. */
static void worldVertex(int x,int y,int height) {projectedVertex(x*256,116*256+(y-117)*210-height*147,(y-117)*147+height*210);}
static void flatColor(int r,int g,int b) {color(RGB15(r,g,b));}
static void noTexture(void) {if(recording)recordCommand(FIFO_TEX_FORMAT,0,0,1);else glBindTexture(0,0);}
/* The scene only queues pixels. Remapping the texture bank while rendering can
 * hide the whole bank (including the ground) from the geometry engine. Keep
 * allocation and palette uploads in init, then copy pixels during VBlank. */
void sceneCommitTextures(void) {
    if(REG_VCOUNT<192||REG_VCOUNT>=207){++sceneUploadLate;return;}
    uint8_t saved=VRAM_A_CR;
    vramSetBankA(VRAM_A_LCD);
    /* Finish both banks' actor frames before spending the window on effects. */
    bool late=false;
    for(int pass=0;pass<2 && !late;++pass) for(int slot=0;slot<GROUND_TEXTURE;++slot) {
        const uint16_t *data=pendingPixels[slot];
        int local=slot%DYNAMIC_TEXTURES;
        bool actor=local<ACTOR_COUNT;
        if(!data || actor!=(pass==0))continue;
        if(REG_VCOUNT>=207){++sceneUploadLate;late=true;break;}
        const uint16_t *pixels=actor?actorStage[slot/DYNAMIC_TEXTURES][local]:data;
        if(textureMemory[slot])dmaCopy(pixels,textureMemory[slot],actor?2048:512);
        cachedPixels[slot]=data;pendingPixels[slot]=NULL;++sceneTextureUploads;
    }
    unsigned line=REG_VCOUNT;
    if(line>sceneUploadEndLine)sceneUploadEndLine=line;
    VRAM_A_CR=saved;
}
static void assignPalette(int index) {
    if(!glAssignColorTable(0,paletteTextures[period*PALETTES_PER_PERIOD+index]))++sceneTextureFailures;
}
static void bindSprite(int slot,const uint16_t *data,int palette) {
    boundTextureSize=slot<ACTOR_COUNT?64:32;
    slot+=textureBank*DYNAMIC_TEXTURES;
    pendingPixels[slot]=cachedPixels[slot]==data?NULL:data;
    glBindTexture(0,textures[slot]);assignPalette(palette);
    glColor(RGB15(31,31,31));
}
static void bindActor(int id,const NativeAnim *anim,int index,int palette) {
    int slot=textureBank*DYNAMIC_TEXTURES+id;
    const uint16_t *begin=anim->pixels+anim->frameOffsets[index];
    const uint16_t *end=anim->pixels+anim->frameOffsets[index+1];
    if(cachedPixels[slot]!=begin) {
        uint16_t *stage=actorStage[textureBank][id];
        if(!nativeDecodeSprite(begin,end,stage)) ++sceneTextureFailures;
        /* DMA reads RAM directly on the ARM9, outside its data cache. */
        DC_FlushRange(stage,sizeof(actorStage[textureBank][id]));
    }
    bindSprite(id,begin,palette);
}
static void billboard(int x,int y,int width,int height,int z) {
    int left=x-width/2,top=y-height;
    beginPrimitive(GL_QUADS);
    glTexCoord2i(0,0);projectedVertex(left*256,top*256,z);
    glTexCoord2i(boundTextureSize,0);projectedVertex((left+width)*256,top*256,z);
    glTexCoord2i(boundTextureSize,boundTextureSize);projectedVertex((left+width)*256,y*256,z);
    glTexCoord2i(0,boundTextureSize);projectedVertex(left*256,y*256,z);
    endPrimitive();
}
static void prop(int x,int y,int w,int h,int u,int v,int tw,int th) {
    int left=x-w/2,bottom=groundY(y),top=bottom-h,z=spriteDepth(y);
    beginPrimitive(GL_QUADS);
    glTexCoord2i(u,v);projectedVertex(left*256,top*256,z);
    glTexCoord2i(u+tw,v);projectedVertex((left+w)*256,top*256,z);
    glTexCoord2i(u+tw,v+th);projectedVertex((left+w)*256,bottom*256,z);
    glTexCoord2i(u,v+th);projectedVertex(left*256,bottom*256,z);endPrimitive();
}
static const int lanternX[]={31,225};
enum { LANTERN_Y=43 };
static void sceneryProps(void) {
    glBindTexture(0,textures[GROUND_TEXTURE+2]);glColor(RGB15(31,31,31));
    const int fenceX[6]={22,52,82,174,204,234};
    for(int i=0;i<6;++i)prop(fenceX[i],42,32,32,64,0,32,32);
    prop(102,42,12,32,64,0,12,32);prop(254,42,12,32,64,0,12,32);
    for(int i=0;i<arenaPropCount;++i) {
        const ArenaProp *p=&arenaProps[i];
        if(p->kind==PROP_LOG) prop(p->x,p->y,p->width,p->height,0,32,64,32);
        else if(p->kind!=PROP_TREE)
            prop(p->x,p->y,p->width,p->height,p->kind==PROP_ROCK_MOSS?32:0,0,32,32);
    }
    for(int i=0;i<2;++i) prop(lanternX[i],LANTERN_Y,32,32,96,0,32,32);
}
static void ellipse(int x,int y,int rx,int ry,int height,int color) {
    noTexture();color=tintColor(color);if(recording)recordCommand(FIFO_COLOR,(uint32_t)color,0,1);else glColor(color);
    beginPrimitive(GL_TRIANGLE_STRIP);
    for(int i=0;i<=12;++i) {
        int a=i*32768/12;
        worldVertex(x,y,height);
        worldVertex(x+cosLerp(a)*rx/4096,y+sinLerp(a)*ry/4096,height);
    }
    endPrimitive();
}
static void teamMarker(int x,int y,bool allied) {
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(0));
    noTexture();
    flatColor(allied?8:31,allied?24:10,allied?31:7);
    beginPrimitive(GL_QUADS);
    worldVertex(x-6,y+9,2);worldVertex(x,y+6,2);
    worldVertex(x+6,y+9,2);worldVertex(x,y+12,2);
    endPrimitive();
}
static void ground(void) {
    glBindTexture(0,textures[GROUND_TEXTURE]);glColor(RGB15(31,31,31));
    beginPrimitive(GL_QUADS);
    glTexCoord2i(0,0);worldVertex(-64,-60,0);
    glTexCoord2i(256,0);worldVertex(320,-60,0);
    glTexCoord2i(256,128);worldVertex(320,270,0);
    glTexCoord2i(0,128);worldVertex(-64,270,0);
    endPrimitive();

}
typedef struct {int x,y,rx,ry;} Pond;
static const Pond ponds[]={{19,212,20,14},{271,171,17,13}};
static void pondBanks(void) {
    for(unsigned i=0;i<sizeof(ponds)/sizeof(ponds[0]);++i) {
        const Pond *p=&ponds[i];
        ellipse(p->x,p->y,p->rx+3,p->ry+2,1,RGB15(11,17,10));
        ellipse(p->x,p->y,p->rx,p->ry,2,RGB15(13,23,22));
        ellipse(p->x+1,p->y-1,p->rx-2,p->ry-2,3,RGB15(6,17,22));
        ellipse(p->x-3,p->y-1,p->rx-8,p->ry-4,4,RGB15(8,20,25));
    }
}
static void waterRipples(void) {
    noTexture();
    glPolyFmt(POLY_ALPHA(19)|POLY_CULL_NONE|POLY_ID(60));
    beginPrimitive(GL_QUADS);
    for(unsigned i=0;i<sizeof(ponds)/sizeof(ponds[0]);++i) {
        const Pond *p=&ponds[i];
        for(int line=0;line<5;++line) {
            int phase=(waterFrame+line*15+(int)i*29)%64;
            int x=p->x-7+(line*9)%13+phase/12;
            int y=p->y-7+line*3;
            int length=2+(phase<32?phase:64-phase)/10;
            color(line%2?RGB15(15,26,27):RGB15(20,29,29));
            worldVertex(x,y,5);worldVertex(x+length,y,5);
            worldVertex(x+length+1,y+1,5);worldVertex(x,y+1,5);
        }
    }
    endPrimitive();
}
static void decor(void) {
    pondBanks();
    for(int i=0;i<arenaPropCount;++i) {
        const ArenaProp *prop=&arenaProps[i];
        if(prop->kind==PROP_TREE) continue;
        ellipse(prop->x+2,prop->y-2,prop->halfWidth+3,prop->depth/2+1,1,RGB15(11,17,10));
    }
    noTexture();beginPrimitive(GL_TRIANGLES);
    for(int i=0;i<54;++i) {
        int x=(i*73+19)%242+7,y=(i*41+13)%166+31;
        if(abs(x-128)<80&&abs(y-117)<48) continue;
        for(int blade=0;blade<3;++blade) {
            flatColor(8+blade*2,15+blade*2,5+blade);
            worldVertex(x-3+blade*2,y,2);worldVertex(x-1+blade*2,y,2);worldVertex(x-3+blade*3,y,7+i%4);
        }
        if(i%3==0) {
            static const int petals[3][3]={{31,23,8},{28,15,25},{30,29,24}};
            const int *shade=petals[(i/3)%3];
            flatColor(shade[0],shade[1],shade[2]);
            worldVertex(x-3,y,6);worldVertex(x+2,y,6);worldVertex(x-1,y,10);
            worldVertex(x+1,y+2,5);worldVertex(x+5,y+2,5);worldVertex(x+3,y+2,8);
            flatColor(31,27,11);
            worldVertex(x-1,y,7);worldVertex(x+1,y,7);worldVertex(x,y,9);
        }
    }
    endPrimitive();
}
static int duration(int species,int anim) {
    const NativeAnim *a=&nativeSpecies[species].anim[anim];int total=0;
    for(int i=0;i<a->frames;++i) total+=a->durations[i];
    return total;
}
static int strikeTime(const VisualCast *cast,int species,int half) {
    if(!cast->serial || cast->species!=species || cast->age>=30) return -1;
    const NativeAnim *a=&nativeSpecies[species].anim[2];
    int elapsed=cast->age*2+half;
    int time=a->hitTime+elapsed*3/2;
    if(cast->kind==MOVE_DASH && a->returnTime>a->rushTime) {
        int travel=DASH_TICKS*2;
        time=elapsed<travel?a->rushTime+(a->returnTime-a->rushTime)*elapsed/travel:
                            a->returnTime+(elapsed-travel)*3/2;
    }
    return time<duration(species,2)?time:-1;
}
static void attackOrigin(int species,int facing,int *x,int *height,int weight) {
    const NativeAnim *a=&nativeSpecies[species].anim[2];
    if(!a->anchors) return;
    int frame=visualFrame(a->durations,a->frames,a->hitTime,false);
    const int8_t *anchor=a->anchors+4*(facing*a->frames+frame);
    *x+=anchor[2]*48*weight/(40*8);
    *height+=(anchor[3]*48/40-*height)*weight/8;
    if(*height<3) *height=3;
}
static void tailLight(int id,int x,int y,int z,int worldY) {
    ++sceneTailLights;noTexture();
    for(int ring=0;ring<3;++ring) {
        int radius=8-ring*2;
        glPolyFmt(POLY_ALPHA(3+ring*3)|POLY_CULL_NONE|POLY_ID(20+ring));
        glColor(RGB15(31,19+ring*3,4+ring*2));beginPrimitive(GL_TRIANGLE_STRIP);
        for(int i=0;i<=12;++i) {
            int a=i*32768/12;vertex(x,y,z-1);
            vertex(x+cosLerp(a)*radius/4096,y+sinLerp(a)*radius/4096,z-1);
        }
        endPrimitive();
    }
    glPolyFmt(POLY_ALPHA(5)|POLY_CULL_NONE|POLY_ID(23+id));
    glColor(RGB15(31,21,6));beginPrimitive(GL_TRIANGLE_STRIP);
    for(int i=0;i<=12;++i) {
        int a=i*32768/12;worldVertex(x,worldY,1);
        worldVertex(x+cosLerp(a)*12/4096,worldY+sinLerp(a)*8/4096,1);
    }
    endPrimitive();
}
static void resultParticles(int x,int y,int z,bool winner,int t) {
    if(winner && t>=58) return;
    noTexture();
    int alpha=winner?25-(t>30?(t-30)/3:0):28;
    glPolyFmt(POLY_ALPHA(alpha)|POLY_CULL_NONE|POLY_ID(winner?61:62));
    if(winner) ++sceneCelebrations;
    else ++sceneTears;
    beginPrimitive(GL_QUADS);
    for(int i=0;i<(winner?6:3);++i) {
        int phase=(t+i*7)%24;
        if(!winner && phase>=14) continue;
        int px=x+(winner?(i-3)*(3+phase/4):(i==1?-1:1)*(4+phase/5+i));
        int py=y+(winner?-4-phase/2:-3+phase/2);
        int radius=winner?2+(i%2):2;
        glColor(winner?(i%2?RGB15(31,23,5):RGB15(31,31,18)):RGB15(19,30,31));
        vertex(px-radius,py,z);vertex(px,py-radius-1,z);
        vertex(px+radius,py,z);
        if(!winner) glColor(RGB15(5,20,31));
        vertex(px,py+radius+1,z);
    }
    endPrimitive();
}
static void actor(int id,int species,int requested,int facing,uint32_t serial,int x,int y,int dodge,int previewSize,int reaction,int strike) {
    VisualPose *p=&poses[id];
    if(dodge && frameDelta) {
        requested=NATIVE_WALK;
        strike=-1;
        p->serial=serial;
        p->once=false;
    }
    if(strike>=0) {
        if(p->species!=species) *p=(VisualPose){.species=species};
        p->anim=2;
        p->facing=facing;
        p->elapsed=strike;
        p->serial=serial;
        p->once=false;
        p->walkGrace=0;
    } else visualPoseStep(p,species,requested,facing,serial,frameDelta,duration(species,p->anim));
    int anim=p->anim,elapsed=p->elapsed;
    bool loop=!p->once;
    facing=p->facing;
    int lift=0;
    if(reaction) {
        ++sceneResultReactions;
        int t=resultFrames-id*3;
        if(t<0)t=0;
        anim=reaction>0?NATIVE_CELEBRATE:NATIVE_DEFEATED;
        elapsed=t;
        loop=reaction>0;
        if(reaction>0) {
            facing=0;
            if(t>=duration(species,NATIVE_CELEBRATE)*2) {
                anim=NATIVE_IDLE;
                elapsed=resultClock;
            }
        }
    }
    const NativeAnim *a=&nativeSpecies[species].anim[anim];
    int frame=visualFrame(a->durations,a->frames,elapsed,loop);
    int index=facing*a->frames+frame;
    const int8_t *anchor=a->anchors?a->anchors+4*index:NULL;
    if(anim==NATIVE_CELEBRATE && anchor) {
        const int8_t *idle=nativeSpecies[species].anim[NATIVE_IDLE].anchors;
        if(idle && anchor[3]>idle[3]) lift=anchor[3]-idle[3];
        if(lift>18) lift=18;
    }
    int shadowX=x+(period<=1?1:-1);
    glPolyFmt(POLY_ALPHA(12-lift/3)|POLY_CULL_NONE|POLY_ID(51));
    ellipse(shadowX,y,11+lift/4,6,1,RGB15(6,10,13));
    glPolyFmt(POLY_ALPHA(16-lift/2)|POLY_CULL_NONE|POLY_ID(59));
    ellipse(x,y,7,4,2,RGB15(6,9,12));
    int base=previewSize?previewSize:48;
    int size=base*a->sourceSize/40;
    int width=size,height=size,footX=x;
    int sy=groundY(y)+(anchor?anchor[1]:a->foot[facing])*height/a->sourceSize,z=spriteDepth(y);
    if(anchor) x-=anchor[0]*width/a->sourceSize;
    if(period==3&&a->flame) {
        const uint8_t *point=a->flame+2*index;
        if(point[1]!=255)tailLight(id,x-width/2+point[0]*width/256,sy-height+point[1]*height/256,z/256,y);
    }
    bindActor(id,a,index,species);
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(id==3?27:id+1));
    if(dodge && id<2) {
        const ActorTrail *trail=&actorTrails[id];
        for(int i=3;i>=1;i-=2) {
            if(trail->count<=i || (trail->x[i]==footX && trail->y[i]==y)) continue;
            int alpha=(i==1?14:8)*(dodge<4?dodge:4)/4;
            glPolyFmt(POLY_ALPHA(alpha)|POLY_CULL_NONE|POLY_ID(i==1?52+id:61+id));
            billboard(x+trail->x[i]-footX,sy+groundY(trail->y[i])-groundY(y),
                      width,height,spriteDepth(trail->y[i])-256);
            ++sceneDodgeGhosts;
        }
        glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(id+1));
    }
    billboard(x,sy,width,height,z);
    if(id<2 && frameDelta) {
        ActorTrail *trail=&actorTrails[id];
        for(int i=3;i>0;--i) {trail->x[i]=trail->x[i-1];trail->y[i]=trail->y[i-1];}
        trail->x[0]=footX;trail->y[0]=y;
        if(trail->count<4) ++trail->count;
    }
    if(reaction>0 || (reaction<0 && resultFrames>=duration(species,NATIVE_DEFEATED))) {
        int hx=footX+(anchor?anchor[2]*base/40:0);
        int hy=groundY(y)-(anchor?anchor[3]*base/40:12);
        resultParticles(hx,hy,z/256+5,reaction>0,reaction>0?resultFrames:resultClock);
    }
}
static void fxBind(int slot,int style,int facing,int frame) {
    if(style<0||style>=FX_COUNT) style=FX_IMPACT;
    const NativeEffect *fx=&nativeEffects[style];
    bindSprite(slot,fx->frames[facing%8][frame%6],11+style);
}
static void effectAt(int x,int y,int size,int height) {billboard(x,groundY(y)-height+size/2,size,size,spriteDepth(y)+4*256);}
static void groundReaction(const VisualImpact *impact,int slot,int age) {
    static const uint16_t colors[ELEMENT_COUNT][2]={
        {RGB15(15,13,16),RGB15(31,19,5)},
        {RGB15(10,23,29),RGB15(20,31,31)},
        {RGB15(10,22,7),RGB15(20,29,10)},
        {RGB15(31,25,5),RGB15(31,31,22)},
        {RGB15(18,15,10),RGB15(23,20,14)}
    };
    int x=impact->x/POS_SCALE,y=impact->y/POS_SCALE;
    Element element=impact->element;
    ++sceneGroundReactions;
    noTexture();
    glPolyFmt(POLY_ALPHA(23-age)|POLY_CULL_NONE|POLY_ID(47+slot));
    beginPrimitive(GL_QUADS);
    for(int i=0;i<6;++i) {
        int angle=i*32768/6+(x+y)*97;
        int spread=2+age/2+i%3;
        int px=x+cosLerp(angle)*spread/4096;
        int py=y+sinLerp(angle)*spread/4096;
        int lift=2+(i%3)+age*(20-age)/28;
        int width=2,height=2;
        if(element==ELEMENT_FIRE) {
            lift+=age/2;
            width=i<4?2+age/8:1;
            height=width;
        } else if(element==ELEMENT_WATER) {
            width=1;
            height=2+i%2;
        } else if(element==ELEMENT_ELECTRIC) {
            height=3;
            lift=2+(i+age)%4;
        } else if(element==ELEMENT_NORMAL) {
            lift=2+age/5;
            width=2+age/8;
        }
        int sy=groundY(py)-lift,z=(py-117)*147/256+lift/2+1;
        glColor(colors[element][i>=(element==ELEMENT_FIRE?4:3)]);
        vertex(px-width,sy+1,z);
        vertex(px,sy-height,z);
        vertex(px+width,sy-1,z);
        vertex(px,sy+height,z);
    }
    endPrimitive();
}
static void impactRing(const VisualImpact *impact,int slot,int age) {
    if(age>=12) return;
    ++sceneImpactRings;
    noTexture();
    glPolyFmt(POLY_ALPHA(19-age)|POLY_CULL_NONE|POLY_ID(35+slot));
    glColor(elementColors[impact->element]);
    int x=impact->x/POS_SCALE,y=impact->y/POS_SCALE;
    int radius=5+age;
    beginPrimitive(GL_QUAD_STRIP);
    for(int i=0;i<=8;++i) {
        int angle=i*4096,c=cosLerp(angle),s=sinLerp(angle);
        worldVertex(x+c*radius/4096,y+s*radius/4096,5);
        worldVertex(x+c*(radius-2)/4096,y+s*(radius-2)/4096,5);
    }
    endPrimitive();
}
static void drawCast(const VisualCast *v,int slot) {
    if(!v->ticks) return;
    static const int dx[8]={0,181,256,181,0,-181,-256,-181};
    static const int dy[8]={256,181,0,-181,-256,-181,0,181};
    int age=v->age*2+renderHalf,total=v->totalTicks*2;
    int frame=age*6/total;if(frame>5)frame=5;
    int alpha=6+v->ticks*16/v->totalTicks;
    glPolyFmt(POLY_ALPHA(alpha)|POLY_CULL_NONE|POLY_ID(8+slot));
    fxBind(slot,v->style,v->facing,frame);
    int x=v->originX/POS_SCALE,y=v->originY/POS_SCALE;
    if(v->kind==MOVE_DASH) {
        int head=age<v->motionTicks*2?age:v->motionTicks*2;
        for(int i=0;i<4;++i) {
            int step=head-i*2;
            if(step<0)continue;
            int progress=gameDashProgress(step);
            int px=(v->originX+(v->x-v->originX)*progress/256)/POS_SCALE;
            int py=(v->originY+(v->y-v->originY)*progress/256)/POS_SCALE;
            effectAt(px,py,18-i*2,10);
        }
    } else if(v->kind==MOVE_AREA) {
        /* Damage is immediate, so the first ring already shows the actual range. */
        int radius=v->range+age*5/total;
        noTexture();glColor(elementColors[v->style/5]);beginPrimitive(GL_QUAD_STRIP);
        for(int i=0;i<=24;++i) {
            int a=i*32768/24,c=cosLerp(a),s=sinLerp(a);
            worldVertex(x+c*radius/4096,y+s*radius/4096,3);
            worldVertex(x+c*(radius-2)/4096,y+s*(radius-2)/4096,3);
        }
        endPrimitive();fxBind(slot,v->style,v->facing,frame);
        for(int i=0;i<6;++i) {int a=i*32768/6;effectAt(x+cosLerp(a)*radius/4096,y+sinLerp(a)*radius/4096,16,8);}
    } else {
        if(v->kind==MOVE_MELEE) {
            for(int i=1;i<=2;++i)effectAt(x+dx[v->facing]*v->range*i/(256*3),y+dy[v->facing]*v->range*i/(256*3),26,11);
        } else {
            int height=14;
            attackOrigin(v->species,v->facing,&x,&height,8);
            effectAt(x,y,16,height);
        }
    }
}
void sceneInit(void) {
    for(period=0;period<4;++period) {
        for(int c=0;c<16;++c) {
            groundPalettes[period][c]=tintColor(meadow_palette[c]);treePalettes[period][c]=tintColor(forest_palette[c]);
            propPalettes[period][c]=c>=14?props_palette[c]:tintColor(props_palette[c]);
            for(int i=0;i<11;++i)actorPalettes[period][i][c]=spriteTint(nativeSpecies[i].palette[c],128);
            for(int i=0;i<FX_COUNT;++i)effectPalettes[period][i][c]=spriteTint(nativeEffects[i].palette[c],192);
        }
    }
    period=battlePeriod=1;
    glInit();glEnable(GL_TEXTURE_2D|GL_ANTIALIAS|GL_BLEND);
    glClearColor(13,18,10,31);glClearPolyID(63);glClearDepth(0x7fff);
    glViewport(0,0,255,191);glGenTextures(GROUND_TEXTURE+3,textures);
    glGenTextures(4*PALETTES_PER_PERIOD,paletteTextures);
    for(int p=0;p<4;++p)for(int i=0;i<PALETTES_PER_PERIOD;++i) {
        const uint16_t *pal=i<11?actorPalettes[p][i]:i<11+FX_COUNT?effectPalettes[p][i-11]:
            i==11+FX_COUNT?groundPalettes[p]:i==12+FX_COUNT?treePalettes[p]:propPalettes[p];
        glBindTexture(0,paletteTextures[p*PALETTES_PER_PERIOD+i]);
        if(!glColorTableNtr(16,pal))++sceneTextureFailures;
    }
    for(int i=0;i<GROUND_TEXTURE;++i) {
        bool actorTexture=i%DYNAMIC_TEXTURES<ACTOR_COUNT;
        int size=actorTexture?64:32;
        glBindTexture(0,textures[i]);
        if(!glTexImageNtr2D(GL_RGB16,size,size,GL_TEXTURE_COLOR0_TRANSPARENT,emptyTexture,NULL))++sceneTextureFailures;
        textureMemory[i]=glGetTexturePointer(textures[i]);
        uintptr_t addr=(uintptr_t)textureMemory[i];
        if(addr<0x06800000u||addr+(size*size/2)>0x06820000u) {textureMemory[i]=NULL;++sceneTextureFailures;}
        assignPalette(actorTexture?0:11);
    }
    for(int i=GROUND_TEXTURE;i<GROUND_TEXTURE+3;++i) {
        glBindTexture(0,textures[i]);
        int ok;
        if(i==GROUND_TEXTURE)ok=glTexImageNtr2D(GL_RGB16,256,128,0,native_meadow,NULL);
        else if(i==GROUND_TEXTURE+1)ok=glTexImageNtr2D(GL_RGB16,64,64,GL_TEXTURE_COLOR0_TRANSPARENT,forest_tree,NULL);
        else ok=glTexImageNtr2D(GL_RGB16,128,64,GL_TEXTURE_COLOR0_TRANSPARENT,props_atlas,NULL);
        if(!ok)++sceneTextureFailures;
        assignPalette(11+FX_COUNT+i-GROUND_TEXTURE);
    }
    for(int i=0;i<ACTOR_COUNT;++i) poses[i].species=-1;
    /* Static meshes are uploaded as one geometry list, not rebuilt every frame. */
    sceneVertices=scenePolygons=0;recording=true;decor();recording=false;
    decorVertices=sceneVertices;decorPolygons=scenePolygons;decorList[0]=listWords;
    DC_FlushRange(decorList,(listWords+1)*sizeof(decorList[0]));
}
void sceneDraw(const Game *g,const Game *old,int half,int displayTicks) {
    sceneVertices=scenePolygons=0;
    ++sceneRenderCount;textureBank^=1;
    bool newBattle=g->state==STATE_FIGHT&&previousState!=STATE_FIGHT&&previousState!=STATE_PAUSE;
    if(newBattle) {
        lightingSeed^=sceneRenderCount*747796405u+(uint32_t)(g->selectedPokemon+1)*2891336453u;
        battlePeriod=visualRandomPeriod(&lightingSeed);++sceneRandomSelections;
    }
    int wanted=lightingMode==4?battlePeriod:lightingMode;
    if(wanted!=period) {
        period=wanted;
        for(int i=0;i<3;++i){glBindTexture(0,textures[GROUND_TEXTURE+i]);assignPalette(11+FX_COUNT+i);}
        sceneVertices=scenePolygons=listWords=0;recording=true;decor();recording=false;
        decorVertices=sceneVertices;decorPolygons=scenePolygons;decorList[0]=listWords;
        DC_FlushRange(decorList,(listWords+1)*sizeof(decorList[0]));sceneVertices=scenePolygons=0;
    }
    bool fight=g->state==STATE_FIGHT||g->state==STATE_PAUSE;
    bool frozen=g->state==STATE_PAUSE||g->state==STATE_RESULT;
    if(!frozen) waterFrame=(waterFrame+displayTicks)%64;
    if(g->state!=STATE_RESULT) resultFrames=resultClock=0;
    else {
        if(resultFrames<120) resultFrames+=displayTicks;
        resultClock=(resultClock+displayTicks)%240;
    }
    frameDelta=frozen?0:displayTicks;
    renderHalf=frozen?0:half;
    if(g->state!=old->state||!fight)old=g;
    if(previousState!=g->state&&g->state==STATE_FIGHT&&previousState!=STATE_PAUSE)
        for(int i=0;i<ACTOR_COUNT;++i) {memset(&poses[i],0,sizeof(poses[i]));poses[i].species=-1;}
    if(newBattle || trailWave!=g->wave) {
        memset(actorTrails,0,sizeof(actorTrails));
        trailWave=g->wave;
    }
    previousState=g->state;
    int playerX=visualLerp(old->playerX,g->playerX,frozen?2:half)/POS_SCALE;
    int playerY=visualLerp(old->playerY,g->playerY,frozen?2:half)/POS_SCALE;
    int enemyX=visualLerp(old->enemyX,g->enemyX,frozen?2:half)/POS_SCALE;
    int enemyY=visualLerp(old->enemyY,g->enemyY,frozen?2:half)/POS_SCALE;
    int rivalX=visualLerp(old->rival.x,g->rival.x,frozen?2:half)/POS_SCALE;
    int rivalY=visualLerp(old->rival.y,g->rival.y,frozen?2:half)/POS_SCALE;
    int focusX=g->enemyHp || !g->rival.hp?enemyX:rivalX;
    int focusY=g->enemyHp || !g->rival.hp?enemyY:rivalY;
    int ultimateTicks=g->lastAttack==4?g->attackAnim:0;
    for(int i=0;i<displayTicks;++i)
        visualCameraStep(&camera,playerX,playerY,focusX,focusY,g->state==STATE_FIGHT,frozen,ultimateTicks);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof32(inttof32(-2),inttof32(2),-6144,6144,inttof32(-4),inttof32(4));
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glScalef32(camera.zoom*16,camera.zoom*16,inttof32(1));
    glTranslate3f32(-camera.x/4,camera.y/4,0);
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(0));
    ground();glCallList(decorList);sceneVertices+=decorVertices;scenePolygons+=decorPolygons;
    waterRipples();
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(0));
    glBindTexture(0,textures[GROUND_TEXTURE+1]);boundTextureSize=64;glColor(RGB15(31,31,31));
    for(int i=0;i<arenaPropCount;++i) {
        const ArenaProp *p=&arenaProps[i];
        if(p->kind==PROP_TREE)
            billboard(p->x,groundY(p->y),p->width,p->height,spriteDepth(p->y));
    }

    sceneryProps();
    if(period>=2) {
        noTexture();
        for(int lamp=0;lamp<2;++lamp)for(int ring=0;ring<3;++ring) {
            int x=lanternX[lamp],y=LANTERN_Y+3,rad=22-ring*6;
            glPolyFmt(POLY_ALPHA((period==3?3:1)+ring*2)|POLY_CULL_NONE|POLY_ID(56+ring));
            glColor(RGB15(31,23,10));beginPrimitive(GL_TRIANGLE_STRIP);
            for(int i=0;i<=12;++i) {
                int a=i*32768/12;worldVertex(x,y,2+ring);
                worldVertex(x+cosLerp(a)*rad/4096,y+sinLerp(a)*rad/4096,2+ring);
            }
            endPrimitive();
        }
    }
    /* A handful of drifting leaves, at the margins of the playable clearing. */
    noTexture();glPolyFmt(POLY_ALPHA(19)|POLY_CULL_NONE|POLY_ID(55));
    beginPrimitive(GL_QUADS);
    for(int i=0;i<8;++i) {
        int periodTicks=92+i*9;
        int t=(g->frameCounter+i*43u)%periodTicks;
        if(t>=periodTicks-12) continue;
        int x=(i&1?242:24)+sinLerp(t*640+i*4096)/512;
        int y=66+(i/2)*37+t/9;
        int height=25-t*23/(periodTicks-12);
        int width=1+abs(sinLerp(t*960+i*1024))/2048;
        color(i%3?RGB15(18,24,8):RGB15(26,22,8));
        worldVertex(x-width,y,height);worldVertex(x,y-2,height);
        worldVertex(x+width,y,height);worldVertex(x,y+2,height);
    }
    for(int i=0;i<6;++i) {
        int phase=(g->frameCounter+i*29u)%128;
        if(phase<16 || phase>104) continue;
        int x=(i&1?238:19)+sinLerp(phase*420+i*3010)/768;
        int y=91+i*15, h=4+phase/8;
        color(period==3?RGB15(28,27,11):RGB15(24,27,15));
        worldVertex(x-1,y,h);worldVertex(x,y-1,h);
        worldVertex(x+1,y,h);worldVertex(x,y+1,h);
    }
    endPrimitive();
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(0));
    if(fight || g->state==STATE_RESULT) {
        int species=gamePlayerSpecies(g),enemySpecies=gameEnemySpecies(g);
        int reaction=g->state!=STATE_RESULT?0:g->result==RESULT_WIN?1:g->result==RESULT_KO?-1:0;
        int playerFacing=visualTargetFacing(poses[0].facing,focusX-playerX,focusY-playerY);
        int enemyFacing=visualTargetFacing(poses[1].facing,playerX-enemyX,playerY-enemyY);
        int playerStrike=fight?strikeTime(&g->casts[0],species,renderHalf):-1;
        int enemyStrike=fight?strikeTime(&g->casts[1],enemySpecies,renderHalf):-1;
        if(playerStrike>=0) playerFacing=g->casts[0].facing;
        if(enemyStrike>=0) enemyFacing=g->casts[1].facing;
        if(fight && g->cpuWindup) {
            const MoveDef *move=&starters[enemySpecies].moves[g->cpuMove];
            const NativeAnim *attack=&nativeSpecies[enemySpecies].anim[2];
            int total=gameCpuWindupTicks(g)*2;
            int elapsed=total-g->cpuWindup*2+renderHalf;
            int release=move->kind==MOVE_DASH?attack->rushTime:attack->hitTime;
            enemyStrike=release*elapsed/total;
            if(move->kind==MOVE_DASH)
                enemyFacing=visualDirection(g->cpuAimX-g->enemyX,g->cpuAimY-g->enemyY);
        }
        bool team=g->mode==MODE_TEAM && g->state!=STATE_RESULT;
        if(team) {
            teamMarker(playerX,playerY,true);
            if(g->enemyHp) teamMarker(enemyX,enemyY,false);
            if(g->rival.hp) teamMarker(rivalX,rivalY,false);
        }
        actor(0,species,g->hurtAnim?3:g->moving?1:0,playerFacing,g->casts[0].serial,playerX,playerY,fight?g->dodgeTimer:0,0,reaction,playerStrike);
        int enemyReaction=g->mode==MODE_TEAM && !g->enemyHp?-1:-reaction;
        actor(1,enemySpecies,g->enemyHurtAnim?3:g->enemyMoving?1:0,enemyFacing,g->casts[1].serial,enemyX,enemyY,fight?g->enemyDodgeTimer:0,0,enemyReaction,enemyStrike);
        if(g->partnerHp>0) {
            int partnerX=visualLerp(old->partnerX,g->partnerX,frozen?2:half)/POS_SCALE;
            int partnerY=visualLerp(old->partnerY,g->partnerY,frozen?2:half)/POS_SCALE;
            if(team) teamMarker(partnerX,partnerY,true);
            bool targetsRival=g->rival.hp && (g->rival.aimAtPartner || !g->enemyHp);
            int facing=visualTargetFacing(poses[2].facing,(targetsRival?rivalX:focusX)-partnerX,(targetsRival?rivalY:focusY)-partnerY);
            actor(2,(g->selectedPokemon+1)%4,g->partnerMoving?1:0,facing,g->partnerAttackSerial,partnerX,partnerY,false,0,reaction,-1);
        }
        if(g->mode==MODE_TEAM) {
            bool targetsPartner=g->rival.windup?g->rival.aimAtPartner:g->partnerHp>0;
            int x=targetsPartner?g->partnerX/POS_SCALE:playerX,y=targetsPartner?g->partnerY/POS_SCALE:playerY;
            int facing=visualTargetFacing(poses[3].facing,x-rivalX,y-rivalY);
            int rivalReaction=g->rival.hp?-reaction:-1;
            actor(3,gameRivalSpecies(g),g->rival.hurt?3:g->rival.moving?1:0,facing,g->rival.attackSerial,rivalX,rivalY,false,0,rivalReaction,-1);
        }
    }
    if(fight) {
        for(int i=0;i<PROJECTILE_CAP;++i) {
            const Projectile *p=&g->projectiles[i];if(p->life<=0) continue;
            int x=visualLerp(p->prevX,p->x,frozen?2:half)/POS_SCALE;
            int y=visualLerp(p->prevY,p->y,frozen?2:half)/POS_SCALE;
            int age=p->age*2-(2-renderHalf);if(age<0)age=0;
            int facing=visualDirection(p->vx,p->vy),height=13;
            if(age<8)attackOrigin(p->species,facing,&x,&height,8-age);
            glPolyFmt(POLY_ALPHA(p->life<3?9+p->life*6:27)|POLY_CULL_NONE|POLY_ID(4+i));
            if(!p->mine) ellipse(x,y,4,3,3,RGB15(25,6,6));
            fxBind(PROJECTILE_TEXTURE+i,p->style,facing,(age/3)%6);
            effectAt(x,y,p->ultimate?30:20,height);
            if(p->ultimate||p->style%5==3||p->style==FX_SOLAR) {
                glPolyFmt(POLY_ALPHA(10)|POLY_CULL_NONE|POLY_ID(4+i));
                for(int j=1;j<=2;++j)effectAt(x-p->vx*j/(POS_SCALE*2),y-p->vy*j/(POS_SCALE*2),18-j*3,height);
            }
        }
        for(int i=0;i<CAST_CAP;++i)drawCast(&g->castEvents[i],CAST_TEXTURE+i);
        for(int i=0;i<IMPACT_CAP;++i) if(g->impacts[i].ticks) {
            const VisualImpact *impact=&g->impacts[i];
            int age=(IMPACT_TICKS-impact->ticks)*2+renderHalf;
            groundReaction(impact,i,age);
            if(impact->hit) {
                impactRing(impact,i,age);
                int frame=age*6/(IMPACT_TICKS*2);
                glPolyFmt(POLY_ALPHA(26-age/2)|POLY_CULL_NONE|POLY_ID(40+i));
                fxBind(IMPACT_TEXTURE+i,FX_IMPACT,0,frame);
                effectAt(impact->x/POS_SCALE,impact->y/POS_SCALE,22,13);
            }
        }
        if(g->evolutionAnim) {
            glPolyFmt(POLY_ALPHA(17)|POLY_CULL_NONE|POLY_ID(45));
            fxBind(EVOLUTION_TEXTURE,FX_EVOLVE,0,((36-g->evolutionAnim)*2+renderHalf)/4%6);
            effectAt(playerX,playerY,48,17);
        }
        if(g->cpuWindup) {
            glPolyFmt(POLY_ALPHA(24)|POLY_CULL_NONE|POLY_ID(46));
            const MoveDef *m=&starters[gameEnemySpecies(g)].moves[g->cpuMove];
            int ax=g->enemyX/POS_SCALE,ay=g->enemyY/POS_SCALE;
            ellipse(ax,ay,5,3,3,RGB15(31,19,5));
            if(m->kind==MOVE_PROJECTILE || m->kind==MOVE_DASH) {
                int targetX=m->kind==MOVE_DASH?g->cpuAimX:g->playerX;
                int targetY=m->kind==MOVE_DASH?g->cpuAimY:g->playerY;
                int stop=arenaSweep(g->enemyX,g->enemyY,targetX,targetY,m->kind==MOVE_DASH?ARENA_ACTOR_MARGIN:2,NULL);
                int tx=arenaAt(g->enemyX,targetX,stop)/POS_SCALE;
                int ty=arenaAt(g->enemyY,targetY,stop)/POS_SCALE;
                for(int i=1;i<8;++i) ellipse(ax+(tx-ax)*i/8,ay+(ty-ay)*i/8,2,2,3,RGB15(31,6,6));
            }
            else {
                int x=g->cpuAimX/POS_SCALE,y=g->cpuAimY/POS_SCALE;
                for(int i=0;i<16;++i) {int a=i*2048;ellipse(x+cosLerp(a)*m->range/4096,y+sinLerp(a)*m->range/4096,2,2,3,RGB15(31,6,6));}
            }
        }
    } else if(g->state==STATE_SELECT) actor(0,g->selectedPokemon,0,0,0,128,129,false,64,0,-1);
    else if(g->state==STATE_OPPONENT) {
        actor(0,g->selectedPokemon,0,0,0,86,127,false,48,0,-1);
        actor(1,g->opponentPokemon,0,0,0,171,127,false,48,0,-1);
    }
    if(sceneVertices>sceneMaxVertices)sceneMaxVertices=sceneVertices;
    if(scenePolygons>sceneMaxPolygons)sceneMaxPolygons=scenePolygons;
    glFlush(0);
}
