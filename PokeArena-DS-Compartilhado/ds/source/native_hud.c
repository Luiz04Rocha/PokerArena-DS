/* Native touch UI adapted from the user's supplied dual-screen screenshots. */
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"
#include "screens.h"
#include "generated_visuals.h"
#include "generated_sprites.h"
#include "native_sprite_decode.h"
#include "visuals.h"
#include "generated_title.inc"
const char *sceneLightingLabel(void);
enum { TITLE_INK=1,TITLE_SHADOW,TITLE_PULSE };
enum { TITLE_MOTION_ROW=8,TITLE_SPRITE_ROW=104,TITLE_WALK_LEG=144,TITLE_SWAP_FADE=10 };
enum { TITLE_ICON_WIDTH=52,TITLE_ICON_HEIGHT=44 };
enum { BACK=32,PANEL,BORDER,WHITE,MUTED,GREEN,ORANGE,CYAN,PURPLE,DARK,RED,YELLOW,SELECTED };
enum { TITLE_LOW_COLORS=BACK-(TITLE_PULSE+4),TITLE_MIDDLE_COLORS=TITLE_ART_PALETTE-(SELECTED+1) };
_Static_assert(TITLE_LOW_COLORS+TITLE_MIDDLE_COLORS+256-TITLE_SPRITE_PALETTE>=STARTER_COUNT*15,
               "Four title sprites must fit in the unreserved main-screen colors");
enum { HUD_EDGE=45,HUD_SURFACE,HUD_CHIP, MOVE_FACE=53,MOVE_BEVEL=58,HUD_INK=63 };
enum { CARD_AMBER,CARD_BLUE,CARD_OLIVE,CARD_ROSE,CARD_VIOLET };
typedef enum { ICON_PLAY,ICON_DUEL,ICON_SOLO,ICON_SOUND,ICON_HELP,ICON_CREDITS } MenuIcon;
enum { MENU_SKY=240,MENU_CLOUD,MENU_GRASS,MENU_GRASS_LIGHT,MENU_STEEL,MENU_WOOD,MENU_PAPER,
       MENU_PLAY_FACE,MENU_PLAY_BEVEL,MENU_SKY_TOP,MENU_SKY_LOW,MENU_GROVE,MENU_PLATE,MENU_GOLD_DARK };
enum { MENU_SLATE=8,MENU_FRAME,MENU_GRID,MENU_FACE,MENU_FIELD,MENU_STEEL_BLUE,MENU_INK,MENU_BLUE_EDGE,
       MENU_BLUE_FACE,MENU_BLUE_FIELD,MENU_SHADE,MENU_SILVER,ICON_METAL,ICON_LIGHT,ICON_SHADE,
       ICON_RED,ICON_RED_SHADE,ICON_REFLECTION };
static uint8_t bottomPixels[256*192] __attribute__((aligned(4)));
static uint8_t overlayPixels[256*192] __attribute__((aligned(4)));
static uint8_t *canvas;
static uint8_t portraitCache[11][2][32*32];
static uint8_t titleSpriteIndices[STARTER_COUNT][16];
typedef struct {
    int family,species,seenForms,phase,leg,turns,pose,look,incoming;
} TitleWalker;
static TitleWalker titleCast[STARTER_COUNT];
static unsigned titlePaletteDirty;
static uint32_t titleSeed;
static volatile unsigned titleSeenForms,titleSeenPoses;
static uint8_t titleIconPixels[TITLE_TILE_COUNT][TITLE_ICON_HEIGHT][TITLE_ICON_WIDTH];
static bool bottomDirty,overlayDirty;
static int overlayFirstRow;
static void initTitleIcons(void);
static void initMenuPalette(void) {
    const u16 colors[]={RGB15(4,5,7),RGB15(6,7,9),RGB15(9,10,12),RGB15(9,10,13),
                        RGB15(5,6,8),RGB15(16,18,22),RGB15(2,3,4),RGB15(9,21,31),
                        RGB15(4,11,18),RGB15(7,17,23),RGB15(3,4,6),RGB15(22,24,27),
                        RGB15(22,24,26),RGB15(30,30,29),RGB15(12,15,19),
                        RGB15(30,6,7),RGB15(18,3,7),RGB15(31,20,19)};
    for(unsigned i=0;i<sizeof(colors)/sizeof(colors[0]);++i) BG_PALETTE_SUB[MENU_SLATE+i]=colors[i];
}
void nativeHudInit(void) {
    initMenuPalette();
    const u16 colors[]={RGB15(3,5,10),RGB15(4,7,14),RGB15(12,17,27),RGB15(31,31,31),RGB15(20,23,29),RGB15(9,28,15),RGB15(31,14,3),RGB15(12,25,31),RGB15(24,12,31),RGB15(2,3,7),RGB15(29,6,7),RGB15(31,25,6),RGB15(7,12,24)};
    for(unsigned i=0;i<sizeof(colors)/sizeof(colors[0]);++i) {BG_PALETTE[32+i]=colors[i];BG_PALETTE_SUB[32+i]=colors[i];}
    const u16 frameColors[]={RGB15(2,4,3),RGB15(7,10,5),RGB15(21,22,16),RGB15(24,24,17),RGB15(28,27,19),RGB15(31,30,21)};
    for(unsigned i=0;i<sizeof(frameColors)/sizeof(frameColors[0]);++i) BG_PALETTE[TITLE_INK+i]=frameColors[i];
    for(unsigned i=0;i<sizeof(titlePalette)/sizeof(titlePalette[0]);++i) BG_PALETTE[TITLE_ART_PALETTE+i]=titlePalette[i];
    /* Each walker keeps all 15 opaque sprite colors in gaps around the main palette. */
    for(int walker=0;walker<STARTER_COUNT;++walker) for(int color=1;color<16;++color) {
        int slot=walker*15+color-1;
        int index=slot<TITLE_LOW_COLORS ? TITLE_PULSE+4+slot :
                  slot<TITLE_LOW_COLORS+TITLE_MIDDLE_COLORS ? SELECTED+1+slot-TITLE_LOW_COLORS :
                  TITLE_SPRITE_PALETTE+slot-TITLE_LOW_COLORS-TITLE_MIDDLE_COLORS;
        titleSpriteIndices[walker][color]=index;
    }
    for(int i=0;i<ELEMENT_COUNT;++i)BG_PALETTE_SUB[48+i]=elementColors[i];
    const u16 moveFaces[]={RGB15(25,13,5),RGB15(7,16,25),RGB15(18,19,10),RGB15(25,9,15),RGB15(14,10,22)};
    const u16 moveBevels[]={RGB15(31,20,9),RGB15(13,23,31),RGB15(24,25,16),RGB15(31,15,21),RGB15(22,17,30)};
    for(unsigned i=0;i<sizeof(moveFaces)/sizeof(moveFaces[0]);++i) {
        BG_PALETTE_SUB[MOVE_FACE+i]=moveFaces[i];
        BG_PALETTE_SUB[MOVE_BEVEL+i]=moveBevels[i];
    }
    BG_PALETTE_SUB[HUD_EDGE]=RGB15(11,18,24);
    BG_PALETTE_SUB[HUD_SURFACE]=RGB15(3,8,13);
    BG_PALETTE_SUB[HUD_CHIP]=RGB15(8,16,22);
    BG_PALETTE_SUB[HUD_INK]=RGB15(2,4,8);
    const u16 menuColors[]={RGB15(4,9,16),RGB15(7,14,21),RGB15(3,10,10),RGB15(6,14,13),
                           RGB15(15,23,28),RGB15(21,12,6),RGB15(31,27,18),RGB15(6,16,12),RGB15(12,26,16),
                           RGB15(3,7,13),RGB15(5,12,19),RGB15(4,12,14),RGB15(4,7,12),RGB15(19,12,4)};
    for(unsigned i=0;i<sizeof(menuColors)/sizeof(menuColors[0]);++i)BG_PALETTE_SUB[MENU_SKY+i]=menuColors[i];
    for(int i=0;i<11;++i)dmaCopy(nativeSpecies[i].palette,BG_PALETTE_SUB+64+i*16,32);
    for(int species=0;species<11;++species)for(int variant=0;variant<2;++variant) {
        int size=variant?30:14;
        for(int y=0;y<size;++y)for(int x=0;x<size;++x) {
            int px=x*32/size,py=y*32/size;
            int index=((py/8)*4+px/8)*16+(py%8)*2+(px%8)/4;
            int c=(nativeSpecies[species].portrait[index]>>((px%4)*4))&15;
            portraitCache[species][variant][y*32+x]=c?64+species*16+c:CYAN;
        }
    }
    initTitleIcons();
}
void nativeHudPresent(u16 *bottom,u16 *overlay) {
    if(REG_VCOUNT<192||REG_VCOUNT>=214)return;
    if(overlayDirty) {
        for(int i=0;i<STARTER_COUNT;++i) if(titlePaletteDirty&(1u<<i)) {
            const uint16_t *palette=nativeSpecies[titleCast[i].species].palette;
            for(int color=1;color<16;++color) BG_PALETTE[titleSpriteIndices[i][color]]=palette[color];
        }
        titlePaletteDirty=0;
        size_t start=overlayFirstRow*256;
        dmaCopy(overlayPixels+start,(uint8_t *)overlay+start,sizeof(overlayPixels)-start);
        overlayDirty=false;
    }
    if(bottomDirty){dmaCopy(bottomPixels,bottom,sizeof(bottomPixels));bottomDirty=false;}
}
static const char *types[5]={"Fogo","Agua","Planta","Eletrico","Normal"};
static const char *kinds[4]={"DISPARO","AREA","INVESTIDA","CONTATO"};
static void rect(int x,int y,int w,int h,int c) {
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(x+w>256)w=256-x;
    if(y+h>192)h=192-y;
    if(w<=0||h<=0)return;
    for(int yy=y;yy<y+h;++yy)memset(canvas+yy*256+x,c,(size_t)w);
}
static void panel(int x,int y,int w,int h,int border,int fill) {
    rect(x+2,y,w-4,h,border);rect(x,y+2,w,h-4,border);
    rect(x+2,y+2,w-4,h-4,fill);
}
static const uint8_t *glyphFor(unsigned code) {
    static const uint8_t acuteE[8]={8,4,30,2,30,2,2,30};
    static const uint8_t cedillaC[8]={30,3,1,1,3,30,8,4};
    static const uint8_t tildeO[8]={22,12,14,18,18,18,18,14};
    if(code==0xc7) return cedillaC;
    if(code==0xc9) return acuteE;
    if(code==0xd5) return tildeO;
    return hudFont[(code>=32 && code<=127?code:'?')-32];
}
static void text(int x,int y,const char *s,int color,int scale) {
    for(;*s;++s,x+=6*scale) {
        const uint8_t *glyph=glyphFor((unsigned char)*s);
        if(scale==1) {
            for(int yy=0;yy<8;++yy) {
                int sy=y+yy;if(sy<0||sy>=192)continue;
                unsigned bits=glyph[yy];
                for(int xx=0;xx<6;++xx)if((bits&(1<<xx))&&x+xx>=0&&x+xx<256)canvas[sy*256+x+xx]=(uint8_t)color;
            }
            continue;
        }
        for(int yy=0;yy<8;++yy) for(int xx=0;xx<6;++xx)
            if(glyph[yy]&(1<<xx))rect(x+xx*scale,y+yy*scale,scale,scale,color);
    }
}
static void center(int y,const char *s,int color,int scale) {text((256-(int)strlen(s)*6*scale)/2,y,s,color,scale);}
static void textRight(int edge,int y,const char *s,int color) {
    text(edge-(int)strlen(s)*6,y,s,color,1);
}
static void outlinedText(int x,int y,const char *s,int color) {
    int origin=x;
    const char *label=s;
    for(;*s;++s,x+=6) {
        const uint8_t *glyph=glyphFor((unsigned char)*s);
        for(int row=-1;row<=8;++row) {
            int sy=y+row;
            if(sy<0 || sy>=192) continue;
            unsigned cur=row>=0 && row<8?glyph[row]:0;
            unsigned mask=(cur<<1)|cur|(cur<<2);
            if(row>0) mask|=glyph[row-1]<<1;
            if(row<7) mask|=glyph[row+1]<<1;
            for(int bit=0;bit<8;++bit) {
                int sx=x+bit-1;
                if((mask&(1u<<bit)) && sx>=0 && sx<256) canvas[sy*256+sx]=HUD_INK;
            }
        }
    }
    text(origin,y,label,color,1);
}
static void centeredLabel(const HudButton *button,int offset,const char *label,int color,bool outline) {
    int x=button->x+(button->w-(int)strlen(label)*6)/2;
    if(outline) outlinedText(x,button->y+offset,label,color);
    else text(x,button->y+offset,label,color,1);
}
static void gauge(int x,int y,int w,int h,int value,int max,int c) {
    rect(x,y,w,h,DARK);if(value<0)value=0;if(value>max)value=max;
    if(max>0) rect(x+1,y+1,(w-2)*value/max,h-2,c);
}
static void lifeBar(int x,int y,int w,int h,int hp,int max) {
    int fill=hp>0 && max>0?(w-2)*hp/max:0;
    if(fill>w-2) fill=w-2;
    if(!fill && hp>0) fill=1;
    int color=hp*4<max?RED:hp*2<max?ORANGE:GREEN;
    rect(x,y,w,h,HUD_INK);
    rect(x+1,y+1,w-2,h-2,HUD_EDGE);
    rect(x+1,y+1,fill,h-2,color);
    for(int i=9;i<fill;i+=9) rect(x+i,y+h-3,1,2,HUD_CHIP);
}
static void commandCard(const HudButton *b,int face,int bevel,int edge) {
    panel(b->x,b->y,b->w,b->h,edge,face);
    rect(b->x+3,b->y+2,b->w-6,2,bevel);
    rect(b->x+2,b->y+4,1,b->h-8,bevel);
    rect(b->x+4,b->y+b->h-4,b->w-8,2,edge);
}
static void portrait(int species,int x,int y,int size) {
    const uint8_t *source=portraitCache[species][size==30?1:0];
    for(int yy=0;yy<size;++yy)memcpy(canvas+(y+yy)*256+x,source+yy*32,(size_t)size);
}
static void menuButton(int x,int y,int w,int h,const char *name,bool selected) {
    panel(x,y+1,w,h,MENU_SHADE,MENU_SHADE);
    panel(x,y,w,h,selected?MENU_BLUE_EDGE:MENU_STEEL_BLUE,selected?MENU_BLUE_FACE:MENU_FACE);
    rect(x+5,y+2,w-10,1,selected?CYAN:MENU_SILVER);
    text(x+(w-(int)strlen(name)*6)/2,y+(h-8)/2,name,WHITE,1);
}
static void menuHeader(const char *name,int scale) {
    rect(0,0,256,24,MENU_SLATE);
    rect(0,24,256,1,MENU_SILVER);
    rect(0,25,256,1,MENU_STEEL_BLUE);
    rect(8,11,16,2,MENU_FRAME);rect(232,11,16,2,MENU_FRAME);
    rect(8,10,4,4,CYAN);rect(244,10,4,4,CYAN);
    center(scale==2?4:9,name,WHITE,scale);
}
static void menuLandscape(void) {
    rect(0,0,256,192,MENU_GRID);
    for(int x=0;x<256;x+=10) rect(x,0,1,192,MENU_FRAME);
    for(int y=0;y<192;y+=10) rect(0,y,256,1,MENU_FRAME);
    rect(0,160,256,32,MENU_SLATE);
    rect(0,159,256,1,MENU_STEEL_BLUE);
}
static void iconStroke(int x,int y,int ax,int ay,int bx,int by,int width,int color) {
    int dx=abs(bx-ax),dy=-abs(by-ay),sx=ax<bx?1:-1,sy=ay<by?1:-1;
    int error=dx+dy;
    for(;;) {
        rect(x+ax*2-width/2,y+ay*2-width/2,width,width,color);
        if(ax==bx && ay==by) break;
        int twice=error*2;
        if(twice>=dy) {error+=dy;ax+=sx;}
        if(twice<=dx) {error+=dx;ay+=sy;}
    }
}
static void menuSword(int x,int y,bool flip) {
    int hand=flip?11:4,tip=flip?3:12;
    iconStroke(x,y,hand,10,tip,2,7,HUD_INK);
    iconStroke(x,y,hand,10,tip,2,4,MENU_STEEL);
    iconStroke(x,y,hand,9,tip,2,2,WHITE);
    iconStroke(x,y,flip?13:2,13,hand,10,6,HUD_INK);
    iconStroke(x,y,flip?13:2,13,hand,10,3,MENU_WOOD);
    iconStroke(x,y,flip?8:2,8,flip?13:7,13,6,HUD_INK);
    iconStroke(x,y,flip?8:2,8,flip?13:7,13,3,YELLOW);
}
static void menuIcon(int x,int y,MenuIcon item,bool muted) {
    switch(item) {
    case ICON_PLAY:
        menuSword(x,y,false);
        break;
    case ICON_DUEL:
        menuSword(x,y,true);
        menuSword(x,y,false);
        break;
    case ICON_SOLO:
        iconStroke(x,y,2,11,7,5,8,HUD_INK);
        iconStroke(x,y,7,5,12,10,8,HUD_INK);
        iconStroke(x,y,12,10,14,8,8,HUD_INK);
        iconStroke(x,y,2,11,7,5,5,CYAN);
        iconStroke(x,y,7,5,12,10,5,CYAN);
        iconStroke(x,y,12,10,14,8,5,CYAN);
        iconStroke(x,y,5,7,7,5,2,WHITE);
        rect(x+5,y+25,24,2,MENU_STEEL);
        break;
    case ICON_SOUND:
        rect(x+4,y+10,14,14,HUD_INK);
        rect(x+12,y+5,9,24,HUD_INK);
        rect(x+6,y+12,10,10,MENU_STEEL);
        rect(x+14,y+7,5,20,WHITE);
        if(muted) iconStroke(x,y,3,13,13,3,4,ORANGE);
        else {
            rect(x+23,y+11,3,12,CYAN);
            rect(x+28,y+7,3,20,CYAN);
        }
        break;
    case ICON_HELP:
        panel(x+3,y+3,26,26,HUD_INK,HUD_CHIP);
        rect(x+10,y+7,12,3,WHITE);
        rect(x+19,y+10,3,6,CYAN);
        rect(x+14,y+15,7,3,CYAN);
        rect(x+13,y+17,4,3,CYAN);
        rect(x+13,y+23,4,3,WHITE);
        break;
    case ICON_CREDITS:
        panel(x+4,y+3,25,27,HUD_INK,MENU_WOOD);
        rect(x+9,y+6,16,20,MENU_PAPER);
        rect(x+10,y+9,12,2,MENU_WOOD);
        rect(x+10,y+14,12,2,MENU_WOOD);
        rect(x+10,y+19,8,2,MENU_WOOD);
        rect(x+7,y+6,2,21,YELLOW);
        break;
    default: break;
    }
}
static void pokeBall(int cx,int cy) {
    for(int y=-17;y<=17;++y) for(int x=-17;x<=17;++x) {
        int radius=x*x+y*y;
        if(radius>289) continue;
        int color=HUD_INK;
        if(radius<=225 && abs(y)>1) {
            bool lit=(x+3)*(x+3)+(y+4)*(y+4)<=196;
            color=y<0?(lit?ICON_RED:ICON_RED_SHADE):(lit?WHITE:ICON_METAL);
        }
        if(radius<=36) color=HUD_INK;
        if(radius<=16) color=y<1?WHITE:MENU_STEEL;
        rect(cx+x,cy+y,1,1,color);
    }
    rect(cx-9,cy-10,6,2,ICON_REFLECTION);
    rect(cx-11,cy-8,4,2,ICON_REFLECTION);
}
static void menuPortrait(int species,int x,int y,int size,int accent) {
    panel(x-2,y-2,size+4,size+4,accent,HUD_INK);
    portrait(species,x,y,size);
}
static void menuCard(const HudButton *b,int iconWidth,bool selected) {
    int x=b->x,y=b->y,w=b->w,h=b->h;
    panel(x+1,y+2,w,h,MENU_INK,MENU_INK);
    panel(x,y,w,h,selected?MENU_BLUE_EDGE:MENU_INK,selected?MENU_BLUE_FACE:MENU_FACE);
    if(iconWidth>0) {
        for(int row=3;row<h-3;++row)
            rect(x+3,y+row,iconWidth+(row-3)/4,1,selected?MENU_BLUE_FIELD:MENU_FIELD);
    }
    rect(x+4,y+2,w-8,1,selected?WHITE:MENU_SILVER);
    rect(x+2,y+4,1,h-8,selected?CYAN:MENU_STEEL_BLUE);
    rect(x+4,y+h-3,w-8,1,selected?CYAN:MENU_STEEL_BLUE);
    rect(x+w-3,y+4,1,h-8,selected?CYAN:MENU_STEEL_BLUE);
}
static int titleLetterWidth(unsigned char letter) {
    return letter=='I'?3:letter=='M'?5:4;
}
static void titleLabel(const HudButton *b,const char *label) {
    static const uint8_t glyphs[26][7]={
        ['A'-'A']={6,9,9,15,9,9,9},   ['C'-'A']={7,8,8,8,8,8,7},
        ['D'-'A']={14,9,9,9,9,9,14},  ['E'-'A']={15,8,8,14,8,8,15},
        ['G'-'A']={7,8,8,11,9,9,7},   ['H'-'A']={9,9,9,15,9,9,9},
        ['I'-'A']={7,2,2,2,2,2,7},    ['J'-'A']={3,1,1,1,9,9,6},
        ['L'-'A']={8,8,8,8,8,8,15},   ['M'-'A']={17,27,21,17,17,17,17},
        ['O'-'A']={6,9,9,9,9,9,6},    ['P'-'A']={14,9,9,14,8,8,8},
        ['R'-'A']={14,9,9,14,12,10,9},['S'-'A']={7,8,8,6,1,1,14},
        ['T'-'A']={15,6,6,6,6,6,6},   ['U'-'A']={9,9,9,9,9,9,6},
        ['Y'-'A']={9,9,9,6,6,6,6}
    };
    int width=0;
    for(const unsigned char *p=(const unsigned char *)label;*p;++p) width+=2*titleLetterWidth(*p)+1;
    int x=b->x+(b->w-width+1)/2,y=b->y+53;
    for(const unsigned char *p=(const unsigned char *)label;*p;++p) {
        int w=titleLetterWidth(*p);
        unsigned char letter=*p==0xc7?'C':*p==0xd5?'O':*p;
        if(letter>='A' && letter<='Z') {
            const uint8_t *glyph=glyphs[letter-'A'];
            for(int yy=0;yy<7;++yy) for(int xx=0;xx<w;++xx) if(glyph[yy]&(1u<<(w-1-xx)))
                rect(x+xx*2,y+yy*2,2,2,WHITE);
        }
        if(*p==0xc7) {rect(x+3,y+14,2,1,WHITE);rect(x+2,y+15,2,1,WHITE);}
        if(*p==0xd5) {rect(x+1,y-3,3,1,WHITE);rect(x+4,y-2,3,1,WHITE);}
        x+=2*w+1;
    }
}
static bool gearPixel(int x,int y) {
    int a=abs(x),b=abs(y),outer=a>b?a:b,inner=a<b?a:b;
    return a*a+b*b<=144 || (outer<=17 && inner<=3) || (inner>=7 && outer<=12 && a+b<=22);
}
static void gearIcon(int cx,int cy) {
    for(int y=-17;y<=17;++y) for(int x=-17;x<=17;++x) {
        if(!gearPixel(x,y)) continue;
        int color=ICON_METAL;
        if(!gearPixel(x-1,y) || !gearPixel(x+1,y) || !gearPixel(x,y-1) || !gearPixel(x,y+1)) color=HUD_INK;
        else if(!gearPixel(x-2,y) || !gearPixel(x,y-2)) color=ICON_LIGHT;
        else if(!gearPixel(x+2,y) || !gearPixel(x,y+2)) color=ICON_SHADE;
        int radius=x*x+y*y;
        if(radius<=47) color=y<0?ICON_SHADE:ICON_LIGHT;
        if(radius<=29) color=HUD_INK;
        if(radius<=12) color=y<0?MENU_STEEL:HUD_CHIP;
        rect(cx+x,cy+y,1,1,color);
    }
}
static void handheldIcon(int cx,int cy) {
    panel(cx-20,cy-18,40,21,HUD_INK,ICON_METAL);
    rect(cx-16,cy-15,32,1,ICON_LIGHT);
    rect(cx-17,cy-12,2,5,ICON_SHADE);rect(cx+15,cy-12,2,5,ICON_SHADE);
    panel(cx-13,cy-13,26,14,HUD_INK,MENU_STEEL);
    rect(cx-10,cy-10,20,8,HUD_EDGE);
    rect(cx-9,cy-9,18,1,CYAN);rect(cx-9,cy-8,9,1,MENU_STEEL);
    panel(cx-21,cy,42,20,HUD_INK,ICON_METAL);
    rect(cx-17,cy+2,34,2,ICON_LIGHT);
    rect(cx-18,cy+16,36,2,ICON_SHADE);
    panel(cx-11,cy+4,22,12,HUD_INK,MENU_STEEL);
    rect(cx-8,cy+7,16,6,HUD_EDGE);
    rect(cx-7,cy+8,14,1,CYAN);
    rect(cx-18,cy+9,6,2,HUD_INK);rect(cx-16,cy+7,2,6,HUD_INK);
    rect(cx+13,cy+10,2,2,HUD_INK);rect(cx+16,cy+7,2,2,HUD_INK);
    rect(cx+14,cy+14,3,1,GREEN);
}
static void hordeIcon(int cx,int cy) {
    static const uint8_t species[]={2,1,3};
    uint16_t pixels[1024];
    for(int i=0;i<3;++i) {
        const NativeAnim *anim=&nativeSpecies[species[i]].anim[NATIVE_IDLE];
        nativeDecodeSprite(anim->pixels,anim->pixels+anim->frameOffsets[1],pixels);
        const int size=60;
        const int8_t *anchor=anim->anchors;
        int left=cx+(i==0?-12:i==1?12:0)-size/2-(anchor?anchor[0]*size/anim->sourceSize:0);
        int top=cy+(i==2?16:8)-size+(anchor?anchor[1]:anim->foot[0])*size/anim->sourceSize;
        for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
            int srcX=x*64/size;
            int color=(pixels[(y*64/size)*16+srcX/4]>>((srcX%4)*4))&15;
            if(color) rect(left+x,top+y,1,1,64+species[i]*16+color);
        }
    }
}
static void initTitleIcons(void) {
    static void (*const drawIcon[TITLE_TILE_COUNT])(int,int)={
        [TITLE_PLAY]=pokeBall,[TITLE_MULTIPLAYER]=handheldIcon,[TITLE_HORDE]=hordeIcon,[TITLE_OPTIONS]=gearIcon
    };
    canvas=bottomPixels;
    for(int i=0;i<TITLE_TILE_COUNT;++i) {
        memset(bottomPixels,0,TITLE_ICON_HEIGHT*256);
        drawIcon[i](TITLE_ICON_WIDTH/2,21);
        for(int y=0;y<TITLE_ICON_HEIGHT;++y)
            memcpy(titleIconPixels[i][y],bottomPixels+y*256,TITLE_ICON_WIDTH);
    }
}
static void titleMenu(const Game *g) {
    static const char *labels[TITLE_TILE_COUNT]={"JOGAR","MULTIPLAYER","HORDA","OP\xC7\xD5" "ES"};
    static const uint8_t accents[TITLE_TILE_COUNT]={GREEN,CYAN,ORANGE,MENU_SILVER};
    menuLandscape();
    rect(0,0,256,5,MENU_INK);
    rect(10,4,236,1,MENU_STEEL_BLUE);
    rect(116,4,24,1,CYAN);
    rect(0,159,256,12,MENU_GRID);
    for(int x=0;x<256;x+=10) rect(x,159,1,12,MENU_FRAME);
    rect(0,173,256,1,MENU_STEEL_BLUE);
    rect(0,175,256,17,MENU_INK);
    for(int i=0;i<TITLE_TILE_COUNT;++i) {
        const HudButton *b=&titleTiles[i];
        bool selected=i==g->menuCursor;
        menuCard(b,0,selected);
        rect(b->x+4,b->y+51,b->w-8,b->h-55,selected?MENU_BLUE_FIELD:MENU_FIELD);
        rect(b->x+8,b->y+48,b->w-16,1,selected?MENU_BLUE_EDGE:MENU_STEEL_BLUE);
        rect(b->x+(b->w-18)/2,b->y+48,18,1,accents[i]);
        rect(b->x+9,b->y+8,6,1,accents[i]);
        rect(b->x+9,b->y+9,1,3,accents[i]);
        rect(b->x+b->w-15,b->y+8,6,1,accents[i]);
        rect(b->x+b->w-10,b->y+9,1,3,accents[i]);
        int iconX=b->x+(b->w-TITLE_ICON_WIDTH)/2,iconY=b->y+4;
        for(int y=0;y<TITLE_ICON_HEIGHT;++y) {
            uint8_t *row=canvas+(iconY+y)*256+iconX;
            for(int x=0;x<TITLE_ICON_WIDTH;++x)
                if(titleIconPixels[i][y][x]) row[x]=titleIconPixels[i][y][x];
        }
        titleLabel(b,labels[i]);
    }
    center(180,g->menuCursor==TITLE_MULTIPLAYER?"MULTIPLAYER LOCAL EM BREVE":"TOQUE OU USE O DIRECIONAL",WHITE,1);
}
static void menuKey(int x,int y,int width,const char *label) {
    panel(x,y,width,14,MENU_STEEL_BLUE,MENU_SLATE);
    text(x+(width-(int)strlen(label)*6)/2,y+4,label,WHITE,1);
}
static void modeMenu(const Game *g) {
    static const char *names[MODE_COUNT]={"1 VS 1","ONDAS SOLO","ONDAS EM DUPLA","2 VS 2"};
    static const char *details[MODE_COUNT]={"Duelo contra a CPU","Enfrente tres desafios","Lute com um parceiro","Parceiro e dois rivais"};
    static const MenuIcon icons[MODE_COUNT]={ICON_DUEL,ICON_SOLO,ICON_PLAY,ICON_DUEL};
    menuLandscape();
    bool horde=g->menuCursor==TITLE_HORDE;
    menuHeader(horde?"HORDA":"ESCOLHA O MODO",1);
    if(horde) {
        panel(23,37,210,22,MENU_STEEL_BLUE,MENU_SLATE);
        center(44,"TRES ONDAS DE ADVERSARIOS",WHITE,1);
    } else {
        panel(23,121,210,28,MENU_STEEL_BLUE,MENU_SLATE);
        center(127,"BATALHAS CONTRA A CPU",WHITE,1);
        center(138,"ESCOLHA 1 VS 1 OU 2 VS 2",MUTED,1);
    }
    for(int i=0;i<MODE_COUNT;++i) {
        if(gameIsHorde((GameMode)i)!=horde) continue;
        const HudButton *b=&modeTiles[i];
        bool selected=(int)g->mode==i;
        menuCard(b,43,selected);
        menuIcon(b->x+13,b->y+2,icons[i],false);
        text(b->x+56,b->y+7,names[i],WHITE,1);
        text(b->x+56,b->y+20,details[i],MUTED,1);
        if(selected) menuKey(b->x+b->w-28,b->y+10,19,"A");
    }
    menuButton(menuBack.x,menuBack.y,menuBack.w,menuBack.h,"< VOLTAR",false);
}
static void optionsMenu(const Game *g) {
    static const char *labels[]={"SOM","COMO JOGAR","CR\xC9" "DITOS"};
    const char *details[]={g->muted?"Desligado":"Ligado","Controles da arena","Projeto e recursos"};
    menuLandscape();
    menuHeader("OP\xC7\xD5" "ES",1);
    for(int i=0;i<OPTION_COUNT;++i) {
        const HudButton *b=&optionTiles[i];
        bool selected=g->menuCursor==i;
        menuCard(b,43,selected);
        menuIcon(b->x+13,b->y+1,(MenuIcon)(ICON_SOUND+i),g->muted);
        text(b->x+56,b->y+7,labels[i],WHITE,1);
        text(b->x+56,b->y+20,details[i],MUTED,1);
        if(selected) menuKey(b->x+b->w-28,b->y+10,19,"A");
    }
    menuButton(menuBack.x,menuBack.y,menuBack.w,menuBack.h,"< VOLTAR",false);
}
static unsigned titleRandom(void) {
    titleSeed^=titleSeed<<13;
    titleSeed^=titleSeed>>17;
    titleSeed^=titleSeed<<5;
    return titleSeed;
}
static int titlePickForm(TitleWalker *walker) {
    const int speciesCount=sizeof(nativeSpecies)/sizeof(nativeSpecies[0]);
    int stages=1+(speciesCount-1-walker->family)/STARTER_COUNT;
    int all=(1<<stages)-1;
    if(walker->seenForms==all) walker->seenForms=1<<(walker->species/STARTER_COUNT);
    int available=all&~walker->seenForms;
    int pick=titleRandom()%__builtin_popcount((unsigned)available);
    int stage=0;
    for(;stage<stages;++stage) if((available&(1<<stage)) && !pick--) break;
    walker->seenForms|=1<<stage;
    return walker->family+stage*STARTER_COUNT;
}
static void initTitleCast(void) {
    if(!titleSeed) titleSeed=(uint32_t)time(NULL)|1u;
    uint8_t families[STARTER_COUNT]={0,1,2,3};
    for(int i=STARTER_COUNT-1;i>0;--i) {
        int j=titleRandom()%(i+1);
        int swap=families[i];families[i]=families[j];families[j]=swap;
    }
    bool evolved=false;
    for(int i=0;i<STARTER_COUNT;++i) {
        TitleWalker *walker=&titleCast[i];
        *walker=(TitleWalker){.family=families[i],.phase=titleRandom()%(2*TITLE_WALK_LEG),
                              .leg=-1,.turns=2+titleRandom()%3,.incoming=-1};
        walker->species=titlePickForm(walker);
        evolved|=walker->species>=STARTER_COUNT;
    }
    if(!evolved) {
        TitleWalker *walker=&titleCast[titleRandom()%STARTER_COUNT];
        walker->species=titlePickForm(walker);
    }
    titlePaletteDirty=(1u<<STARTER_COUNT)-1;
}
static int titleArtColor(int red,int green,int blue);
static void titleWalker(int slot,unsigned ticks,int x0,int y0,int x1,int y1) {
    static const uint16_t *cached[STARTER_COUNT];
    static uint16_t decoded[STARTER_COUNT][1024];
    static uint8_t softShadow;
    static const uint8_t poses[]={NATIVE_IDLE,NATIVE_IDLE,NATIVE_ATTACK,NATIVE_CELEBRATE};
    TitleWalker *walker=&titleCast[slot];
    ticks+=walker->phase;
    int leg=ticks/TITLE_WALK_LEG,step=ticks%TITLE_WALK_LEG;
    if(walker->leg!=leg) {
        walker->incoming=walker->leg>=0 && --walker->turns==0?titlePickForm(walker):-1;
        if(walker->incoming>=0) walker->turns=2+titleRandom()%3;
        walker->leg=leg;
        walker->pose=poses[titleRandom()%4];
        if(slot<2 && walker->pose==NATIVE_CELEBRATE) walker->pose=NATIVE_ATTACK;
        walker->look=titleRandom()%3==0?0:-1;
    }
    int opacity=TITLE_SWAP_FADE;
    if(walker->incoming>=0 && step<2*TITLE_SWAP_FADE) {
        opacity=abs(step-TITLE_SWAP_FADE);
        if(step>=TITLE_SWAP_FADE && walker->species!=walker->incoming) {
            walker->species=walker->incoming;
            titlePaletteDirty|=1u<<slot;
        }
    }
    if(!opacity) return;
    const int pause=44;
    bool returning=leg&1;
    int progress=step<pause?0:step-pause;
    int distance=TITLE_WALK_LEG-pause;
    int travel=returning?distance-progress:progress;
    int x=x0+(x1-x0)*travel/distance;
    int bend=12*travel*(distance-travel)/(distance*distance);
    int y=y0+(y1-y0)*travel/distance+(returning?-bend:bend);
    int dx=x1-x0,dy=y1-y0;
    int slope=12*(distance-2*travel)/distance;
    int facing=visualDirection(returning?-dx:dx,returning?slope-dy:dy+slope);
    int species=walker->species;
    int pose=progress?NATIVE_WALK:walker->pose;
    int poseTicks=step;
    if(walker->incoming>=0) {
        if(step<2*TITLE_SWAP_FADE) pose=NATIVE_IDLE;
        else poseTicks-=2*TITLE_SWAP_FADE;
    }
    if(!progress && walker->look>=0) facing=walker->look;
    const NativeAnim *anim=&nativeSpecies[species].anim[pose];
    int index=facing*anim->frames+visualFrame(anim->durations,anim->frames,poseTicks,
                                           pose==NATIVE_IDLE||pose==NATIVE_WALK);
    int size=(slot<2?49:54)*anim->sourceSize/64;
    const uint16_t *begin=anim->pixels+anim->frameOffsets[index];
    if(cached[slot]!=begin) {
        nativeDecodeSprite(begin,anim->pixels+anim->frameOffsets[index+1],decoded[slot]);
        cached[slot]=begin;
    }
    titleSeenForms|=1u<<species;
    titleSeenPoses|=1u<<pose;
    const int8_t *anchor=anim->anchors?anim->anchors+4*index:NULL;
    int left=x-size/2-(anchor?anchor[0]*size/anim->sourceSize:0);
    int top=y-size+(anchor?anchor[1]:anim->foot[facing])*size/anim->sourceSize;
    if(!softShadow) softShadow=titleArtColor(17,20,16);
    int radius=(size+7)/8;
    rect(x-radius,y,2*radius,3,softShadow);
    rect(x-radius+2,y-1,2*radius-4,3,TITLE_SHADOW);
    rect(x-radius+3,y+3,2*radius-6,1,softShadow);
    int startY=top<TITLE_SPRITE_ROW?TITLE_SPRITE_ROW-top:0;
    int startX=left<2?2-left:0;
    int endX=left+size>254?254-left:size;
    for(int yy=startY;yy<size && top+yy<190;++yy) {
        const uint16_t *row=decoded[slot]+(yy*64/size)*16;
        uint8_t *target=canvas+(top+yy)*256;
        for(int xx=startX;xx<endX;++xx) {
            int src=xx*64/size;
            int color=(row[src/4]>>((src%4)*4))&15;
            if(color && (opacity==TITLE_SWAP_FADE || ((xx*3+yy*7)%TITLE_SWAP_FADE)<opacity))
                target[left+xx]=titleSpriteIndices[slot][color];
        }
    }
}
/* Match the indexed cover so the ambience shares its palette with the scenery. */
static int titleArtColor(int red,int green,int blue) {
    int best=0,distance=3*32*32;
    for(unsigned i=0;i<sizeof(titlePalette)/sizeof(titlePalette[0]);++i) {
        unsigned color=titlePalette[i];
        int dr=(int)(color&31)-red,dg=(int)((color>>5)&31)-green,db=(int)((color>>10)&31)-blue;
        int error=dr*dr+dg*dg+db*db;
        if(error<distance) {best=i;distance=error;}
    }
    return TITLE_ART_PALETTE+best;
}
static bool titleWaterPixel(int x,int y) {
    unsigned color=titlePalette[titlePixels[y*256+x]-TITLE_ART_PALETTE];
    int r=color&31,g=(color>>5)&31,b=(color>>10)&31;
    return g>10 && b+1>=g && b>=r+3;
}
static void titleAmbience(unsigned ticks) {
    static const struct {uint8_t x,phase;uint16_t period;} leaves[]={
        {23,0,192},{43,74,218},{52,123,238},{30,164,255},
        {204,23,208},{218,96,232},{232,150,256},{200,183,277}
    };
    static const uint8_t water[][2]={{210,89},{217,86},{236,81},{244,113},{236,159},{229,172}};
    static uint8_t leafGreen,leafGold,leafDark,waterGlint,foam;
    if(!leafGreen) {
        leafGreen=titleArtColor(20,25,11);
        leafGold=titleArtColor(28,24,11);
        leafDark=titleArtColor(10,15,6);
        waterGlint=titleArtColor(17,25,26);
        foam=titleArtColor(24,28,27);
    }
    for(unsigned i=0;i<sizeof(water)/sizeof(water[0]);++i) {
        unsigned phase=(ticks+i*13)%72;
        if(phase>=36) continue;
        int x=water[i][0]+phase/12,y=water[i][1];
        int length=2+(phase<18?phase:36-phase)/5;
        for(int dx=0;dx<length;++dx) if(titleWaterPixel(x+dx,y))
            rect(x+dx,y,1,1,waterGlint);
    }
    /* Foam threads flow down the spring face and break into the pool. */
    for(int i=0;i<3;++i) {
        unsigned phase=(ticks/2+i*9)%28;
        if(phase>=18) continue;
        int x=213+i*2+(phase>9),y=74+phase*2/3;
        for(int dy=0;dy<2;++dy) if(titleWaterPixel(x,y+dy))
            rect(x,y+dy,1,1,i==1?foam:waterGlint);
    }
    for(unsigned i=0;i<sizeof(leaves)/sizeof(leaves[0]);++i) {
        unsigned phase=(ticks+leaves[i].phase)%leaves[i].period;
        int y=8+phase*157/leaves[i].period;
        int sway=(phase+i*9)%48;
        int x=leaves[i].x+(sway<24?sway:48-sway)/2-6;
        int tint=(i%3)?leafGreen:leafGold;
        bool flipped=(phase/12+i)&1;
        rect(x,y,2,1,tint);
        rect(x-1,y+1,4,1,tint);
        rect(x+(flipped?1:-1),y+2,2,1,leafDark);
        rect(x+(flipped?2:-2),y+3,1,1,leafDark);
    }
}
static void titleCover(int firstRow,unsigned ticks) {
    size_t start=firstRow*256;
    memcpy(canvas+start,titlePixels+start,sizeof(titlePixels)-start);
    if(!firstRow) rect(0,0,256,2,TITLE_INK);
    rect(0,190,256,2,TITLE_INK);
    rect(0,firstRow,2,192-firstRow,TITLE_INK);
    rect(254,firstRow,2,192-firstRow,TITLE_INK);
    titleAmbience(ticks);
    /* Paint the two distant lanes first so the front pair can pass in front. */
    titleWalker(0,ticks,44,133,100,138);
    titleWalker(1,ticks,158,132,211,137);
    titleWalker(2,ticks,56,150,109,155);
    titleWalker(3,ticks,137,154,198,160);
    int pulse=(ticks/12)%8;
    int level=pulse<4?pulse:7-pulse;
    int y=173-(level==3);
    panel(66,y+2,124,13,TITLE_INK,TITLE_INK);
    panel(66,y,124,13,TITLE_SHADOW,TITLE_INK);
    rect(69,y+2,2,1,TITLE_PULSE+level);
    rect(185,y+2,2,1,TITLE_PULSE+level);
    center(y+3,"TOQUE PARA COME\xC7" "AR",TITLE_PULSE+level,1);
}
static void roundPips(int x,int y,int wins,int color) {
    for(int i=0;i<2;++i) {
        int fill=i<wins?color:MENU_FRAME;
        rect(x+i*10,y+1,8,4,fill);
        rect(x+i*10+1,y,6,6,fill);
        if(i<wins) rect(x+i*10+2,y+1,4,1,WHITE);
    }
}
static void matchup(const Game *g) {
    int species[2]={gamePlayerSpecies(g),gameEnemySpecies(g)};
    menuLandscape();
    menuHeader(gameModeName(g),1);
    for(int i=0;i<2;++i) {
        const PokemonDef *mon=&starters[i?species[i]:g->selectedPokemon];
        int accent=48+mon->moves[0].element;
        HudButton card={i?149:7,34,100,83,0};
        commandCard(&card,HUD_SURFACE,accent,HUD_EDGE);
        if(g->mode==MODE_TEAM) {
            int support=i?gameRivalSpecies(g):(g->selectedPokemon+1)%STARTER_COUNT;
            menuPortrait(species[i],card.x+16,46,30,accent);
            menuPortrait(support,card.x+58,54,14,48+starters[support].moves[0].element);
        } else menuPortrait(species[i],card.x+35,46,30,accent);
        centeredLabel(&card,50,i?mon->name:gamePlayerName(g),WHITE,false);
        rect(card.x+3,97,94,16,MENU_PLATE);
        const char *role=g->mode==MODE_TEAM?(i?"DUPLA RIVAL":"SUA DUPLA"):(i?"RIVAL":"LIDER");
        centeredLabel(&card,68,role,accent,false);
    }
    panel(111,59,34,26,MENU_GOLD_DARK,HUD_INK);
    center(67,"VS",YELLOW,1);
    panel(28,124,200,23,HUD_EDGE,HUD_INK);
    if(!gameIsHorde(g->mode)) {
        char score[32];
        int scoreY=g->mode==MODE_TEAM?128:132;
        snprintf(score,sizeof(score),"ROUND %d  |  %d - %d",g->duelRound,g->playerRounds,g->rivalRounds);
        center(scoreY,score,CYAN,1);
        roundPips(42,scoreY,g->playerRounds,CYAN);
        roundPips(196,scoreY,g->rivalRounds,ORANGE);
        if(g->mode==MODE_TEAM) {
            char partner[40];
            snprintf(partner,sizeof(partner),"PARCEIRO: %s",starters[(g->selectedPokemon+1)%STARTER_COUNT].name);
            center(138,partner,MUTED,1);
        }
    } else if(g->mode==MODE_DUO) {
        char partner[40];
        snprintf(partner,sizeof(partner),"PARCEIRO: %s",starters[(g->selectedPokemon+1)%STARTER_COUNT].name);
        center(132,partner,CYAN,1);
    } else center(132,gameWaveName(g),CYAN,1);
    const HudButton *pause=&hudButtons[7];
    commandCard(pause,HUD_SURFACE,HUD_EDGE,HUD_INK);
    centeredLabel(pause,8,"PAUSA",WHITE,false);
    centeredLabel(pause,21,"START",MUTED,false);
}
static const char *resultLabel(const Game *g) {
    if(!gameIsHorde(g->mode) && gameDuelContinues(g)) {
        if(g->result==RESULT_TIME) return "EMPATE";
        return g->result==RESULT_WIN?"ROUND VENCIDO":"ROUND PERDIDO";
    }
    return g->result==RESULT_WIN?"VITORIA!":g->result==RESULT_TIME?"TEMPO ESGOTADO":"DERROTA";
}
static void actionMenuCard(const HudButton *b,const char *title,const char *key,bool primary) {
    menuCard(b,43,primary);
    menuKey(b->x+8,b->y+10,34,key);
    text(b->x+57,b->y+13,title,WHITE,1);
}
static void pauseMenu(void) {
    menuLandscape();
    menuHeader("PAUSA",1);
    actionMenuCard(&pauseTiles[0],"CONTINUAR","START",true);
    actionMenuCard(&pauseTiles[1],"VOLTAR A SELECAO","B",false);
    panel(57,125,142,42,MENU_STEEL_BLUE,MENU_SLATE);
    center(130,"HORARIO DO CENARIO",MUTED,1);
    center(147,sceneLightingLabel(),WHITE,1);
    for(int i=2;i<4;++i) {
        const HudButton *b=&pauseTiles[i];
        menuCard(b,0,false);
        centeredLabel(b,4,i==2?"<":">",WHITE,true);
        centeredLabel(b,15,i==2?"L":"R",WHITE,true);
    }
    rect(0,174,256,18,MENU_SLATE);
    center(179,"L/R OU TOQUE NAS SETAS",MUTED,1);
}
static void resultMenu(const Game *g) {
    bool won=g->result==RESULT_WIN;
    bool more=gameDuelContinues(g);
    int accent=won?YELLOW:g->result==RESULT_TIME?CYAN:MUTED;
    menuLandscape();
    menuHeader(resultLabel(g),2);
    if(gameIsHorde(g->mode)) {
        panel(20,33,216,54,MENU_STEEL_BLUE,MENU_SLATE);
        menuPortrait(gamePlayerSpecies(g),31,45,30,accent);
        text(72,42,"LIDER",MUTED,1);
        text(72,54,gamePlayerName(g),WHITE,1);
        text(72,69,gameModeName(g),MUTED,1);
        char wave[16];
        snprintf(wave,sizeof(wave),"ONDA %d/%d",g->wave,WAVE_COUNT);
        textRight(224,69,wave,WHITE);
    } else {
        int enemy=gameEnemySpecies(g);
        const HudButton leader={12,33,74,53,0},rival={170,33,74,53,0};
        commandCard(&leader,HUD_SURFACE,won?YELLOW:CYAN,HUD_EDGE);
        commandCard(&rival,HUD_SURFACE,g->result==RESULT_KO?YELLOW:ORANGE,HUD_EDGE);
        menuPortrait(gamePlayerSpecies(g),34,40,30,CYAN);
        menuPortrait(enemy,192,40,30,ORANGE);
        centeredLabel(&leader,42,gamePlayerName(g),WHITE,false);
        centeredLabel(&rival,42,starters[enemy].name,WHITE,false);

        panel(89,33,78,53,HUD_EDGE,HUD_INK);
        char label[24],score[2];
        snprintf(label,sizeof(label),"ROUND %d",g->duelRound);
        center(38,label,MUTED,1);
        score[0]='0'+g->playerRounds;score[1]='\0';
        text(101,49,score,CYAN,2);
        score[0]='0'+g->rivalRounds;
        text(143,49,score,ORANGE,2);
        rect(124,56,8,2,MENU_SILVER);
        roundPips(98,68,g->playerRounds,CYAN);
        roundPips(140,68,g->rivalRounds,ORANGE);
        text(98,76,g->mode==MODE_TEAM?"NOS":"VOCE",MUTED,1);
        textRight(158,76,"CPU",MUTED);
        if(more && gameStage(g)>0) center(89,"EVOLUCAO MANTIDA",GREEN,1);
    }
    if(won && gameIsHorde(g->mode)) {
        rect(211,40,3,19,MENU_GOLD_DARK);
        rect(205,45,15,7,MENU_GOLD_DARK);
        rect(211,40,3,15,YELLOW);
        rect(207,46,11,3,YELLOW);
        rect(211,46,3,3,WHITE);
    }
    actionMenuCard(&resultTiles[0],more?(g->result==RESULT_TIME?"REPETIR ROUND":"PROXIMO ROUND"):"JOGAR NOVAMENTE","START",true);
    actionMenuCard(&resultTiles[1],"MENU INICIAL","B",false);
}
static void battle(const Game *g) {
    char s[40];
    int hp=gameMaxHp(g),evolution=gameEvolutionPercent(g),seconds=gameSecondsLeft(g);
    int enemy=gameEnemySpecies(g);
    bool evoReady=evolution==100 && gameStage(g)<gameFinalStage(g);
    panel(4,4,176,42,MENU_SILVER,MENU_SLATE);
    panel(184,4,68,42,MENU_SILVER,MENU_SLATE);
    int elementColor=48+gameMoveDef(g,0)->element;
    rect(7,43,170,1,MENU_STEEL_BLUE);
    rect(187,43,62,1,MENU_STEEL_BLUE);
    panel(7,7,34,34,elementColor,HUD_INK);
    portrait(gamePlayerSpecies(g),9,9,30);
    text(45,8,gamePlayerName(g),WHITE,1);
    snprintf(s,sizeof(s),"%d/%d",g->playerHp,hp);
    textRight(174,8,s,g->playerHp*4<hp?ORANGE:WHITE);
    lifeBar(45,19,130,10,g->playerHp,hp);
    text(45,33,"EVO",MUTED,1);
    gauge(64,34,53,5,evolution,100,PURPLE);
    snprintf(s,sizeof(s),"%d%%",evolution);
    textRight(141,33,s,evoReady?WHITE:PURPLE);
    const HudButton *evo=&hudButtons[6];
    panel(evo->x,evo->y,evo->w,evo->h,evoReady?MENU_BLUE_EDGE:MENU_STEEL_BLUE,evoReady?MENU_BLUE_FACE:MENU_INK);
    centeredLabel(evo,2,gameStage(g)==gameFinalStage(g)?"MAX":"EVO",evoReady?WHITE:MUTED,false);
    text(203,8,"TEMPO",MUTED,1);
    snprintf(s,sizeof(s),"%02d:%02d",seconds/60,seconds%60);
    bool duel=!gameIsHorde(g->mode);
    text(188,duel?17:22,s,seconds<=30?ORANGE:WHITE,2);
    if(duel) {
        snprintf(s,sizeof(s),"R%d  %d-%d",g->duelRound,g->playerRounds,g->rivalRounds);
        rect(191,34,54,9,MENU_INK);
        text(218-(int)strlen(s)*3,35,s,CYAN,1);
    }
    panel(4,49,248,18,MENU_SILVER,MENU_SLATE);
    if(g->mode==MODE_TEAM) {
        text(12,51,"PARCEIRO",CYAN,1);
        text(76,51,"RIVAL 1",WHITE,1);
        text(140,51,"RIVAL 2",WHITE,1);
        lifeBar(12,60,56,5,g->partnerHp,PARTNER_HP);
        lifeBar(76,60,56,5,g->enemyHp,g->enemyMaxHp);
        lifeBar(140,60,56,5,g->rival.hp,TEAM_RIVAL_HP);
        textRight(245,54,"VS 2x2",YELLOW);
    } else {
        portrait(enemy,7,51,14);
        rect(24,52,22,12,MENU_BLUE_FIELD);
        text(26,54,"CPU",CYAN,1);
        text(50,54,starters[enemy].name,WHITE,1);
        lifeBar(113,54,75,8,g->enemyHp,g->enemyMaxHp);
        if(g->mode==MODE_VS) textRight(245,54,"VS 1x1",YELLOW);
        else {
            snprintf(s,sizeof(s),"ONDA %d/%d",g->wave,WAVE_COUNT);
            textRight(245,54,s,g->wave==3?ORANGE:YELLOW);
        }
    }
    for(int i=0;i<MOVE_COUNT;++i) {
        const HudButton *b=&hudButtons[i];
        const MoveDef *m=gameMoveDef(g,i);
        bool ready=g->moveCooldowns[i]==0;
        bool active=g->attackAnim && g->lastAttack==i;
        int element=m->element;
        menuCard(b,22,ready||active);
        char key[2]={"ABXY"[i],0};
        panel(b->x+6,b->y+8,19,17,active?YELLOW:ready?CYAN:MENU_STEEL_BLUE,MENU_SLATE);
        text(b->x+12,b->y+13,key,ready||active?WHITE:MUTED,1);
        rect(b->x+6,b->y+27,19,2,48+element);
        text(b->x+33,b->y+5,m->name,ready||active?WHITE:MUTED,1);
        if(active) {
            rect(b->x+3,b->y+3,2,b->h-6,YELLOW);
            text(b->x+33,b->y+20,"ACIONADO",YELLOW,1);
        } else if(ready) {
            text(b->x+33,b->y+16,kinds[m->kind],MUTED,1);
            text(b->x+33,b->y+26,"PRONTO",GREEN,1);
        }
        else {
            int t=(g->moveCooldowns[i]*10+GAME_HZ-1)/GAME_HZ;
            snprintf(s,sizeof(s),"RECARGA %d.%ds",t/10,t%10);
            text(b->x+33,b->y+19,s,CYAN,1);
            gauge(b->x+33,b->y+30,b->w-41,3,m->cooldownTicks-g->moveCooldowns[i],m->cooldownTicks,CYAN);
        }
    }
    const HudButton *dodge=&hudButtons[4],*ult=&hudButtons[5],*pause=&hudButtons[7];
    bool dodgeReady=!g->dodgeCooldown;
    menuCard(dodge,0,g->dodgeTimer>0);
    centeredLabel(dodge,7,"ESQUIVA",WHITE,true);
    centeredLabel(dodge,20,g->dodgeTimer?"L  EM USO":dodgeReady?"L  PRONTA":"L  RECARGA",dodgeReady?WHITE:MUTED,true);
    bool ultimate=g->attackAnim && g->lastAttack==4;
    bool charged=g->ultimateCharge==100;
    menuCard(ult,0,charged||ultimate);
    const char *ultimateName=gameStage(g)>0?gameUltimateDef(g)->name:"ULTIMATE";
    int nameColor=charged?YELLOW:WHITE;
    const char *space=strchr(ultimateName,' ');
    if(space && (int)strlen(ultimateName)*6>ult->w-8) {
        snprintf(s,sizeof(s),"%.*s",(int)(space-ultimateName),ultimateName);
        centeredLabel(ult,4,s,nameColor,false);
        centeredLabel(ult,12,space+1,nameColor,false);
    } else centeredLabel(ult,7,ultimateName,nameColor,true);
    if(charged) strcpy(s,"R  PRONTO");
    else snprintf(s,sizeof(s),"R  %d%%",g->ultimateCharge);
    centeredLabel(ult,20,s,charged?YELLOW:WHITE,false);
    gauge(ult->x+9,ult->y+28,ult->w-18,4,g->ultimateCharge,100,charged?YELLOW:PURPLE);
    menuCard(pause,0,false);
    centeredLabel(pause,7,"PAUSA",WHITE,true);
    centeredLabel(pause,20,"START",MUTED,false);
}
static void selection(const Game *g) {
    bool opponent=g->state==STATE_OPPONENT;
    menuLandscape();
    menuHeader(opponent?"ESCOLHA O OPONENTE":"ESCOLHA SEU LIDER",1);
    int selected=opponent?g->opponentPokemon:g->selectedPokemon,n=0;
    for(int i=0;i<4;++i) {
        if(opponent&&i==g->selectedPokemon)continue;
        const HudButton *b=&pickerTiles[n++];
        int x=b->x,y=b->y;
        int element=starters[i].moves[0].element;
        menuCard(b,35,i==selected);
        menuPortrait(i,x+5,y+4,30,48+element);
        text(x+43,y+9,starters[i].name,WHITE,1);
        rect(x+43,y+22,4,4,48+element);
        text(x+51,y+21,types[element],MUTED,1);
    }
    static const char *descriptions[4][2]={
        {"Rapido e explosivo.","Brasas a distancia."},{"Resistente e versatil.","Jatos de agua e ondas."},
        {"Chicotes e folhas.","Pressao de perto e longe."},{"Agil e eletrizante.","Choques e trovoes."}};
    panel(4,119,248,42,MENU_STEEL_BLUE,MENU_SLATE);text(12,124,starters[selected].name,WHITE,1);
    text(12,137,descriptions[selected][0],WHITE,1);text(12,149,descriptions[selected][1],MUTED,1);
    menuButton(4,168,118,20,"< VOLTAR",false);
    menuButton(130,168,122,20,opponent?"LUTAR! >":"ESCOLHER >",true);
}
static void helpOrCreditsMenu(bool help) {
    menuLandscape();
    menuHeader(help?"COMO JOGAR":"CR\xC9" "DITOS",1);
    panel(12,34,232,118,MENU_STEEL_BLUE,MENU_SLATE);
    if(help) {
        static const char *keys[]={"DIR","ABXY","L/R","SEL","START"};
        static const char *actions[]={"Mover pela arena","Usar os quatro golpes","Esquiva / Ultimate",
                                      "Confirmar evolucao","Pausar a batalha"};
        for(int i=0;i<5;++i) {
            int y=40+18*i;
            menuKey(20,y,42,keys[i]);
            text(70,y+4,actions[i],WHITE,1);
        }
        center(135,"A TELA DE TOQUE TAMBEM FUNCIONA",MUTED,1);
    } else {
        center(44,GAME_NAME " - Luiz",WHITE,1);
        center(60,"Inspirado em Poke Arena 2D",MUTED,1);
        rect(26,76,204,1,MENU_STEEL_BLUE);
        center(85,"Pokemon: Nintendo /",WHITE,1);
        center(98,"Game Freak / Creatures",WHITE,1);
        center(124,"Creditos completos no ZIP.",MUTED,1);
    }
    menuButton(menuBack.x,menuBack.y,menuBack.w,menuBack.h,"< VOLTAR",false);
}
void nativeHudDraw(const Game *g) {
    memset(bottomPixels,g->state==STATE_FIGHT && !g->introTicks?MENU_FRAME:BACK,sizeof(bottomPixels));canvas=bottomPixels;
    if(g->state==STATE_FIGHT && g->introTicks)matchup(g);
    else if(g->state==STATE_FIGHT)battle(g);
    else if(g->state==STATE_SELECT||g->state==STATE_OPPONENT)selection(g);
    else if(g->state==STATE_TITLE) titleMenu(g);
    else if(g->state==STATE_MODE) modeMenu(g);
    else if(g->state==STATE_OPTIONS) optionsMenu(g);
    else if(g->state==STATE_PAUSE) pauseMenu();
    else if(g->state==STATE_RESULT) resultMenu(g);
    else helpOrCreditsMenu(g->state==STATE_HELP);
    DC_FlushRange(bottomPixels,sizeof(bottomPixels));bottomDirty=true;
}
void nativeHudDrawOverlay(const Game *g,int displayTicks) {
    static int previousOverlay=-1;
    static unsigned titleTicks,resultTicks;
    static bool titleWasActive;
    int count=gameIntroCount(g),overlayState=(int)g->state*16;
    if(g->state==STATE_FIGHT)
        overlayState+=count?count:g->roundDelay?4:g->cpuWindup?5:g->frameCounter<15?6:0;
    bool duelResult=g->state==STATE_RESULT && !gameIsHorde(g->mode);
    if(!duelResult) resultTicks=0;
    else if(resultTicks<20) resultTicks+=displayTicks;
    bool showRoundScore=resultTicks>=20;
    if(g->state==STATE_RESULT) overlayState+=g->result+(showRoundScore?4:0);
    bool title=g->state==STATE_TITLE || g->state==STATE_OPTIONS;
    bool changed=overlayState!=previousOverlay;
    unsigned oldTicks=titleTicks;
    if(title && !titleWasActive) initTitleCast();
    titleTicks=title?titleTicks+displayTicks:0;
    titleWasActive=title;
    if(!changed && (!title || oldTicks/2==titleTicks/2)) return;
    previousOverlay=overlayState;
    int firstRow=title && !changed?TITLE_MOTION_ROW:0;
    canvas=overlayPixels;
    if(title) titleCover(firstRow,titleTicks);
    else memset(overlayPixels,0,sizeof(overlayPixels));
    if(g->state==STATE_FIGHT) {
        if(count) {
            char names[40],digit[2]={(char)('0'+count),0};
            snprintf(names,sizeof(names),"%s  VS  %s",gamePlayerName(g),starters[gameEnemySpecies(g)].name);
            panel(9,8,238,23,SELECTED,DARK);
            rect(13,12,2,15,ORANGE);rect(241,12,2,15,CYAN);
            center(16,names,WHITE,1);
            int digitY=37;
            if(!gameIsHorde(g->mode)) {
                char round[12];
                snprintf(round,sizeof(round),"ROUND %d",g->duelRound);
                panel(96,31,64,12,BORDER,DARK);
                center(34,round,CYAN,1);
                digitY=45;
            }
            panel(109,digitY,38,34,BORDER,DARK);
            rect(114,digitY+2,28,1,CYAN);
            center(digitY+5,digit,YELLOW,3);
        } else if(g->roundDelay) {
            char wave[40];
            snprintf(wave,sizeof(wave),"ONDA %d/%d - %s",g->wave,WAVE_COUNT,gameWaveName(g));
            panel(20,8,216,34,BORDER,DARK);
            center(14,wave,YELLOW,1);
            center(27,"RECUPERACAO DE ATE 20 PV",GREEN,1);
        } else if(g->cpuWindup) {
            char warning[40];
            const MoveDef *move=&starters[gameEnemySpecies(g)].moves[g->cpuMove];
            snprintf(warning,sizeof(warning),"CPU: %s",move->name);
            panel(33,8,190,20,ORANGE,DARK);
            center(14,warning,ORANGE,1);
        }
        else if(g->frameCounter<15) {
            panel(82,8,92,24,CYAN,DARK);
            center(12,"LUTEM!",YELLOW,2);
        }
    } else if(g->state==STATE_PAUSE)center(20,"PAUSA",WHITE,2);
    else if(g->state==STATE_RESULT) {
        panel(64,8,128,24,g->result==RESULT_WIN?GREEN:BORDER,DARK);
        center(16,resultLabel(g),g->result==RESULT_WIN?YELLOW:WHITE,1);
        if(showRoundScore) {
            char score[24];
            snprintf(score,sizeof(score),"ROUND %d  %d - %d",g->duelRound,g->playerRounds,g->rivalRounds);
            panel(72,33,112,14,BORDER,DARK);
            center(37,score,CYAN,1);
        }
    }
    size_t start=firstRow*256;
    DC_FlushRange(overlayPixels+start,sizeof(overlayPixels)-start);
    if(!overlayDirty || firstRow<overlayFirstRow) overlayFirstRow=firstRow;
    overlayDirty=true;
}
