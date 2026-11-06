#!/usr/bin/env python3
"""Draw the original opening plaza on the same pixel grid as the DS sprites."""
from pathlib import Path
import random

from PIL import Image, ImageDraw


SIZE = (256, 192)
GRASS = '#85b878'
GRASS_LIT = '#a3cd82'
GRASS_SHADE = '#5d9b72'
GRASS_DEEP = '#457d69'
PAVER = '#c6c9ad'
SEAM = '#a8b098'
INK = '#233d43'


def tree(draw, x, y, size, alternate=False):
    def shape(points, color):
        draw.polygon([(x+px*size//32, y+py*size//32) for px, py in points], fill=color)

    shape([(-16,-3),(15,-6),(22,-1),(14,5),(-14,5),(-20,1)], GRASS_DEEP)
    shape([(-5,-21),(5,-23),(5,-6),(10,1),(2,2),(-2,-1),(-9,2),(-6,-5)], '#63544b')
    shape([(-3,-19),(0,-20),(0,-4),(-5,-1)], '#9e8057')
    for rise, width in ((0,17),(8,15),(16,12)):
        base = -13-rise
        tip = base-19
        shape([(0,tip),(-7,tip+5),(-7,tip+8),(-width+3,base-6),(-width,base-1),
               (-width+5,base+1),(-9,base+3),(-2,base+1),(5,base+3),(10,base+1),
               (width,base),(width-1,base-5),(8,tip+8),(7,tip+4)], '#214e50')
        shape([(0,tip+1),(-6,tip+7),(-width+2,base-2),(-width+6,base),(0,base-1),
               (5,base+1),(width-2,base-2),(6,tip+7)], '#337963' if alternate else '#317b59')
        shape([(0,tip+2),(-5,tip+7),(-width+5,base-3),(-8,base-2),(-3,base-4),
               (2,base-3),(6,base-5),(2,tip+7)], '#73b479' if alternate else '#6eac62')
        shape([(1,tip+3),(4,tip+9),(10,base-4),(5,base-5),(3,base-8)], '#4c9567')


def grove_shrub(draw, x, y):
    def shape(points, color):
        draw.polygon([(x+px,y+py) for px,py in points], fill=color)

    shape([(-9,2),(-6,-5),(-2,-5),(0,-8),(6,-7),(8,-3),(11,1),(8,4),(-6,4)], GRASS_DEEP)
    shape([(-7,1),(-5,-3),(-1,-4),(1,-7),(5,-6),(6,-2),(9,0),(5,2),(-5,2)], '#337963')
    shape([(-5,-2),(-2,-3),(1,-6),(4,-5),(3,-3),(0,-2),(-4,0)], '#6eac62')
    draw.line((x-6,y+1,x-3,y), fill='#73b479')
    draw.line((x+3,y-2,x+6,y-1), fill='#73b479')
    draw.point((x+1,y-5), fill='#f2de9b')


def spring_grove(draw):
    grove_shrub(draw,205,54)
    grove_shrub(draw,229,59)
    draw.polygon([(187,84),(192,69),(208,67),(227,71),(234,77),(242,77),(242,83),
                  (234,84),(228,94),(217,97),(197,94)], fill=GRASS_DEEP)
    draw.polygon([(193,82),(202,74),(219,74),(231,78),(243,78),(243,81),(232,82),
                  (226,92),(211,94),(198,91)], fill='#376775')
    draw.polygon([(197,82),(204,77),(218,77),(231,80),(241,80),(230,83),
                  (225,89),(214,92),(201,88)], fill='#4e9caa')
    draw.line((204,89,212,91,222,89,226,87), fill='#79bcc1')
    draw.line((231,80,240,80,244,78), fill='#9acdd0')

    # Faceted stones frame the spring. Their dark front faces sit in the pool.
    draw.polygon([(194,69),(197,62),(207,59),(214,64),(214,74),(209,81),(199,81),(193,76)],
                 fill='#637e82', outline=INK)
    draw.polygon([(195,70),(201,72),(209,70),(213,67),(213,74),(207,79),(199,79),(195,76)],
                 fill='#446372')
    draw.polygon([(196,67),(200,63),(207,61),(212,65),(208,69),(202,70)], fill='#9fae9c')
    draw.line((196,67,200,65,204,65), fill='#6eac62', width=2)
    draw.line((199,65,203,63,207,63), fill='#8bbb72')
    draw.line((208,67,211,66,212,69), fill='#768f80')
    draw.polygon([(219,71),(222,65),(229,64),(233,68),(234,74),(231,80),(223,81),(219,77)],
                 fill='#637e82', outline=INK)
    draw.polygon([(220,70),(224,66),(229,66),(232,69),(229,73),(224,74)], fill='#9fae9c')
    draw.line((223,68,226,67,230,68), fill='#6eac62', width=2)
    draw.line((230,74,232,76,229,79), fill='#446372', width=2)

    draw.polygon([(213,72),(218,71),(219,78),(222,82),(218,86),(210,86),(208,83),(212,79)],
                 fill='#79bcc1')
    draw.line((214,73,214,79,211,83,216,84,219,82), fill='#badcd1')
    draw.line((216,74,217,79,219,81), fill='#e5e8c6')
    draw.line((209,84,212,86,215,86), fill='#badcd1')
    draw.point((218,87), fill='#e5e8c6')

    grove_shrub(draw,193,87)
    grove_shrub(draw,231,94)
    draw.ellipse((191,96,204,102), fill=GRASS_DEEP)
    draw.rectangle((196,88,199,99), fill='#637e82', outline=INK)
    draw.line((197,90,197,97), fill='#9fae9c')
    draw.polygon([(193,85),(195,82),(201,82),(203,86),(201,90),(194,90)], fill='#9fae9c', outline=INK)
    draw.rectangle((196,85,200,87), fill='#ead18a')
    draw.point((197,85), fill='#faf0b4')


def main():
    randomizer = random.Random(7215)
    scene = Image.new('RGB', SIZE, GRASS)
    draw = ImageDraw.Draw(scene)

    # Large grass clusters make the terrain read at one screen pixel per pixel.
    for _ in range(660):
        x, y = randomizer.randrange(256), randomizer.randrange(192)
        shade = randomizer.choice((GRASS_LIT, GRASS_SHADE, '#91be7b'))
        draw.line((x, y, x+randomizer.randrange(2,8), y), fill=shade)
        if randomizer.randrange(3)==0:
            draw.point((x+2,y-1), fill=shade)

    creek = [(246,0),(255,0),(255,191),(220,191),(226,171),(239,151),(243,132),
             (239,114),(235,98),(237,81),(245,66),(249,48),(242,29)]
    draw.polygon(creek, fill='#4e9caa', outline='#4f877d', width=3)
    draw.line((251,30,253,55,248,78,243,96,248,121,248,144,238,164,232,183,228,191),
              fill='#79bcc1', width=2)
    for y in range(33,192,9):
        x = 247 if y<150 else 236-(y-152)//4
        draw.line((x,y,x+randomizer.randrange(3,10),y), fill='#9acdd0')

    draw.polygon([(0,126),(33,124),(36,150),(0,151)], fill=SEAM)
    draw.polygon([(0,130),(33,128),(33,147),(0,147)], fill=PAVER)
    draw.line((0,137,26,136), fill=SEAM)
    draw.line((9,129,9,137,17,137,17,147), fill=SEAM)

    # The branch connects the spring's overlook, the plaza and the near edge.
    draw.polygon([(209,94),(226,94),(222,104),(211,119),(192,119),(205,102)], fill=SEAM)
    draw.polygon([(212,96),(222,96),(219,104),(209,116),(197,117),(208,103)], fill=PAVER)
    draw.line((205,108,215,108), fill=SEAM)
    draw.polygon([(100,152),(148,151),(154,191),(88,191)], fill=SEAM)
    draw.polygon([(105,152),(145,152),(149,191),(92,191)], fill=PAVER)
    plaza = [(29,108),(214,108),(225,115),(232,157),(223,166),(22,166),(13,156),(20,118)]
    draw.polygon([(x,y+3) for x,y in plaza], fill=GRASS_DEEP)
    draw.polygon(plaza, fill=SEAM)
    inner = [(30,112),(213,112),(221,118),(227,155),(221,162),(24,162),(18,155),(24,120)]
    draw.polygon(inner, fill=PAVER)
    draw.line((29,110,214,110,223,117), fill='#e4dfbf', width=2)
    for row, y in enumerate((122,134,147,158)):
        draw.line((26,y,219,y), fill='#b1b8a0')
        for x in range(34+row%2*9,217,19):
            draw.line((x,y-9,x,y), fill='#b1b8a0')
            draw.line((x+2,y-8,x+7,y-8), fill='#d8d8b9')
    for y in (170,180,190):
        draw.line((102,y,145,y), fill=SEAM)
        x = 122 if y==180 else 116
        draw.line((x,y-6,x,y), fill=SEAM)

    draw.polygon([(221,125),(255,125),(255,148),(224,150)], fill='#67504a', outline=INK)
    draw.polygon([(221,124),(255,124),(255,142),(224,144)], fill='#c6a177')
    for x in range(225,256,5):
        draw.line((x,125,x+2,143), fill='#997b63')
    for x in (228,251):
        draw.rectangle((x,122,x+2,128), fill='#997b63', outline='#705750')
        draw.rectangle((x+1,140,x+3,146), fill='#997b63', outline='#705750')
    draw.line((226,123,255,122), fill='#e0bc8a', width=2)
    draw.line((227,141,255,140), fill='#e0bc8a', width=2)

    for x,y in ((14,107),(35,102),(144,103),(158,104),(17,174),(181,175),(218,172),(88,178),(157,184)):
        draw.line((x-2,y+2,x+3,y+2), fill=GRASS_DEEP)
        draw.line((x,y+2,x-2,y-2), fill=GRASS_SHADE)
        draw.line((x,y+2,x+2,y-2), fill=GRASS_SHADE)
        draw.point((x+2,y-3), fill='#f2de9b')
        draw.point((x-2,y-3), fill='#ece4cb')

    for i,(x,y,size) in enumerate(((2,26,31),(30,23,30),(61,24,30),(88,21,29),(116,22,28),
                                    (143,27,30),(162,23,24),(247,36,29),(11,60,37),
                                    (35,55,32),(150,62,33),(10,96,35),(35,92,28),
                                    (248,84,28),(248,107,24))):
        tree(draw,x,y,size,bool(i%2))
    spring_grove(draw)

    # Low walls along the back of the path have readable front faces.
    for x in (33,46,59,72,143,156,169):
        draw.rectangle((x,99,x+11,106), fill='#83988c', outline='#4c756e')
        draw.rectangle((x+1,98,x+10,101), fill='#c1c5a2')
        draw.line((x+2,99,x+9,99), fill='#e0d6a8')
    for i,(x,y,size) in enumerate(((2,191,25),(27,199,30),(54,195,28),(212,198,27),(251,194,31))):
        tree(draw,x,y,size,bool(i%2))

    target = Path(__file__).resolve().parents[1] / 'assets/title-route.png'
    scene.save(target, optimize=True)


if __name__ == '__main__':
    main()
