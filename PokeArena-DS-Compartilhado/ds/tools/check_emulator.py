#!/usr/bin/env python3
"""Check the real ROM through a local DS libretro core, without a GUI.

Requires Pillow, ARM nm/objcopy/gcc, the linked ELF and the emulator core.
The core/toolchain are not downloaded by this script. No ROM state is modified:
assertions read emulated RAM, and controls use joypad/touch input callbacks.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import faulthandler
import json
import os
import subprocess
import struct
import time
from xml.etree import ElementTree

from PIL import Image,ImageStat,ImageChops


def main():
    faulthandler.enable()
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--nm', type=Path, required=True)
    parser.add_argument('--rom', type=Path, default=root/'arena_dupla_dsi.nds')
    parser.add_argument('--elf', type=Path, default=root/'build/blocksds/arena_dupla_dsi.elf')
    parser.add_argument('--out', type=Path, default=root.parent/'docs/capturas-v0.29')
    parser.add_argument('--smoke',action='store_true',help='Only capture native menus and the first fight')
    parser.add_argument('--flash-probe',action='store_true',help='Record every display frame during attacks')
    parser.add_argument('--night',action='store_true',help='Use night lighting for the frame probe')
    parser.add_argument('--waves',action='store_true',help='Play all three waves as each starter')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    run_kind='waves' if args.waves else 'probe' if args.flash_probe else 'regression'
    work = root/'build/emulator'/args.core.stem/run_kind
    work.mkdir(parents=True, exist_ok=True)
    tool_bin = args.nm.resolve().parent
    ext = '.exe' if os.name == 'nt' else ''
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([str(tool_bin),str(tool_bin.parents[2]/'bin'),env.get('PATH','')])
    layout_object = work/'inspect_layout.o'
    layout_binary = work/'inspect_layout.bin'
    subprocess.run([str(tool_bin/('arm-none-eabi-gcc'+ext)), '-std=gnu17', '-mthumb', '-mcpu=arm946e-s+nofp',
                    '-I'+str(root/'include'), '-c', str(root/'tests/inspect_game_layout.c'), '-o',str(layout_object)],env=env,check=True)
    subprocess.run([str(tool_bin/('arm-none-eabi-objcopy'+ext)), '-O','binary','--only-section=.arena_layout',
                    str(layout_object),str(layout_binary)],env=env,check=True)
    data = layout_binary.read_bytes()
    records = [struct.unpack('<24sII',data[i:i+32]) for i in range(0,len(data),32)]
    layout = {name.rstrip(b'\0').decode():(offset,size) for name,offset,size in records}
    # Read ELF32 symbols directly: avoids depending on nm's optional host DLLs.
    elf=args.elf.read_bytes();assert elf[:6]==b'\x7fELF\x01\x01'
    shoff=struct.unpack_from('<I',elf,32)[0]
    shsize,shnum=struct.unpack_from('<HH',elf,46)
    sections=[struct.unpack_from('<10I',elf,shoff+i*shsize) for i in range(shnum)]
    matches=[];symbols={}
    for section in sections:
        if section[1]!=2:continue
        strings=sections[section[6]];names=elf[strings[4]:strings[4]+strings[5]]
        for offset in range(section[4],section[4]+section[5],section[9]):
            name,value,symbol_size,_,_,_=struct.unpack_from('<IIIBBH',elf,offset)
            label=names[name:].split(b'\0',1)[0].decode(errors='replace')
            symbols[label]=(value,symbol_size)
            if label=='game':matches.append((value,symbol_size))
    assert len(matches) == 1, 'Expected the single native game state symbol'
    address, size = matches[0]
    assert size == layout['$game'][1], 'ARM metadata does not match the linked game'
    print('Layout and ARM symbols verified',flush=True)

    dll = C.CDLL(str(args.core.resolve()))
    ENV = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
    VIDEO = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
    AUDIO = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
    BATCH = C.CFUNCTYPE(C.c_size_t, C.POINTER(C.c_int16), C.c_size_t)
    POLL = C.CFUNCTYPE(None)
    INPUT = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)
    LOG = C.CFUNCTYPE(None, C.c_int, C.c_char_p)
    class Variable(C.Structure):
        _fields_ = [('key', C.c_char_p), ('value', C.c_char_p)]
    class GameInfo(C.Structure):
        _fields_ = [('path', C.c_char_p), ('data', C.c_void_p), ('size', C.c_size_t), ('meta', C.c_char_p)]

    # These objects stay alive while the core holds their pointers.
    directory = os.fsencode(str(work.resolve()))
    options = {b'desmume_internal_resolution': b'256x192', b'desmume_screens_layout': b'top/bottom',
               b'desmume_cpu_mode': b'interpreter', b'desmume_pointer_mouse': b'enabled',
               b'desmume_pointer_type': b'touch'}
    audio_peak=0
    held = set(); pointer = None; frame = None; frame_count = 0; pixel_format = 0
    log_callback = LOG(lambda level, message: print('Core:',message.decode(errors='replace').strip(),flush=True) if level>=2 else None)
    def environment(cmd, data):
        nonlocal pixel_format
        if cmd == 27:
            C.cast(data, C.POINTER(C.c_void_p))[0] = C.cast(log_callback, C.c_void_p); return True
        if cmd in (9, 30, 31):
            C.cast(data, C.POINTER(C.c_char_p))[0] = directory; return True
        if cmd == 10:
            pixel_format = C.cast(data, C.POINTER(C.c_int))[0]; return pixel_format in (0, 1, 2)
        if cmd == 3:
            C.cast(data, C.POINTER(C.c_bool))[0] = True; return True
        if cmd == 15:
            var = C.cast(data, C.POINTER(Variable)).contents
            if var.key in options: var.value = options[var.key]; return True
            return False
        if cmd == 17:
            C.cast(data, C.POINTER(C.c_bool))[0] = False; return True
        if cmd == 16:
            variables=C.cast(data,C.POINTER(Variable));index=0
            while variables[index].key:
                var=variables[index]
                options.setdefault(var.key,var.value.split(b'; ',1)[-1].split(b'|',1)[0])
                index+=1
            return True
        return cmd in (11, 16, 18, 35, 36)
    def video(data, width, height, pitch):
        nonlocal frame, frame_count
        frame_count += 1
        if data and data != C.c_void_p(-1).value:
            frame = (C.string_at(data, pitch*height), width, height, pitch, pixel_format)
    def status(port, device, index, button):
        if port != 0: return 0
        if device == 1: return int(button in held)
        if device == 6 and pointer:
            if button == 2: return 1
            if button == 0: return int((pointer[0]+0.5)*65536/256)-32768
            if button == 1: return int((192+pointer[1]+0.5)*65536/384)-32768
        return 0
    def audio_batch(data,frames):
        nonlocal audio_peak
        for i in range(frames*2): audio_peak=max(audio_peak,abs(data[i]))
        return frames
    callbacks = [ENV(environment), VIDEO(video), AUDIO(lambda l,r: None), BATCH(audio_batch), POLL(lambda: None), INPUT(status)]
    for name, callback in zip(['environment', 'video_refresh', 'audio_sample', 'audio_sample_batch', 'input_poll', 'input_state'], callbacks):
        setter = getattr(dll, 'retro_set_'+name); setter.argtypes = [type(callback)]; setter(callback)
    dll.retro_get_memory_data.argtypes = [C.c_uint]; dll.retro_get_memory_data.restype = C.c_void_p
    dll.retro_get_memory_size.argtypes = [C.c_uint]; dll.retro_get_memory_size.restype = C.c_size_t
    dll.retro_load_game.argtypes = [C.POINTER(GameInfo)]; dll.retro_load_game.restype = C.c_bool
    class SystemInfo(C.Structure):
        _fields_=[('name',C.c_char_p),('version',C.c_char_p),('extensions',C.c_char_p),('fullpath',C.c_bool),('block_extract',C.c_bool)]
    info=SystemInfo();dll.retro_get_system_info(C.byref(info))
    emulator=info.name.decode()+' '+info.version.decode()+' libretro'
    print('Core:',emulator,flush=True)
    dll.retro_init()
    print('Emulator initialized',flush=True)
    rom_path = os.fsencode(str(args.rom.resolve()))
    rom_bytes=args.rom.read_bytes();rom_buffer=C.create_string_buffer(rom_bytes)
    assert dll.retro_load_game(C.byref(GameInfo(rom_path,C.cast(rom_buffer,C.c_void_p),len(rom_bytes),None)))
    print('ROM loaded',flush=True)
    ram = dll.retro_get_memory_data(2)
    ram_size = dll.retro_get_memory_size(2)
    game_offset = address-0x02000000
    assert ram and 0 <= game_offset <= ram_size-size
    arena_left,arena_top=layout['$arenaMin']
    arena_right,arena_bottom=layout['$arenaMax']
    prop_stride,_=layout['$arenaProp']
    props_address,props_size=symbols['arenaProps']
    props_raw=C.string_at(ram+props_address-0x02000000,props_size)
    footprints=[]
    for offset in range(0,props_size,prop_stride):
        x,y,_,_,half_width,depth=struct.unpack_from('<6i',props_raw,offset)
        footprints.append((x-half_width,y-depth,x+half_width,y))
    def snapshot():
        raw = C.string_at(ram+game_offset, size)
        values = {}
        for name, (offset, field_size) in layout.items():
            if name.startswith('$'): continue
            if name == 'moveCooldowns': values[name] = [int.from_bytes(raw[offset+i*4:offset+i*4+4], 'little', signed=True) for i in range(4)]
            elif name == 'evolved': values[name] = bool(raw[offset])
            else: values[name] = int.from_bytes(raw[offset:offset+field_size], 'little', signed=name != 'frameCounter' and field_size == 4)
        return values
    def run(frames):
        for _ in range(frames): dll.retro_run()
    def press(button):
        held.add(button); run(6); held.remove(button); run(6)
    def touch(x, y, frames=6):
        nonlocal pointer
        pointer = (x, y); run(frames); pointer = None; run(6)
    def capture():
        assert frame
        raw,w,h,pitch,fmt = frame
        assert (w,h) == (256,384)
        if fmt == 1: image = Image.frombytes('RGB', (w,h), raw, 'raw', 'BGRX', pitch)
        else:
            rgb = bytearray()
            for y in range(h):
                for x in range(w):
                    p = int.from_bytes(raw[y*pitch+x*2:y*pitch+x*2+2], 'little')
                    rgb.extend(((p>>11&31)*255//31, (p>>5&63)*255//63, (p&31)*255//31) if fmt == 2 else
                               ((p>>10&31)*255//31, (p>>5&31)*255//31, (p&31)*255//31))
            image = Image.frombytes('RGB', (w,h), bytes(rgb))
        assert len(image.getcolors(256*384) or []) > 1, 'Blank emulator frame'
        return image
    def shot(name):
        capture().save(out/name);return snapshot()
    def metric(name):
        addr,length=symbols[name];assert length==4
        return int.from_bytes(C.string_at(ram+addr-0x02000000,4),'little')
    def camera_zoom():
        addr,length=symbols['camera'];assert length==12
        return struct.unpack('<iii',C.string_at(ram+addr-0x02000000,length))[2]
    def actor_pose(actor):
        addr,length=symbols['poses']
        assert length%4==0
        return struct.unpack('<iiii',C.string_at(ram+addr-0x02000000+actor*(length//4),16))

    def strike_marks(pokemon):
        dex=(4,7,1,25)[pokemon]
        path=root.parent/'reference/spritecollab-f92cb9f/sprite'/f'{dex:04d}'/'AnimData.xml'
        nodes={n.findtext('Name'):n for n in ElementTree.parse(path).findall('Anims/Anim')}
        attack=nodes['Attack']
        while attack.findtext('CopyOf'):
            attack=nodes[attack.findtext('CopyOf')]
        durations=[int(n.text) for n in attack.findall('Durations/Duration')]
        return [sum(durations[:int(attack.findtext(tag))]) for tag in ('RushFrame','HitFrame','ReturnFrame')]
    def wait_for_fight():
        for _ in range(14):
            ticks=snapshot()['introTicks']
            if not ticks: break
            run(min(16,2*ticks))
        assert snapshot()['state']==2 and snapshot()['introTicks']==0
    def choose_mode(mode):
        assert snapshot()['state']==0
        touch(65,130 if mode in (1,2) else 59)
        assert snapshot()['state']==5
        touch(120,{0:49,1:129,2:87,3:87}[mode])
        assert snapshot()['state']==1 and snapshot()['mode']==mode

    started = time.monotonic(); states = {}; checks = [];performance=[];random_periods=[];ultimate_zooms=[]
    try:
        run(180)
        assert snapshot()['state'] == 0
        if args.waves:
            def obstruction(ax,ay,bx,by,margin=0):
                for left,top,right,bottom in footprints:
                    start,end=0.0,1.0
                    for a,b,low,high in ((ax,bx,left-margin,right+margin),(ay,by,top-margin,bottom+margin)):
                        if max(a,b)<=low or min(a,b)>=high:
                            start,end=1,0
                            break
                        if a!=b:
                            near,far=sorted(((low-a)/(b-a),(high-a)/(b-a)))
                            start=max(start,near);end=min(end,far)
                    if start<end:return left,top,right,bottom
                return None
            matches=[]
            for pokemon,mode in [(p,1) for p in range(4)]+[(0,2)]:
                choose_mode(mode)
                while snapshot()['selectedPokemon']!=pokemon:press(7)
                press(8);wait_for_fight()
                reached=1
                stage_hp=[snapshot()['enemyMaxHp']]
                for _ in range(1550):
                    s=snapshot()
                    if s['state']==3:break
                    if s['wave']>reached:
                        reached=s['wave'];stage_hp.append(s['enemyMaxHp'])
                        held.clear();run(4)
                        shot(f'ondas-{mode}-{pokemon}-{reached}.png')
                    px,py=s['playerX']/16,s['playerY']/16
                    ex,ey=s['enemyX']/16,s['enemyY']/16
                    dx,dy=ex-px,ey-py
                    distance=(dx*dx+dy*dy)**0.5
                    held.clear()
                    cover=obstruction(px,py,ex,ey,4)
                    if cover:
                        left,top,right,bottom=cover
                        choices=[(x,y) for x in (left-9,right+9) for y in (top-9,bottom+9)
                                 if arena_left<=x<=arena_right and arena_top<=y<=arena_bottom and abs(x-px)+abs(y-py)>3
                                 and not obstruction(px,py,x,y,4)]
                        if choices:
                            tx,ty=min(choices,key=lambda c:abs(c[0]-px)+abs(c[1]-py)+abs(c[0]-ex)+abs(c[1]-ey)
                                      +40*bool(obstruction(c[0],c[1],ex,ey,4)))
                            dx,dy=tx-px,ty-py
                    if distance>34 or cover:
                        if abs(dx)>2:held.add(7 if dx>0 else 6)
                        if abs(dy)>2:held.add(5 if dy>0 else 4)
                    held.add(8)
                    special=None
                    if s['ultimateCharge']==100:special=11
                    elif s['evolutionTicks']>=(900 if s['evolutionStage'] else 750) and s['evolutionStage']<(1 if pokemon==3 else 2):
                        special=2
                    elif s['moveCooldowns'][1]==0 and distance<(115 if pokemon==2 else 36):special=0
                    elif s['moveCooldowns'][3]==0 and distance<(54 if pokemon==2 else 125):special=1
                    if special is not None:
                        held.add(special);run(4);held.remove(special)
                    run(8)
                held.clear();run(4)
                s=snapshot()
                match={'pokemon':pokemon,'mode':mode,'wave':s['wave'],'result':s['result'],
                       'remaining_hp':s['playerHp'],'elapsed_seconds':240-s['fightTicks']/30,
                       'opponent_hp_by_wave':stage_hp}
                print('Wave playthrough:',match,flush=True)
                matches.append(match)
                if not (s['state']==3 and s['result']==1 and s['wave']==3):
                    shot(f'ondas-{mode}-{pokemon}-falha.png')
                    raise AssertionError(s)
                shot(f'ondas-{mode}-{pokemon}-vitoria.png')
                press(0);assert snapshot()['state']==0
            (out/'ondas.json').write_text(json.dumps({'emulator':emulator,
                'rom_sha256':hashlib.sha256(rom_bytes).hexdigest(),'matches':matches},indent=2))
            return
        states['title'] = shot('01-titulo.png')
        capture().crop((0,192,256,384)).save(out/'24-menu-inicial.png')
        title_frames=[]
        for _ in range(72):
            title_frames.append(capture().crop((0,0,256,192)))
            run(8)
        assert len({f.crop((28,104,117,169)).tobytes() for f in title_frames})>=12
        assert len({f.crop((126,104,221,169)).tobytes() for f in title_frames})>=12
        assert len({f.crop((66,170,190,186)).tobytes() for f in title_frames})>=3
        assert len({f.crop((62,10,192,104)).tobytes() for f in title_frames})==1
        assert len({f.crop((0,8,62,104)).tobytes() for f in title_frames})>=12
        assert len({f.crop((192,8,256,104)).tobytes() for f in title_frames})>=12
        title_frames[0].save(out/'33-abertura-animada.gif',save_all=True,append_images=title_frames[1:],duration=130,loop=0)
        for _ in range(13):
            if metric('titleSeenForms')==0x7ff and metric('titleSeenPoses')&0x17==0x17:
                break
            run(144)
        assert metric('titleSeenForms')==0x7ff, 'The opening must rotate through all eleven forms'
        assert metric('titleSeenPoses')&0x17==0x17, 'The opening must show idle, walk, attack and celebrate'
        press(7);assert snapshot()['menuCursor']==1
        press(8);assert snapshot()['state']==0
        touch(210,59);assert snapshot()['state']==0 and snapshot()['menuCursor']==1
        touch(65,130);assert snapshot()['state']==5 and snapshot()['mode']==1
        states['horde']=shot('29-horda.png')
        touch(120,49);assert snapshot()['state']==5 and snapshot()['mode']==1
        touch(120,87);assert snapshot()['state']==1 and snapshot()['mode']==2
        press(0);press(0);assert snapshot()['state']==0
        touch(211,130);assert snapshot()['state']==9
        states['options']=shot('30-opcoes.png')
        touch(120,49);assert snapshot()['state']==9 and snapshot()['muted']
        touch(120,49);assert not snapshot()['muted']
        touch(120,89);assert snapshot()['state']==7
        states['help']=shot('11-como-jogar.png');touch(100,178);assert snapshot()['state']==9
        touch(120,131);assert snapshot()['state']==8
        states['credits']=shot('12-creditos.png');touch(100,178);assert snapshot()['state']==9
        touch(100,178);assert snapshot()['state']==0
        touch(45,59); assert snapshot()['state'] == 5
        checks.append('falling leaves and prompt, all eleven opening forms and four actions, inactive multiplayer card by touch and buttons, horde, options sound, help, credits and back navigation')
        states['mode']=shot('02-modo.png')
        capture().crop((0,192,256,384)).save(out/'25-menu-modos.png')
        touch(120,87);assert snapshot()['state']==1 and snapshot()['mode']==3
        press(8);assert snapshot()['state']==6
        press(8);assert snapshot()['state']==2 and snapshot()['introTicks']>60
        assert snapshot()['rivalHp']==snapshot()['partnerHp']==90
        states['team_opening']=shot('31-abertura-2v2.png')
        wait_for_fight()
        run(20)
        assert all(actor_pose(i)[0]>=0 for i in range(4))
        states['team_fight']=shot('32-combate-2v2.png')
        held.add(8);run(20);held.clear()
        assert snapshot()['playerCastSerial']>0 and metric('sceneTextureFailures')==0
        run(240)
        assert snapshot()['rivalAttacks']>0 and snapshot()['partnerHp']<90
        assert snapshot()['state']==2 and snapshot()['wave']==1
        press(3);press(0);press(0);press(0)
        assert snapshot()['state']==0
        touch(65,59)
        checks.append('2v2 selectable by touch, four actors, rival attacks partner and no wave transition')
        touch(120,49); assert snapshot()['state']==1 and snapshot()['mode']==0
        duel_lighting_count=metric('sceneRandomSelections')+1
        states['leader']=shot('03-lider.png')
        press(8); assert snapshot()['state'] == 6
        press(11); assert snapshot()['opponentPokemon'] == 2
        states['select'] = shot('04-oponente.png')
        press(8)
        opening=snapshot()
        assert opening['state']==2 and opening['introTicks']>60
        states['opening']=shot('18-abertura.png')
        held.update((8,7));run(20);held.clear()
        assert snapshot()['playerX']==opening['playerX'] and snapshot()['playerCastSerial']==0
        assert snapshot()['fightTicks']==opening['fightTicks']
        press(3);assert snapshot()['state']==4
        paused_intro=snapshot()['introTicks'];run(30)
        assert snapshot()['introTicks']==paused_intro
        press(3);wait_for_fight()
        checks.append('match countdown blocks attacks and the clock and resumes after pause')
        run(12)
        audio_peak=0;run(4)
        assert audio_peak>0 and metric('musicNotes')>0,'No music after the opening cue'
        assert metric('sceneRandomSelections')==duel_lighting_count
        first_period=metric('battlePeriod');random_periods.append(first_period)
        assert metric('period')==first_period and 0<=first_period<4
        states['fight']=shot('05-arena-25d.png')
        capture().crop((0,192,256,384)).save(out/'22-hud-batalha.png')
        if args.flash_probe:
            if args.night:
                press(3)
                while metric('lightingMode')!=3:press(11)
                press(3);run(12)
            frames=[];brightness=[];changes=[];previous=None
            dodge_at=None;cancelled_dash=False
            held.add(8)
            for index in range(240):
                for button,begin in ((0,30),(1,90),(9,150)):
                    if index==begin:held.add(button)
                    if index==begin+4:held.discard(button)
                if index>=150 and dodge_at is None and snapshot()['playerDashTicks']>0:
                    dodge_at=index
                    held.add(10)
                if dodge_at is not None and index==dodge_at+4:
                    held.discard(10)
                    state=snapshot()
                    cancelled_dash=state['dodgeTimer']>0 and not state['playerDashTicks']
                run(1);im=capture().crop((0,0,256,192));frames.append(im)
                brightness.append(round(sum(ImageStat.Stat(im).mean)/3,2))
                assert 10<brightness[-1]<240 and len(im.getcolors(256*192) or [])>16,'Blank/flat attack frame'
                changes.append(round(sum(ImageStat.Stat(ImageChops.difference(im,previous)).mean)/3,2) if previous else 0)
                previous=im
            held.clear()
            frames[0].save(out/'flash-probe.gif',save_all=True,append_images=frames[1:],duration=20,loop=0)
            worst=sorted(range(1,len(frames)),key=lambda i:changes[i],reverse=True)[:8]
            for i in worst:frames[i].save(out/f'flash-{i:03d}.png')
            report={'emulator':emulator,'rom_sha256':hashlib.sha256(rom_bytes).hexdigest(),
                    'night':args.night,'frames':len(frames),'brightness':brightness,'changes':changes,'worst':worst,
                    'upload_end_line':metric('sceneUploadEndLine'),'upload_deferrals':metric('sceneUploadLate'),
                    'tail_lights':metric('sceneTailLights'),'cancelled_dash':cancelled_dash}
            assert report['upload_end_line']<214,'Texture copy crossed into 3D rendering'
            assert metric('sceneTextureFailures')==0
            assert cancelled_dash,'Frame probe did not cancel a dash'
            if args.night:assert report['tail_lights']>0
            (out/'flash-probe.json').write_text(json.dumps(report,indent=2))
            print('Flash probe:',[(i,brightness[i],changes[i]) for i in worst],flush=True);return
        rendered=metric('sceneRenderCount');logic=snapshot()['frameCounter'];uploads=metric('sceneTextureUploads')
        dust_draws=metric('sceneStepDustDraws')
        walking=[]
        held.add(7)
        for _ in range(20):
            run(3)
            walking.append(capture().crop((0,0,256,192)))
        held.remove(7)
        render_rate=metric('sceneRenderCount')-rendered;logic_rate=snapshot()['frameCounter']-logic
        print('Frames in 60 VBlanks:',render_rate,'logic:',logic_rate,'texture uploads:',metric('sceneTextureUploads')-uploads,flush=True)
        print('Timing us:',{n:metric(n) for n in ('graphicsSceneUsec','graphicsHudUsec','graphicsMaxUsec','graphicsEvenUsec','graphicsOddUsec','graphicsEvenHud')},flush=True)
        assert render_rate>=58 and 29<=logic_rate<=31,(render_rate,logic_rate)
        checks.append('60 display updates with 30 logic updates per second')
        assert metric('battlePeriod')==first_period and metric('sceneRandomSelections')==duel_lighting_count
        performance.append({'scenario':'movement','display_frames':render_rate,'logic_ticks':logic_rate,'emulated_frames':60})
        assert metric('sceneStepDustDraws')>dust_draws,'Walking dust was not rendered'
        walking[0].save(out/'36-poeira-dos-passos.gif',save_all=True,append_images=walking[1:],duration=50,loop=0)
        assert metric('sceneIdleBreaths')>0,'Idle breathing was not rendered'
        checks.append('idle breathing rendered with normal movement and combat')
        checks.append('subtle step dust rendered during normal movement')
        if args.smoke:
            print('Preview metrics:',{n:metric(n) for n in ('sceneMaxVertices','sceneMaxPolygons','sceneTextureFailures')})
            print('OK: native 2.5D menus and fight');return
        assert metric('musicNotes')>0,'Battle music never started'
        checks.append('original battle music played')
        held.add(8)
        previous = snapshot()['moveCooldowns'][0]; starts = 0
        for _ in range(60):
            run(1); current = snapshot()['moveCooldowns'][0]
            if current > previous: starts += 1
            previous = current
        assert starts >= 2, 'Held A did not repeat'
        checks.append('held A repeats')
        held.add(0); run(6); held.remove(0); run(6)
        assert snapshot()['moveCooldowns'][1] > 0, 'B did not activate while A was held'
        held.remove(8)
        states['cooldowns'] = shot('03-recargas-individuais.png')
        touch(64,116)
        assert snapshot()['moveCooldowns'][2] > 0, 'Touch X failed'
        touch(192,116)
        assert snapshot()['moveCooldowns'][3] > 0, 'Touch Y failed'
        checks += ['B works while A repeats', 'native touch X', 'native touch Y']
        states['touch'] = shot('04-toque-x-y.png')
        capture().crop((0,192,256,384)).save(out/'23-hud-recarga.png')
        x = snapshot()['playerX']
        held.add(7); run(30); held.remove(7)
        assert snapshot()['playerX'] > x
        touch(212,170)
        assert snapshot()['state'] == 4, 'Touch pause failed'
        before = snapshot(); run(90); after = snapshot()
        for key in ('fightTicks','moveCooldowns','frameCounter','playerX','playerHp'):
            assert after[key] == before[key], 'Pause advanced '+key
        frozen_pixels=capture().tobytes();run(30);assert capture().tobytes()==frozen_pixels,'Paused animation moved'
        music_notes=metric('musicNotes');audio_peak=0;run(24)
        assert metric('musicNotes')==music_notes and audio_peak==0,'Music continued during pause'
        states['pause'] = shot('05-pausa.png')
        capture().crop((0,192,256,384)).save(out/'26-menu-pausa.png')
        random_count=metric('sceneRandomSelections');saved_period=metric('battlePeriod')
        light_frames=[]
        for theme in range(5):
            if theme==0:touch(220,148)
            else:press(11)
            assert metric('lightingMode')==theme
            if theme<4:
                assert metric('period')==theme
                light_frames.append(capture().tobytes())
                states['lighting_'+str(theme)]=shot(f'15-horario-{theme}.png')
        assert len(set(light_frames))==4,'Lighting palettes did not change'
        assert metric('sceneRandomSelections')==random_count and metric('battlePeriod')==saved_period
        assert metric('period')==saved_period,'Random mode failed to restore the match lighting'
        touch(36,148);assert snapshot()['state']==4 and metric('lightingMode')==3
        touch(220,148);assert snapshot()['state']==4 and metric('lightingMode')==4
        checks.append('pause lighting arrows work by touch')
        checks.append('four lighting palettes and random period stable throughout each match')
        checks += ['native movement', 'native touch pause freezes clock and cooldowns']
        press(3); assert snapshot()['state'] == 2
        assert metric('sceneRandomSelections')==random_count,'Unpausing rolled the lighting again'
        while snapshot()['evolutionTicks'] < 750 and snapshot()['state'] == 2: run(60)
        assert snapshot()['state'] == 2, 'Fight ended before evolution test'
        touch(160,37); assert snapshot()['evolved']
        states['evolution'] = shot('06-evolucao.png')
        checks.append('native touch evolution')
        while snapshot()['evolutionTicks']<900 and snapshot()['state']==2:
            for direction in (7,4,6,5):
                held.add(direction); run(30); held.remove(direction)
                if snapshot()['evolutionTicks']>=900 or snapshot()['state']!=2: break
        assert snapshot()['state']==2, 'Fight ended before final evolution'
        touch(160,37); assert snapshot()['evolutionStage']==2
        states['final_evolution']=shot('09-charizard.png')
        checks.append('second evolution to Charizard')
        for _ in range(300):
            run(1)
            if snapshot()['cpuWindup']>0:
                assert actor_pose(1)[1]==2,'CPU still idle while preparing an attack'
                states['warning']=shot('10-aviso-cpu.png')
                checks.append('CPU wind-up warning')
                break
        assert 'warning' in states
        # Start all twelve character/mode combinations using console input.
        for pokemon in range(4):
            for mode in range(3):
                press(3); assert snapshot()['state'] == 4
                press(0); assert snapshot()['state'] == 1
                press(0);assert snapshot()['state']==5
                press(0);choose_mode(mode)
                for _ in range((pokemon-snapshot()['selectedPokemon'])%4): press(7)
                count_before=metric('sceneRandomSelections')
                press(3)
                if mode==0: assert snapshot()['state']==6;press(3)
                assert snapshot()['state'] == 2
                assert metric('sceneRandomSelections')==count_before+1
                random_periods.append(metric('battlePeriod'))
                assert snapshot()['selectedPokemon'] == pokemon and snapshot()['mode'] == mode
                run(8)
        wait_for_fight()
        states['duo'] = shot('07-dupla-pikachu.png')
        assert actor_pose(2)[2]==2,'Partner does not face the opponent'
        checks.append('all 4 characters in all 3 modes')
        # Exercise held stylus A and each remaining touch command in the real ROM.
        pointer = (64,92)
        previous = snapshot()['moveCooldowns'][0]; starts = 0
        for _ in range(60):
            run(1); current = snapshot()['moveCooldowns'][0]
            if current > previous: starts += 1
            previous = current
        pointer = None; run(6)
        assert starts >= 2, 'Held stylus A did not repeat'
        touch(192,92)
        assert snapshot()['moveCooldowns'][1] > 0, 'Touch B failed'
        touch(44,170)
        assert snapshot()['dodgeCooldown'] > 0, 'Touch dodge failed'
        pointer = (64,92)
        for _ in range(40):
            if snapshot()['ultimateCharge'] >= 100: break
            run(30)
        pointer = None; run(6)
        assert snapshot()['state'] == 2 and snapshot()['ultimateCharge'] >= 100
        touch(126,170)
        assert snapshot()['ultimateCharge'] < 100 and snapshot()['lastAttack'] == 4
        states['ultimate'] = shot('08-ultimate-por-toque.png')
        checks += ['held stylus A repeats', 'native touch B', 'native touch dodge', 'native touch ultimate']
        movie=[];attack_brightness=[]
        charmander_clips=[]
        charmander_moves=('brasas','pirotecnia','nitrocarga','lanca-chamas','fogo-draconico')
        def attack_launch(capture_frames):
            if not capture_frames:
                run(3)
                return []
            frames=[]
            for _ in range(3):
                run(1)
                frames.append(capture())
            return frames

        def capture_charmander_move(slot, before, frames):
            # Pick an early frame with the most newly visible fire pixels in the arena.
            box=(0,18,256,188)
            baseline=before.crop(box).get_flattened_data()
            def fire_pixels(frame):
                return sum(r>=136 and r-b>=52 and r-g>=8 and
                           max(abs(r-r0),abs(g-g0),abs(b-b0))>=24
                           for (r,g,b),(r0,g0,b0) in zip(frame.crop(box).get_flattened_data(),baseline))
            peak=max(frames[:12],key=fire_pixels)
            peak.save(out/f'charmander-{slot+1}-{charmander_moves[slot]}.png')
            charmander_clips.append((before,frames))

        def attack_frame():
            im=capture();top=im.crop((0,0,256,192))
            brightness=sum(ImageStat.Stat(top).mean)/3
            colors=len(top.getcolors(256*192) or [])
            if not (10<brightness<240 and colors>16):
                im.save(out/'quadro-rejeitado.png')
                print('Rejected frame:',brightness,colors,snapshot(),{n:metric(n) for n in ('sceneUploadEndLine','sceneUploadLate','graphicsMaxUsec','sceneMaxVertices','sceneTextureFailures')},flush=True)
                raise AssertionError('Blank/flat attack frame')
            attack_brightness.append(brightness)
            return im
        for pokemon in range(4):
            rush,hit,recover=strike_marks(pokemon)
            press(3);press(0);assert snapshot()['state']==1
            press(0);assert snapshot()['state']==5
            press(0);choose_mode(0)
            for _ in range((pokemon-snapshot()['selectedPokemon'])%4):press(7)
            press(3);assert snapshot()['state']==6
            target=2 if pokemon==0 else 0
            while snapshot()['opponentPokemon']!=target:press(11)
            press(3);assert snapshot()['state']==2
            wait_for_fight()
            for slot,(x,y) in enumerate(((64,92),(192,92),(64,128),(192,128))):
                before=capture() if pokemon==0 else None
                cast=snapshot()['playerCastSerial'];rendered=metric('sceneRenderCount');logic=snapshot()['frameCounter']
                pointer=(x,y);charm_frames=attack_launch(before is not None);pointer=None
                assert snapshot()['playerCastSerial']>cast,'Move did not start'
                age=snapshot()['playerCastAge']*2
                expected=[rush+(recover-rush)*t//12 if slot==2 else hit+t*3//2 for t in (age,age+1)]
                pose=actor_pose(0)
                assert pose[1]==2 and pose[3] in expected,(pokemon,slot,pose,expected)
                if slot==2:assert snapshot()['playerDashTicks']>0,'Dash teleported instead of moving over time'
                for i in range(30):
                    run(1)
                    im=attack_frame()
                    if i%2==0:
                        movie.append(im)
                        if before is not None: charm_frames.append(im)
                if before is not None: capture_charmander_move(slot,before,charm_frames)
                assert snapshot()['moveCooldowns'][slot]>0 or slot==0
                renders=metric('sceneRenderCount')-rendered;logic_ticks=snapshot()['frameCounter']-logic
                performance.append({'scenario':f'pokemon_{pokemon}_move_{slot}','display_frames':renders,'logic_ticks':logic_ticks,'emulated_frames':33})
                assert renders>=29 and 15<=logic_ticks<=17,(pokemon,slot,renders,logic_ticks)
            states['effects_'+str(pokemon)]=shot(f'13-efeitos-{pokemon}.png')
            pointer=(64,92)
            for _ in range(80):
                if snapshot()['ultimateCharge']>=100:break
                run(15)
            pointer=None;run(4)
            assert snapshot()['state']==2 and snapshot()['ultimateCharge']>=100,'Ultimate did not charge'
            base_zoom=camera_zoom();peak_zoom=base_zoom
            before=capture() if pokemon==0 else None
            pointer=(126,170);charm_frames=attack_launch(before is not None);pointer=None
            age=snapshot()['playerCastAge']*2
            assert actor_pose(0)[3] in (hit+age*3//2,hit+(age+1)*3//2),'Ultimate pose out of sync'
            for i in range(42):
                run(1)
                peak_zoom=max(peak_zoom,camera_zoom())
                im=attack_frame()
                if i%2==0:
                    movie.append(im)
                    if before is not None: charm_frames.append(im)
            if before is not None: capture_charmander_move(4,before,charm_frames)
            assert snapshot()['lastAttack']==4 and snapshot()['ultimateCharge']<100
            states['ultimate_'+str(pokemon)]=shot(f'14-ultimate-{pokemon}.png')
            assert snapshot()['state']==2,'Gallery opponent defeated before the next character'
            ultimate_zooms.append({'pokemon':pokemon,'initial':base_zoom,'peak':peak_zoom})
        assert any(z['peak']>z['initial']+2 for z in ultimate_zooms),'Ultimate camera did not approach'
        checks.append('camera approaches during the ultimate')
        assert len({im.tobytes() for im in movie})>80,'Animations did not advance'
        movie[0].save(out/'combate-nativo.gif',save_all=True,append_images=movie[1:],duration=[30 if i%3<2 else 40 for i in range(len(movie))],loop=0,disposal=2)
        gallery, gallery_times=[], []
        for before, frames in charmander_clips:
            gallery.extend([before,*frames])
            gallery_times.extend([250]+[30]*3+[60]*(len(frames)-4)+[330])
        gallery[0].save(out/'charmander.gif',save_all=True,append_images=gallery[1:],duration=gallery_times,loop=0,disposal=2)
        checks.append('all twenty animated moves including four ultimates in the native ROM')
        assert metric('sceneCharmanderFx')==31, f"Charmander effects mask {metric('sceneCharmanderFx'):05b}"
        checks.append('all five redesigned Charmander attacks rendered and captured individually')
        checks.append('all twenty casts show the expected PMD release or rush frame')
        checks.append(f'{len(attack_brightness)} consecutive attack display samples without blank/flat frames')

        # Reach the southern face of the northwest rock using normal controls.
        press(3);press(0);assert snapshot()['state']==1
        while snapshot()['selectedPokemon']!=0:press(7)
        press(3);assert snapshot()['state']==6
        press(3);assert snapshot()['state']==2
        wait_for_fight()
        rock_left,_,rock_right,rock_bottom=footprints[0]
        rock_x=(rock_left+rock_right)//2
        south_face=rock_bottom+4
        held.add(6);run(snapshot()['playerX']//16-rock_x);held.remove(6)
        held.add(4);run(65);held.remove(4)
        stopped=snapshot()
        assert abs(stopped['playerX']-rock_x*16)<=3*16 and stopped['playerY']==south_face*16,stopped
        reactions=metric('sceneGroundReactions')
        held.add(9);run(4);held.remove(9)
        assert snapshot()['lastAttack']==2
        states['cover']=shot('16-pedra-e-poeira.png')
        run(14)
        assert snapshot()['playerY']==stopped['playerY'],'Dash entered the rock'
        assert metric('sceneGroundReactions')>reactions,'Dust did not render'
        held.add(7);run(28);held.remove(7)
        held.add(4);run(18);held.remove(4)
        assert snapshot()['playerY']<stopped['playerY'],'Could not move around the rock'
        states['cover_exit']=shot('17-contorno-da-pedra.png')
        checks += ['rock stops player walking and dashing', 'player can round cover', 'ground reactions rendered']
        held.add(8)
        for _ in range(420):
            battle=snapshot()
            if battle['state']==3: break
            if battle['ultimateCharge']==100:
                held.add(11);run(3);held.remove(11)
            run(4 if min(battle['playerHp'],battle['enemyHp'])<40 else 12)
        held.clear()
        assert snapshot()['state']==3 and snapshot()['result'] in (1,2),'Expected a win or knockout'
        run(4)
        states['result']=shot('19-resultado.png')
        first_result=capture().crop((0,0,256,192)).tobytes()
        run(12)
        assert metric('sceneResultReactions')>0
        assert capture().crop((0,0,256,192)).tobytes()!=first_result,'Result sprites did not animate'
        run(24)
        assert metric('sceneCelebrations')>0 and metric('sceneTears')>0,'Result effects did not render'
        states['celebration']=shot('21-vitoria-e-lagrimas.png')
        capture().crop((0,192,256,384)).save(out/'27-menu-resultado.png')
        round_result=snapshot()
        assert round_result['duelRound']==1 and round_result['playerRounds']+round_result['rivalRounds']==1
        lighting_draws=metric('sceneRandomSelections')
        touch(128,116)
        assert snapshot()['state']==2 and snapshot()['introTicks']>60
        assert snapshot()['duelRound']==2
        assert all(snapshot()[k]==round_result[k] for k in ('playerRounds','rivalRounds','evolutionStage','evolutionTicks'))
        assert metric('sceneRandomSelections')==lighting_draws
        states['round_two']=shot('35-round-2.png')
        checks += ['animated outcome and next round by touch with score, form and lighting retained', 'music clock freezes during pause']
        wait_for_fight()
        start_x=snapshot()['playerX']
        held.add(6);run(18)
        assert snapshot()['playerX']<start_x and actor_pose(0)[2]==2
        assert actor_pose(1)[2]==6,'Opponent turned away from the player'
        states['backstep']=shot('20-recuo-de-frente.png')
        dash_x=snapshot()['playerX']
        held.add(9);run(4);held.remove(9)
        assert snapshot()['playerDashTicks']>0 and snapshot()['playerX']<dash_x
        assert actor_pose(0)[1:3]==(2,6),'Dash pose does not follow the chosen direction'
        dodge_x=snapshot()['playerX']
        held.add(10);run(4);held.clear()
        assert snapshot()['dodgeTimer']>0 and not snapshot()['playerDashTicks']
        assert snapshot()['playerX']<dodge_x,'Dodge did not follow the D-pad'
        assert actor_pose(0)[1]==1 and metric('sceneDodgeGhosts')>0
        assert metric('sceneImpactRings')>0
        checks.append('fighters face their target during retreat and partner movement, dash and dodge follow D-pad')
        checks.append('knockout tears, victory celebration, directional dodge ghosts and impact rings')
        held.add(7);run(240);held.remove(7)
        assert snapshot()['state']==2 and snapshot()['playerX']==arena_right*16
        states['wide_arena']=shot('28-arena-ampliada.png')
        held.add(5);run(132);held.remove(5)
        assert snapshot()['state']==2 and snapshot()['playerY']==arena_bottom*16
        states['arena_corner']=shot('34-borda-sudeste.png')
        checks.append('player crosses the open center and reaches the expanded right and bottom edges')
        max_vertices=metric('sceneMaxVertices');max_polygons=metric('sceneMaxPolygons');texture_errors=metric('sceneTextureFailures')
        print('Geometry metrics:',max_vertices,max_polygons,'texture errors:',texture_errors,flush=True)
        assert 0<max_vertices<=6144 and 0<max_polygons<=2048 and texture_errors==0,(max_vertices,max_polygons,texture_errors)
        checks.append('geometry and texture allocation within DS limits')
        assert audio_peak>0, 'No sound output from the ROM'
        checks.append('nonzero native audio output')
        assert metric('sceneUploadEndLine')<214,'Texture copy crossed into 3D rendering'
        assert metric('sceneTailLights')>0,'No fire tail lighting rendered at night'
        checks += ['texture uploads finish before 3D rendering', 'fire tail light at night']
        report = {'emulator':emulator, 'frames':frame_count,
                  'seconds_wall':round(time.monotonic()-started,3), 'rom_sha256':hashlib.sha256(rom_bytes).hexdigest(),
                  'audio_peak':audio_peak,'scene_max_vertices':max_vertices,'scene_max_polygons':max_polygons,
                  'texture_failures':texture_errors,'performance_samples':performance,
                  'texture_uploads':metric('sceneTextureUploads'),'draw_max_us':metric('graphicsMaxUsec'),
                  'match_periods':random_periods,'random_selections':metric('sceneRandomSelections'),
                  'upload_end_line':metric('sceneUploadEndLine'),'upload_deferrals':metric('sceneUploadLate'),
                  'tail_lights':metric('sceneTailLights'),
                  'ground_reaction_draws':metric('sceneGroundReactions'),
                  'music_notes':metric('musicNotes'),'result_reaction_draws':metric('sceneResultReactions'),
                  'ultimate_zooms':ultimate_zooms,
                  'attack_frame_samples':len(attack_brightness),'attack_mean_brightness_range':[min(attack_brightness),max(attack_brightness)],
                  'checks':checks, 'snapshots':states}
        (out/'resultado.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print('OK: '+', '.join(checks))
    finally:
        dll.retro_unload_game(); dll.retro_deinit()


if __name__ == '__main__': main()
