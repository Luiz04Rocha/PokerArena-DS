#!/usr/bin/env python3
"""Build native tile arrays from the supplied PMD sheets.

Keeps all frames, eight directions and source frame timing (60 Hz).
No external graphics files or filesystem access are needed by the ROM.
"""
from pathlib import Path
import argparse
import json
from xml.etree import ElementTree
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SPECIES = (4,7,1,25,5,8,2,26,6,9,3)  # Same order as starters[] followed by their evolved forms.
CORE_ANIMS = ('Idle','Walk','Attack','Hurt')
ANIMS = (*CORE_ANIMS,'Celebrate','Defeated')
RESULT_CLIPS = {'Celebrate': ('Hop','Idle'), 'Defeated': ('Faint','Sleep','Hurt')}
DIRS = tuple(range(8))

def sprite_source(root, dex):
    ident = f'{dex:04d}'
    if not (root/'sprite').is_dir():
        folder = root/ident
        metadata = json.loads((folder/'anims.json').read_text())
        for kind, candidates in RESULT_CLIPS.items():
            metadata[kind] = next(metadata[name] for name in candidates
                                  if name in metadata and (folder/metadata[name]['file']).is_file())
        return folder, metadata, folder/'portrait.png'

    folder = root/'sprite'/ident
    clips = {clip.findtext('Name'): clip for clip in
             ElementTree.parse(folder/'AnimData.xml').findall('Anims/Anim')}
    metadata = {}
    for kind in ANIMS:
        name, visited = kind, set()
        if kind in RESULT_CLIPS:
            name = next(candidate for candidate in RESULT_CLIPS[kind]
                        if candidate in clips and (folder/f'{candidate}-Anim.png').is_file())
        while True:
            if name in visited or name not in clips:
                raise ValueError(f'Invalid animation alias: {ident}/{kind} -> {name}')
            visited.add(name)
            clip = clips[name]
            alias = clip.findtext('CopyOf')
            if not alias:
                break
            name = alias
        metadata[kind] = {
            'file': f'{name}-Anim.png',
            'shadow': f'{name}-Shadow.png',
            'offsets': f'{name}-Offsets.png',
            'frameWidth': int(clip.findtext('FrameWidth')),
            'frameHeight': int(clip.findtext('FrameHeight')),
            'durations': [int(item.text) for item in clip.findall('Durations/Duration')],
            'markers': [int(clip.findtext(tag, '0')) for tag in ('RushFrame','HitFrame','ReturnFrame')]
        }
    return folder, metadata, root/'portrait'/ident/'Normal.png'

def frame_canvas(image,size):
    ratio=min(1,size/max(image.size))
    image=image.resize((max(1,round(image.width*ratio)),max(1,round(image.height*ratio))),Image.Resampling.NEAREST)
    canvas=Image.new('RGBA',(size,size)); canvas.paste(image,((size-image.width)//2,size-image.height))
    return canvas

def flame_point(image,size):
    # Bright yellow is the flame core in the supplied fire-family sheets.
    # Measure before palette quantization, using the exact texture placement.
    canvas=frame_canvas(image,size)
    points=[(x,y) for y in range(size) for x in range(size)
            if (lambda c:c[3]>=128 and c[0]>=245 and c[1]>=235 and c[2]<80)(canvas.getpixel((x,y)))]
    if not points:return [0,255]  # Flame occluded in this authored frame.
    return [min(254,round((sum(p[i] for p in points)/len(points)+.5)*256/size)) for i in (0,1)]

def marker(image, box, color):
    tile = image.crop(box)
    for index, pixel in enumerate(tile.get_flattened_data()):
        if pixel == color:
            return index % tile.width, index // tile.width
    return None

def pixels(image, palette, size=32, *, tiled=False):
    canvas=frame_canvas(image,size)
    out=[]
    for r,g,b,a in canvas.get_flattened_data():
        out.append(0 if a<128 else 1+min(range(len(palette)),key=lambda i:sum((palette[i][j]-v)**2 for j,v in enumerate((r,g,b)))))
    if not tiled:
        return [sum(out[pos+i]<<(4*i) for i in range(4)) for pos in range(0,len(out),4)]
    # Portraits retain the tile order used by the HUD.
    words=[]
    for ty in range(size//8):
        for tx in range(size//8):
            for y in range(8):
                for x in (0,4):
                    pos=(ty*8+y)*size+tx*8+x
                    words.append(sum(out[pos+i]<<(4*i) for i in range(4)))
    return words

def pack_frame(words):
    """Encode runs of transparent words separately from literal pixel words."""
    packed = []
    i = 0
    while i < len(words):
        start = i
        if words[i] == 0 and i+1 < len(words) and words[i+1] == 0:
            while i < len(words) and words[i] == 0:
                i += 1
            packed.append(0x8000 | (i-start))
        else:
            i += 1
            while i < len(words) and not (words[i] == 0 and i+1 < len(words) and words[i+1] == 0):
                i += 1
            packed.extend((i-start,*words[start:i]))
    return packed

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'reference/spritecollab-f92cb9f',
                        help='Original PMD folder or a SpriteCollab repository folder with sprite/ and portrait/')
    parser.add_argument('--destination',type=Path,default=ROOT/'ds',
                        help='Directory for the generated source/ and include/ files')
    args=parser.parse_args()
    out=args.destination/'source/generated_sprites.c'
    header=args.destination/'include/generated_sprites.h'
    lines=['/* Generated from supplied PMD sprites. See docs/CREDITOS.md. */','#include "generated_sprites.h"']
    definitions=[]; frames_total=packed_words=0
    def array(name,values):
        lines.append(f'static const uint16_t {name}[] __attribute__((aligned(4))) = {{')
        for n in range(0,len(values),16): lines.append('    '+','.join(f'0x{v:04x}' for v in values[n:n+16])+',')
        lines.append('};')
    for dex in SPECIES:
        folder,meta,portrait_path=sprite_source(args.source,dex)
        sheets={name:Image.open(folder/meta[name]['file']).convert('RGBA') for name in ANIMS}
        portrait=Image.open(portrait_path).convert('RGBA')
        opaque=[(r,g,b) for im in [*(sheets[name] for name in CORE_ANIMS),portrait]
                for r,g,b,a in im.get_flattened_data() if a>=128]
        sample=Image.new('RGB',(len(opaque),1)); sample.putdata(opaque)
        q=sample.quantize(colors=15,method=Image.Quantize.MEDIANCUT)
        palette=[tuple(q.getpalette()[i:i+3]) for i in range(0,45,3)]
        rgb555=[0]+[(r>>3)|((g>>3)<<5)|((b>>3)<<10) for r,g,b in palette]
        animations=[]
        for name in ANIMS:
            m=meta[name]; sheet=sheets[name]; fw,fh=m['frameWidth'],m['frameHeight']
            count=len(m['durations'])
            rows=sheet.height//fh
            if count>16 or sheet.width!=fw*count or sheet.height%fh or rows not in (1,8):
                raise ValueError(f'Invalid grid: {dex}/{name}')
            words=[];flame=[];anchors=[];starts=[]
            shadow_path=folder/m.get('shadow','')
            shadow=Image.open(shadow_path).convert('RGBA') if shadow_path.is_file() else None
            offsets_path=folder/m.get('offsets','')
            offsets=Image.open(offsets_path).convert('RGBA') if offsets_path.is_file() else None
            for layer in (shadow,offsets):
                if layer and layer.size!=sheet.size:
                    raise ValueError(f'Invalid marker grid: {dex}/{name}')
            marks=m.get('markers',[0,0,0])
            if any(not 0<=value<count for value in marks):
                raise ValueError(f'Invalid action frame: {dex}/{name}')
            times=[sum(m['durations'][:value]) for value in marks]
            texture_size=64
            for direction in DIRS:
                row=direction if rows==8 else 0
                for frame in range(count):
                    box=(frame*fw,row*fh,(frame+1)*fw,(row+1)*fh)
                    image=sheet.crop(box)
                    if not image.getchannel('A').getbbox():
                        raise ValueError(f'Empty frame: {dex}/{name} direction {direction} frame {frame}')
                    starts.append(len(words))
                    words.extend(pack_frame(pixels(image,palette,texture_size)))
                    if shadow:
                        ground=marker(shadow,box,(255,255,255,255))
                        if ground is None:
                            raise ValueError(f'Missing ground marker: {dex}/{name}/{direction}/{frame}')
                        head=marker(offsets,box,(0,0,0,255)) if offsets else None
                        head=head or (marker(offsets,box,(255,255,255,255)) if offsets else None)
                        head=head or (ground[0],ground[1]-11)
                        anchors.extend((ground[0]-fw//2,fh-ground[1],head[0]-ground[0],ground[1]-head[1]))
                    if dex in (4,5,6):flame.extend(flame_point(image,texture_size))
                    frames_total+=1
            symbol=f's{dex}_{name.lower()}'; array(symbol,words)
            starts.append(len(words))
            if starts[-1]>65535:
                raise ValueError(f'Animation exceeds native frame offsets: {dex}/{name}')
            array(symbol+'_frames',starts)
            packed_words+=len(words)
            flame_symbol=symbol+'_flame' if flame else '0'
            if flame:lines.append(f'static const uint8_t {flame_symbol}[] = {{'+','.join(map(str,flame))+'};')
            anchor_symbol=symbol+'_anchors' if anchors else '0'
            if anchors:
                if any(not -128<=value<=127 for value in anchors):
                    raise ValueError(f'Anchor outside native range: {dex}/{name}')
                lines.append(f'static const int8_t {anchor_symbol}[] = {{'+','.join(map(str,anchors))+'};')
            feet=[]
            for direction in DIRS:
                row=direction if rows==8 else 0
                first=sheet.crop((0,row*fh,fw,(row+1)*fh)).getbbox()
                feet.append(fh-first[3] if first else 0)
            animations.append('{'+symbol+','+symbol+'_frames,'+str(count)+',{'+','.join(map(str,m['durations']))+'},'+str(max(64,fw,fh))+',{'+','.join(map(str,feet))+'},'+','.join(map(str,times))+','+flame_symbol+','+anchor_symbol+'}')
        symbol=f's{dex}_portrait'; array(symbol,pixels(portrait,palette,tiled=True))
        definitions.append('    {{'+','.join(f'0x{v:04x}' for v in rgb555)+'},{'+','.join(animations)+'},'+symbol+'}')
    lines.append('const NativeSpecies nativeSpecies[11] = {\n'+',\n'.join(definitions)+'\n};')
    out.parent.mkdir(parents=True,exist_ok=True)
    header.parent.mkdir(parents=True,exist_ok=True)
    out.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    header.write_text('''#ifndef GENERATED_SPRITES_H
#define GENERATED_SPRITES_H
#include <stdint.h>
enum { NATIVE_IDLE, NATIVE_WALK, NATIVE_ATTACK, NATIVE_HURT, NATIVE_CELEBRATE, NATIVE_DEFEATED, NATIVE_ANIM_COUNT };
typedef struct {
    const uint16_t *pixels, *frameOffsets;
    uint8_t frames, durations[16], sourceSize, foot[8];
    uint8_t rushTime, hitTime, returnTime;
    const uint8_t *flame;
    const int8_t *anchors;
} NativeAnim;
typedef struct { uint16_t palette[16]; NativeAnim anim[NATIVE_ANIM_COUNT]; const uint16_t *portrait; } NativeSpecies;
extern const NativeSpecies nativeSpecies[11];
#endif
''',encoding='utf-8')
    print(f'OK: {frames_total} animation frames in eight directions; {packed_words*2} bytes of packed frame data')

if __name__=='__main__': main()
