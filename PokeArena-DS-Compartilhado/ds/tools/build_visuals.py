#!/usr/bin/env python3
"""Generate native indexed animation textures, a small HUD font and ground tiles.

Effects are deterministic pixel drawings authored here, not web assets.
The Pokemon animation sheets are handled separately by build_sprites.py.
"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[2]
NAMES=('brasas','pirotecnia','nitrocarga','lanca-chamas','inferno',
       'bolha','onda','jato','hidrobomba','turbilhao',
       'chicote','folhas','investida','petalas','raio-solar',
       'choque','descarga','velocidade','trovao','tempestade','impacto','evolucao')
COLORS=((91,22,28),(213,48,24),(255,116,25),(255,204,62),(255,250,197))
PALETTES=[COLORS]*5+[((12,51,101),(24,113,191),(52,191,237),(148,235,255),(242,255,255))]*5+[
    ((22,62,39),(48,118,49),(96,183,62),(186,233,104),(251,255,182))]*5+[
    ((105,64,29),(218,130,24),(255,210,33),(255,244,118),(255,255,234))]*5+[
    ((103,46,56),(229,110,66),(255,192,84),(255,244,193),(255,255,255)),
    ((44,43,115),(97,101,214),(113,205,255),(227,240,255),(255,255,255))]

def effect(style,frame):
    im=Image.new('P',(32,32)); d=ImageDraw.Draw(im); phase=frame/6*math.tau
    def line(points,color=4,width=2): d.line(points,fill=color,width=width)
    def dot(x,y,r=1,c=4): d.ellipse((int(x-r),int(y-r),int(x+r),int(y+r)),fill=c)
    def ring(r,c=3): d.ellipse((16-r,16-r*.6,16+r,16+r*.6),outline=c,width=2)
    def sparks(count=6,r=12):
        for i in range(count):
            a=phase+i*math.tau/count; x=16+math.cos(a)*r; y=16+math.sin(a)*r
            line([(int(x),int(y)),(int(x+math.cos(a)*3),int(y+math.sin(a)*3))],4,1)
    def leaf(x,y,angle):
        dx,dy=math.cos(angle)*5,math.sin(angle)*5
        d.polygon([(int(x-dx),int(y-dy)),(int(x-dy*.5),int(y+dx*.5)),(int(x+dx),int(y+dy)),(int(x+dy*.5),int(y-dx*.5))],fill=3,outline=1)
        line([(int(x-dx*.6),int(y-dy*.6)),(int(x+dx*.6),int(y+dy*.6))],5,1)
    if style in (0,2,3,4):
        for j in range(3 if style!=4 else 5):
            x=16+(j-1)*3 if style!=4 else 8+j*4
            tip=2+(frame+j)%3; base=24+(j+frame)%5
            d.polygon([(x-4,base-3),(x-3,12),(x+((frame+j)%3-1)*2,tip),(x+3,12),(x+5,base)],fill=2)
            d.polygon([(x-2,base-2),(x,9+(j+frame)%5),(x+3,base)],fill=4)
        if style==2: line([(8,30),(12,22),(14,15)],3,2)
        if style==3: line([(15,31),(12,22),(18,15),(15,5)],5,3)
        if style==4: ring(10+frame%3,3); sparks(8,12)
    elif style==1:
        ring(3+frame*2,3)
        for j in range(7):
            a=j*math.tau/7; r=4+frame*1.8; x=16+math.cos(a)*r; y=17+math.sin(a)*r*.6
            d.polygon([(int(x-3),int(y+4)),(int(x),int(y-6-frame%3)),(int(x+3),int(y+4))],fill=2)
            dot(x,y,1,4)
    elif style==5:
        d.polygon([(16,3),(9,15),(9,22),(14,27),(20,26),(24,21),(22,14)],fill=2,outline=1)
        d.ellipse((11,13,22,25),fill=3); d.ellipse((12,14,16,19),fill=5)
        dot(23,29-frame,1,4); dot(7,25-frame,1,3)
    elif style in (6,9):
        ring(3+frame*2,3); ring(6+frame,2)
        for j in range(6):
            a=j*math.tau/6+phase; x=16+math.cos(a)*(5+frame); y=16+math.sin(a)*(4+frame*.6)
            line([(int(x),int(y)),(int(x+math.cos(a)*4),int(y-4))],4,2)
        if style==9:
            for t in range(20):
                a=t*.55+phase; r=t*.6
                dot(16+math.cos(a)*r,16+math.sin(a)*r*.6,1,3+t%2)
    elif style in (7,8):
        for j in range(3):
            x=10+j*6
            line([(x,31),(x+int(math.sin(phase+j)*2),20),(x,5+j*2)],2+j%2,4 if style==8 else 2)
            line([(x,24),(x,8+j*2)],5,1)
        d.ellipse((10,2,22,14),fill=3); dot(14,5,2,5)
    elif style==10:
        pts=[]
        for t in range(12):
            a=-2.7+t*.25+frame*.2; pts.append((int(16+math.cos(a)*12),int(17+math.sin(a)*11)))
        line(pts,1,4); line(pts,3,2); leaf(*pts[-1],phase)
    elif style==11:
        leaf(16,15,phase); leaf(8,25,phase+.7); leaf(24,24,phase-.7)
    elif style in (12,17):
        for j in range(5):
            x=5+j*5; line([(x,29),(x+2,21),(x+1,10+(j+frame)%4)],3,1)
        if style==12:
            for j in range(4): dot(6+j*6,27+(j+frame)%3,2,2)
        else: line([(19,30),(12,20),(21,14),(14,2)],5,2)
    elif style==13:
        ring(5+frame,2)
        for j in range(8):
            a=j*math.tau/8+phase; x=16+math.cos(a)*11; y=16+math.sin(a)*9
            d.ellipse((int(x-2),int(y-3),int(x+2),int(y+3)),fill=3+j%2)
            dot(x,y,0,5)
    elif style==14:
        line([(16,31),(16,1)],1,13); line([(16,31),(16,1)],3,9); line([(16,31),(16,1)],5,3+frame%2)
        sparks(6,12)
    elif style in (15,18):
        pts=[(16,1),(10+(frame%2)*3,10),(22,11),(12,22),(18,22),(13,31)]
        line(pts,2,5); line(pts,4,3); line(pts,5,1)
        dot(5+frame*4,7+(frame*7)%19,1,4)
        if style==18: line([(17,14),(27,15),(21,24)],4,2); line([(13,9),(5,15),(11,22)],4,2)
    elif style in (16,19):
        ring(4+frame*2,3)
        for j in range(6 if style==16 else 9):
            a=j*math.tau/(6 if style==16 else 9)+phase*.3
            pts=[]
            for t in (3,7,11,15):
                q=a+(.14 if (t+frame)%2 else -.14); pts.append((int(16+math.cos(q)*t),int(16+math.sin(q)*t*.8)))
            line(pts,2,3); line(pts,5,1)
    elif style==20:
        r=3+frame*2
        d.polygon([(16,16-r),(18,13),(16+r,16),(19,18),(16,16+r),(13,19),(16-r,16),(13,14)],fill=4)
        dot(16,16,max(1,4-frame//2),5); sparks(5,r)
    else:
        ring(5+frame*2,3); sparks(8,7+frame)
        for j in range(3):
            x=8+j*8; y=27-(frame*4+j*7)%24
            line([(x,y-2),(x,y+2)],5,1); line([(x-2,y),(x+2,y)],5,1)
    im.putpalette([0,0,0]+[v for rgb in PALETTES[style] for v in rgb]+[0]*750)
    return im

def main():
    c=['/* Generated by tools/build_visuals.py; original procedural effects. */','#include "generated_visuals.h"']
    def array(decl,words):
        c.append(decl+' __attribute__((aligned(4))) = {')
        for i in range(0,len(words),16): c.append('    '+','.join(f'0x{v:04x}' for v in words[i:i+16])+',')
        c.append('};')
    definitions=[]; previews=[]
    for style,name in enumerate(NAMES):
        frames=[effect(style,f) for f in range(6)]
        assert len({f.tobytes() for f in frames})>=3,name
        for heading in range(8):
            for frame,im in enumerate(frames):
                rotated=im.rotate(180+heading*45,resample=Image.Resampling.NEAREST)
                p=list(rotated.get_flattened_data()); assert max(p)<=5
                symbol=f'fx_{style}_{heading}_{frame}'
                array('static const uint16_t '+symbol+'[256]',[sum(p[i+k]<<(k*4) for k in range(4)) for i in range(0,1024,4)])
        palette=[0]+[(r>>3)|((g>>3)<<5)|((b>>3)<<10) for r,g,b in PALETTES[style]]+[0]*10
        definitions.append('{{'+','.join(f'0x{v:04x}' for v in palette)+'},{'+','.join('{'+','.join(f'fx_{style}_{d}_{f}' for f in range(6))+'}' for d in range(8))+'}}')
        previews.append(frames)
    c.append('const NativeEffect nativeEffects[FX_COUNT] = {\n'+',\n'.join(definitions)+'\n};')
    # Same received grass/dirt materials, reduced to native 64x64 RGB555 textures.
    for name in ('grass','dirt'):
        im=Image.open(ROOT/f'reference/pokearena-original/public/tex/{name}.jpg').convert('RGB').resize((64,64))
        array(f'const uint16_t native_{name}[4096]',[0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10) for r,g,b in im.get_flattened_data()])
    font_path=Path('C:/Windows/Fonts/consola.ttf')
    if not font_path.exists(): font_path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf')
    font=ImageFont.truetype(str(font_path),10)
    c.append('const uint8_t hudFont[96][8] = {')
    for code in range(32,128):
        im=Image.new('1',(6,8)); ImageDraw.Draw(im).text((0,6),chr(code),font=font,fill=1,anchor='ls')
        rows=[sum(im.getpixel((x,y))<<x for x in range(6)) for y in range(8)]
        c.append('    {'+','.join(map(str,rows))+'},')
    c.append('};')
    (ROOT/'ds/source/generated_visuals.c').write_text('\n'.join(c)+'\n',encoding='utf-8')
    (ROOT/'ds/include/generated_visuals.h').write_text('''#ifndef GENERATED_VISUALS_H
#define GENERATED_VISUALS_H
#include <stdint.h>
#include "visuals.h"
typedef struct { uint16_t palette[16]; const uint16_t *frames[8][6]; } NativeEffect;
extern const NativeEffect nativeEffects[FX_COUNT];
extern const uint16_t native_grass[4096],native_dirt[4096];
extern const uint8_t hudFont[96][8];
#endif
''')
    out=ROOT/'docs/capturas-v0.5'; out.mkdir(exist_ok=True)
    font=ImageFont.truetype(str(font_path),12)
    sheets=[]
    for frame in range(6):
        sheet=Image.new('RGB',(560,352),(15,22,38)); d=ImageDraw.Draw(sheet)
        for style,name in enumerate(NAMES):
            x=(style%6)*92; y=(style//6)*88
            p=previews[style][frame].convert('RGBA'); p.putalpha(Image.frombytes('L',(32,32),previews[style][frame].tobytes()).point(lambda v:255 if v else 0))
            sheet.paste(p.resize((64,64),Image.Resampling.NEAREST),(x+14,y),p.resize((64,64),Image.Resampling.NEAREST))
            d.text((x+2,y+65),name,font=font,fill=(218,232,252))
        sheets.append(sheet)
    sheets[2].save(out/'efeitos-originais.png')
    sheets[0].save(out/'efeitos-originais.gif',save_all=True,append_images=sheets[1:],duration=100,loop=0,disposal=2)
    print('OK: 22 effects, six frames, eight directions, native font and ground textures')

if __name__=='__main__': main()
