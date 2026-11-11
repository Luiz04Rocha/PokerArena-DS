#!/usr/bin/env python3
"""Draw the original opening plaza on the same pixel grid as the DS sprites."""
from pathlib import Path
import random

from PIL import Image, ImageColor, ImageDraw


SIZE = (256, 192)
GRASS = '#86b47a'
GRASS_LIT = '#a7c783'
GRASS_SHADE = '#659b77'
GRASS_DEEP = '#42766e'
PAVER = '#c7c9ad'
SEAM = '#9ea891'
INK = '#223c46'


def tree(draw, x, y, size, alternate=False):
    def shape(points, color):
        draw.polygon([(x+px*size//32, y+py*size//32) for px, py in points], fill=color)

    shape([(-16,-3),(15,-6),(24,-2),(19,3),(10,6),(-14,5),(-20,1)], GRASS_DEEP)
    shape([(-5,-21),(5,-23),(5,-6),(10,1),(2,2),(-2,-1),(-9,2),(-6,-5)], '#63544b')
    shape([(-3,-19),(0,-20),(0,-4),(-5,-1)], '#9e8057')
    for rise, width in ((0,17),(8,15),(16,12)):
        base = -13-rise
        tip = base-19
        shape([(0,tip),(-7,tip+5),(-7,tip+8),(-width+3,base-6),(-width,base-1),
               (-width+5,base+1),(-9,base+3),(-2,base+1),(5,base+3),(10,base+1),
               (width,base),(width-1,base-5),(8,tip+8),(7,tip+4)], '#204751')
        shape([(0,tip+1),(-6,tip+7),(-width+2,base-2),(-width+6,base),(0,base-1),
               (5,base+1),(width-2,base-2),(6,tip+7)], '#337e70' if alternate else '#377d63')
        shape([(0,tip+2),(-5,tip+7),(-width+5,base-3),(-8,base-2),(-3,base-4),
               (2,base-3),(6,base-5),(2,tip+7)], '#68a887' if alternate else '#79a968')
        shape([(1,tip+3),(4,tip+9),(10,base-4),(5,base-5),(3,base-8)], '#4e926d')
        shape([(-6,tip+10),(-4,tip+9),(-2,tip+10),(-6,tip+11)], '#91b67c')
        shape([(-width+4,base-3),(-width+7,base-5),(-width+8,base-3),(-width+5,base-2)],
              '#539568')


def broadleaf(draw, x, y):
    def shape(points, color):
        draw.polygon([(x+px,y+py) for px,py in points], fill=color)

    shape([(-16,0),(-10,-6),(10,-7),(23,-2),(26,2),(13,6),(-9,5)], GRASS_DEEP)
    shape([(-5,-25),(5,-25),(5,-6),(9,0),(4,1),(0,-1),(-8,2),(-7,-3)], '#5b5250')
    shape([(-4,-23),(-1,-23),(-1,-7),(-5,0),(-6,-1)], '#a2825a')
    shape([(-20,-25),(-17,-32),(-12,-35),(-8,-42),(0,-45),(7,-43),(12,-38),
           (18,-35),(21,-28),(19,-21),(13,-17),(5,-17),(0,-15),(-8,-18),
           (-14,-17),(-20,-21)], '#214950')
    shape([(-18,-27),(-15,-32),(-10,-34),(-6,-41),(1,-43),(7,-40),(12,-35),
           (17,-33),(19,-27),(15,-22),(10,-20),(4,-21),(-2,-18),(-7,-21),
           (-14,-20),(-17,-22)], '#367761')
    shape([(-15,-28),(-12,-32),(-8,-32),(-5,-38),(1,-41),(7,-39),(10,-35),
           (8,-32),(4,-31),(-1,-31),(-5,-28),(-8,-26),(-13,-26)], '#659b66')
    shape([(-5,-36),(-2,-39),(2,-40),(5,-38),(1,-36)], '#89ad6d')
    shape([(4,-29),(8,-30),(12,-32),(16,-31),(17,-28),(12,-25),(8,-26),(5,-24)],
          '#57966c')
    shape([(-13,-27),(-8,-29),(-7,-27),(-11,-26)], '#7aa86e')
    shape([(-8,-23),(-4,-24),(1,-23),(-2,-21)], '#4a8a63')


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
    # Exposed faces of the raised rock shelf.
    draw.polygon([(182,86),(194,78),(224,81),(239,90),(233,101),(221,106),
                  (185,106),(174,100)], fill=GRASS_DEEP)
    draw.polygon([(177,84),(185,75),(220,73),(233,81),(235,91),(227,101),
                  (183,101),(176,93)], fill='#355966')
    draw.polygon([(177,84),(185,75),(220,73),(233,81),(234,89),(226,94),
                  (183,95),(176,91)], fill='#788f7c')
    draw.line((180,92,183,95,225,95,232,90), fill='#a9b892')
    for x in (190,207,219):
        draw.line((x,96,x-1,99), fill='#254851')
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


def bridge(draw):
    # Deck, side face, offset shadow and rail posts for the tilted view.
    draw.polygon([(223,140),(255,137),(255,151),(226,154),(220,150)], fill='#28535e')
    draw.polygon([(221,123),(255,120),(255,147),(225,151),(220,144)], fill='#5e5349', outline=INK)
    draw.polygon([(221,123),(255,120),(255,140),(225,144)], fill='#c49c72', outline='#865f55')
    draw.line((226,145,255,141), fill='#997a65')
    draw.line((226,148,255,145), fill='#3e4245')
    for x in range(224,255,5):
        y = 123-(x-224)//10
        draw.line((x,y+1,x+3,y+17), fill='#866955')
        draw.line((x+1,y+1,x+3,y+14), fill='#d4ae7d')
    for x0,y0,x1,y1 in ((222,120,255,117),(225,141,255,137)):
        draw.line((x0,y0,x1,y1), fill='#604d46', width=4)
        draw.line((x0,y0-1,x1,y1-1), fill='#ddbc8b')
    for x,y in ((222,120),(239,118),(253,117),(225,142),(241,140),(254,138)):
        draw.rectangle((x,y-3,x+2,y+3), fill='#8d6d57', outline='#665348')
        draw.line((x,y-3,x+2,y-3), fill='#e2c19a')


def plaza_tiles(draw, randomizer):
    # The rows widen toward the camera. Keep texture beneath the cast subtle.
    rows = ((114,121,28,217),(122,132,25,220),(133,145,22,222),
            (146,160,23,223))
    for row,(top,bottom,left,right) in enumerate(rows):
        draw.line((left,bottom,right,bottom), fill='#a8b097')
        for x in range(34+row%2*9,right-4,19):
            draw.line((x,top,x,bottom), fill='#a8b097')
            draw.line((x+2,top+1,x+8,top+1), fill='#d9d9b8')
            tone = randomizer.choice(('#b7bca1','#d0cfaf','#c4c6a8'))
            draw.line((x+6,top+4,x+randomizer.randrange(8,14),top+4), fill=tone)
            if randomizer.randrange(4)==0:
                draw.point((x+10,bottom-2), fill='#9fac92')
    draw.line((25,160,32,160,36,161,54,161), fill='#8eaa84')
    draw.line((199,160,211,160,215,158,222,158), fill='#8eaa84')


def main():
    randomizer = random.Random(7215)
    scene = Image.new('RGB', SIZE, GRASS)
    draw = ImageDraw.Draw(scene)

    # Broad canopy shadows frame the bright walkway.
    draw.polygon([(0,0),(256,0),(256,31),(241,37),(213,30),(182,40),(151,46),
                  (134,43),(108,57),(78,62),(62,57),(48,74),(19,85),(0,84)], fill=GRASS_SHADE)
    draw.polygon([(0,166),(23,168),(42,174),(67,175),(79,187),(96,192),(0,192)], fill=GRASS_SHADE)
    draw.polygon([(159,192),(174,180),(204,175),(211,165),(256,159),(256,192)], fill=GRASS_SHADE)
    shade_rgb = ImageColor.getrgb(GRASS_SHADE)
    for _ in range(540):
        x, y = randomizer.randrange(256), randomizer.randrange(192)
        shades = ('#77a77a','#578a73') if scene.getpixel((x,y))==shade_rgb else (GRASS_LIT,GRASS_SHADE,'#93bb7d')
        shade = randomizer.choice(shades)
        draw.line((x, y, x+randomizer.randrange(2,6), y), fill=shade)
        if randomizer.randrange(3)==0:
            draw.point((x+2,y-1), fill=shade)

    creek = [(246,0),(255,0),(255,191),(220,191),(226,171),(239,151),(243,132),
             (239,114),(235,98),(237,81),(245,66),(249,48),(242,29)]
    draw.polygon(creek, fill='#438e9d', outline='#396e70', width=3)
    draw.line((246,30,253,54,244,78,238,98,243,117,248,133,241,152,229,173,224,190),
              fill='#5eb0ae', width=2)
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
    draw.polygon([(13,156),(22,165),(223,165),(232,157),(232,160),(223,168),(22,168),(13,159)],
                 fill='#7e9288')
    draw.polygon(plaza, fill=SEAM)
    inner = [(30,112),(213,112),(221,118),(227,155),(221,162),(24,162),(18,155),(24,120)]
    draw.polygon(inner, fill=PAVER)
    draw.line((29,110,214,110,223,117), fill='#e4dfbf', width=2)
    plaza_tiles(draw, randomizer)
    for y in (170,180,190):
        draw.line((102,y,145,y), fill=SEAM)
        x = 122 if y==180 else 116
        draw.line((x,y-6,x,y), fill=SEAM)

    bridge(draw)

    for x,y in ((14,107),(35,102),(144,103),(158,104),(17,174),(181,175),(218,172),(88,178),(157,184)):
        draw.line((x-2,y+2,x+3,y+2), fill=GRASS_DEEP)
        draw.line((x,y+2,x-2,y-2), fill=GRASS_SHADE)
        draw.line((x,y+2,x+2,y-2), fill=GRASS_SHADE)
        draw.point((x+2,y-3), fill='#f2de9b')
        draw.point((x-2,y-3), fill='#ece4cb')

    for i,(x,y,size) in enumerate(((2,26,31),(30,23,30),(61,24,30),(88,21,29),(116,22,28),
                                    (143,27,30),(162,23,24),(247,36,29),
                                    (35,55,32),(150,62,33),(10,96,35),(35,92,28),
                                    (248,84,28),(248,107,24))):
        tree(draw,x,y,size,bool(i%2))
    broadleaf(draw,11,61)
    broadleaf(draw,206,48)
    spring_grove(draw)

    # Low walls along the back of the path have readable front faces.
    for x in (33,46,59,72,143,156,169):
        draw.polygon([(x,101),(x+12,101),(x+13,108),(x+1,109)], fill=GRASS_DEEP)
        draw.polygon([(x,100),(x+10,98),(x+12,101),(x+12,106),(x+2,107),(x,105)],
                     fill='#68857c', outline='#4c756e')
        draw.polygon([(x,100),(x+10,98),(x+12,101),(x+2,103)], fill='#b9bd9d')
        draw.line((x+1,100,x+9,99), fill='#e0d6a8')
    for i,(x,y,size) in enumerate(((2,191,25),(54,195,28),(212,198,27),(251,194,31))):
        tree(draw,x,y,size,bool(i%2))
    broadleaf(draw,28,215)

    target = Path(__file__).resolve().parents[1] / 'assets/title-route.png'
    scene.save(target, optimize=True)


if __name__ == '__main__':
    main()
