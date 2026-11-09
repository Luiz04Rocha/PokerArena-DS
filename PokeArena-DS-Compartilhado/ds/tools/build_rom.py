#!/usr/bin/env python3
"""Build with an existing BlocksDS/Wonderful toolchain without needing make.

Python 3.10+; compiler, SDK and packaging tools must already be installed.
Does not install software or modify system environment settings.
"""
import argparse
import os
from pathlib import Path
import subprocess

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wonderful', type=Path, default=Path(os.environ.get('WONDERFUL_TOOLCHAIN','/opt/wonderful')))
    parser.add_argument('--blocksds', type=Path, default=None)
    args=parser.parse_args()
    wf=args.wonderful.resolve()
    sdk=(args.blocksds or Path(os.environ.get('BLOCKSDS',str(wf/'thirdparty/blocksds/core')))).resolve()
    ext='.exe' if os.name=='nt' else ''
    cc=wf/'toolchain/gcc-arm-none-eabi/bin'/('arm-none-eabi-gcc'+ext)
    ndstool=sdk/'tools/ndstool'/('ndstool'+ext)
    for f in [cc,ndstool,sdk/'sys/crts/ds_arm9.specs']:
        if not f.is_file(): parser.error(f'Arquivo do SDK nao encontrado: {f}')
    root=Path(__file__).resolve().parents[1]
    build=root/'build/blocksds'; build.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy()
    env['WONDERFUL_TOOLCHAIN']=wf.as_posix()
    env['BLOCKSDS']=sdk.as_posix()
    env['PATH']=os.pathsep.join([str(wf/'bin'),str(cc.parent),env.get('PATH','')])
    common=['-mthumb','-mcpu=arm946e-s+nofp','-DARM9','-D__NDS__','-D__BLOCKSDS__','-specs='+str(sdk/'sys/crts/ds_arm9.specs')]
    objects=[]
    def run(command): subprocess.run([str(x) for x in command],env=env,check=True,cwd=root)
    for source in sorted((root/'source').glob('*.c')):
        output=build/(source.stem+'.o'); objects.append(output)
        print('Compilando',source.name,flush=True)
        run([cc,*common,'-std=gnu17','-Wall','-Wextra','-Werror','-O2','-ffunction-sections','-fdata-sections',
             '-I'+str(root/'include'),'-I'+str(sdk/'libs/libnds/include'),'-c',source,'-o',output])
    elf=build/'arena_dupla_dsi.elf'
    run([cc,*common,*objects,'-L'+str(sdk/'libs/libnds/lib'),'-Wl,--gc-sections,-Map,'+str(build/'arena_dupla_dsi.map'),
         '-Wl,--start-group','-lnds9','-lc','-Wl,--end-group','-o',elf])
    rom=root/'arena_dupla_dsi.nds'
    run([ndstool,'-c',rom,'-9',elf,'-7',sdk/'sys/arm7/main_core/arm7_maxmod.elf',
         '-b',sdk/'sys/icon.gif','PokeArena DS v0.35;DS / DSi - prototipo;Luiz'])
    print('ROM gerada:',rom,rom.stat().st_size,'bytes')

if __name__=='__main__': main()
