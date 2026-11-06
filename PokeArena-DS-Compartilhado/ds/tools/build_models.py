#!/usr/bin/env python3
"""Reduce the received nature GLBs to small, flat-shaded native DS meshes."""
from pathlib import Path
import json,struct,io,math
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]

def convert(name,budget):
    raw=(ROOT/f'reference/pokearena-original/public/assets/nature/{name}.glb').read_bytes()
    length=struct.unpack_from('<I',raw,12)[0];doc=json.loads(raw[20:20+length]);pos=20+length
    size=struct.unpack_from('<I',raw,pos)[0];binary=raw[pos+8:pos+8+size]
    def accessor(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        fmt={5126:'f',5125:'I',5123:'H',5121:'B'}[a['componentType']];n={'VEC3':3,'VEC2':2,'SCALAR':1}[a['type']]
        stride=v.get('byteStride',struct.calcsize(fmt)*n);base=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from('<'+fmt*n,binary,base+i*stride) for i in range(a['count'])]
    images=[]
    for image in doc['images']:
        v=doc['bufferViews'][image['bufferView']];offset=v.get('byteOffset',0)
        images.append(Image.open(io.BytesIO(binary[offset:offset+v['byteLength']])).convert('RGB'))
    faces=[];points=[]
    for mesh in doc['meshes']:
        for p in mesh['primitives']:
            a=p['attributes'];vertices=accessor(a['POSITION']);uv=accessor(a['TEXCOORD_0'])
            points.extend(vertices)
            indices=[x[0] for x in accessor(p['indices'])] if 'indices' in p else list(range(len(vertices)))
            material=doc['materials'][p['material']]['pbrMetallicRoughness']
            texture=images[doc['textures'][material['baseColorTexture']['index']]['source']] if 'baseColorTexture' in material else None
            for i in range(0,len(indices),3):
                ids=indices[i:i+3];tri=[vertices[j] for j in ids]
                u=sum(uv[j][0] for j in ids)/3;v=sum(uv[j][1] for j in ids)/3
                rgb=texture.getpixel((min(texture.width-1,max(0,int(u*texture.width))),min(texture.height-1,max(0,int(v*texture.height))))) if texture else tuple(round(c*255) for c in material.get('baseColorFactor',(1,1,1,1))[:3])
                faces.append((tri,rgb))
    mins=[min(p[i] for p in points) for i in range(3)];maxs=[max(p[i] for p in points) for i in range(3)]
    height=maxs[1]-mins[1];center=((mins[0]+maxs[0])/2,mins[1],(mins[2]+maxs[2])/2)
    normal=lambda p:tuple((p[i]-center[i])/height for i in range(3))
    for grid in range(12,1,-1):
        clusters={}
        key=lambda p:tuple(round(x*grid) for x in normal(p))
        for p in points:clusters.setdefault(key(p),[]).append(normal(p))
        means={k:tuple(sum(v[i] for v in vs)/len(vs) for i in range(3)) for k,vs in clusters.items()}
        reduced=[];seen=set()
        for tri,rgb in faces:
            ks=[key(p) for p in tri];tag=tuple(sorted(ks))
            if len(set(ks))<3 or tag in seen:continue
            seen.add(tag);vs=[means[k] for k in ks]
            a=[vs[1][i]-vs[0][i] for i in range(3)];b=[vs[2][i]-vs[0][i] for i in range(3)]
            n=(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]);length=math.sqrt(sum(x*x for x in n))
            if length<1e-7:continue
            light=.72+.28*max(0,(-n[0]+2*n[1]+n[2])/(length*math.sqrt(6)))
            if 'Tree' in name and rgb[1]>rgb[0]*1.15 and rgb[1]>rgb[2]*1.15:
                rgb=(min(rgb[0],100),min(rgb[1],147),max(38,min(rgb[2],72)))
            color=[min(31,round(c/255*31*light)) for c in rgb]
            reduced.append((vs,color[0]|color[1]<<5|color[2]<<10))
        if len(reduced)<=budget:break
    return reduced

def main():
    c=['/* Reduced from received nature models. See docs/CREDITOS.md. */','#include "generated_models.h"']
    definitions=[]
    for name,budget in (('Rock_1',70),('Rock_3',80),('NormalTree_5',110),('PineTree_2',100)):
        faces=convert(name,budget);assert len(faces)<=budget,(name,len(faces))
        c.append(f'static const NativeTriangle {name}[] = {{')
        for vertices,color in faces:
            axes=['{'+','.join(str(round(p[axis]*256)) for p in vertices)+'}' for axis in range(3)]
            c.append('    {'+','.join(axes)+f',0x{color:04x}'+'},')
        c.append('};');definitions.append('{'+str(len(faces))+','+name+'}')
        print(name,len(faces),'triangles')
    c.append('const NativeMesh nativeMeshes[4] = {'+','.join(definitions)+'};')
    (ROOT/'ds/source/generated_models.c').write_text('\n'.join(c)+'\n')
    (ROOT/'ds/include/generated_models.h').write_text('''#ifndef GENERATED_MODELS_H
#define GENERATED_MODELS_H
#include <stdint.h>
typedef struct { int16_t x[3],y[3],z[3]; uint16_t color; } NativeTriangle;
typedef struct { int count; const NativeTriangle *triangles; } NativeMesh;
extern const NativeMesh nativeMeshes[4];
#endif
''')
if __name__=='__main__':main()
