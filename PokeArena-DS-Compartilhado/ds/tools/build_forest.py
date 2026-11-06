"""Original indexed pixel canopy for native forest billboards."""
from pathlib import Path
import random
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2]
rng=random.Random(513)
im=Image.new('P',(64,64));d=ImageDraw.Draw(im)
pal=[(0,0,0),(26,65,55),(33,88,68),(36,111,78),(39,135,83),(61,158,91),(91,181,105),(132,200,121),(88,63,40),(127,89,50),(165,121,65),(52,100,70),(114,170,100),(164,210,131),(215,228,163),(66,131,88)]
im.putpalette([v for c in pal for v in c]+[0]*720)
d.polygon([(25,42),(38,42),(38,57),(44,61),(34,59),(29,62),(28,58),(21,60),(26,54)],fill=8)
d.rectangle((29,43,34,57),fill=9);d.line((30,44,30,57),fill=10,width=2)
for cx,cy,rx,ry in [(22,39,19,12),(42,37,18,12),(17,27,14,12),(37,24,21,17),(29,14,14,11)]:
    d.ellipse((cx-rx,cy-ry,cx+rx,cy+ry),fill=1)
    d.ellipse((cx-rx+2,cy-ry+1,cx+rx-1,cy+ry-3),fill=3)
    d.ellipse((cx-rx+3,cy-ry+1,cx+rx-4,cy+ry-7),fill=4)
    d.ellipse((cx-rx+5,cy-ry+2,cx+rx-7,cy+ry-9),fill=5)
for i in range(220):
    x=rng.randrange(5,58);y=rng.randrange(5,47)
    if im.getpixel((x,y)) in (3,4,5):
        c=rng.choice((4,5,6,7));d.line((x,y,x+2,y-1),fill=c,width=1)
indices=list(im.get_flattened_data())
words=[sum(indices[i+k]<<(k*4) for k in range(4)) for i in range(0,4096,4)]
cols=[(a>>3)|((b>>3)<<5)|((c>>3)<<10) for a,b,c in pal]
text='#include <stdint.h>\nconst uint16_t forest_palette[16]={'+','.join(hex(c) for c in cols)+'};\nconst uint16_t forest_tree[1024] __attribute__((aligned(4)))={'+','.join(hex(w) for w in words)+'};\n'
(ROOT/'ds/source/generated_forest.c').write_text(text)
out=ROOT/'docs/capturas-v0.9';out.mkdir(exist_ok=True)
im.convert('RGB').resize((256,256),Image.Resampling.NEAREST).save(out/'arvore-original.png')
