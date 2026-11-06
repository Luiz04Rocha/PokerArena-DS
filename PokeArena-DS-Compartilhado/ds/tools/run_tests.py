#!/usr/bin/env python3
"""Compile and run portable C logic/HUD checks (a host compiler, not devkitARM)."""
import argparse
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC','cc'))
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error(f'Compilador C nao encontrado: {args.cc}. Informe --cc gcc, clang ou caminho do tcc.')
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='arena-tests-') as temp:
        exe = Path(temp)/('test_game.exe' if os.name=='nt' else 'test_game')
        subprocess.run([compiler,'-std=c99','-Wall','-Werror','-I'+str(root/'include'),
            str(root/'source/data.c'),str(root/'source/arena.c'),str(root/'source/game.c'),str(root/'source/visuals.c'),str(root/'source/screens.c'),str(root/'tests/test_game.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)

if __name__=='__main__': main()
