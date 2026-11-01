"""Restore the full Atari framebuffer/memory semantics in an isolated WROVER build."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,importlib.util,os
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
SOURCE=ROOT/'.build/upstream/esp_8_bit'
WORK=ROOT/'.build/wrover-dropzone-ota'
OUT=WORK/'WroverDropzone'
def sha(b):return hashlib.sha256(b).hexdigest()
def body(text,signature,replacement):
    start=text.index(signature);opening=text.index('{',start);level=1;end=opening+1
    while level:
        level+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[:opening+1]+'\n'+replacement+'\n'+text[end-1:]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--refresh',action='store_true')
    parser.add_argument('--variant',default='fullspeed-v3')
    parser.add_argument('--scanline',dest='scanline',action='store_true',default=True)
    parser.add_argument('--legacy-framebuffer',dest='scanline',action='store_false')
    parser.add_argument('--pc-pointer',dest='pc_pointer',action='store_true',default=True)
    parser.add_argument('--integer-pc',dest='pc_pointer',action='store_false');args=parser.parse_args()
    assert re.fullmatch(r'[a-zA-Z0-9_-]{1,31}',args.variant)
    previous={p:(p.read_bytes(),p.stat().st_atime,p.stat().st_mtime) for p in OUT.rglob('*') if p.is_file()} if OUT.exists() else {}
    from project import ensure_upstream
    ensure_upstream()
    if OUT.exists():assert args.refresh and (WORK/'av-source.json').is_file(),'Refusing to overwrite an unowned tree'
    shutil.copytree(SOURCE,OUT,ignore=shutil.ignore_patterns('esp_8_bit.ino','.vscode','data'),dirs_exist_ok=args.refresh)
    for p in (HERE/'dropzone').iterdir():
        shutil.copy2(p,OUT/p.name if p.suffix=='.ino' else OUT/'src'/p.name)
    shutil.copy2(HERE/'partitions-ota.csv',OUT/'partitions.csv')
    shutil.copy2(HERE/'protect-nvs.cpp',OUT/'src/protect-nvs.cpp')
    shutil.copy2(HERE/'perf-report.h',OUT/'src/perf-report.h')
    (OUT/'src/perf-config.h').write_text('#pragma once\n#define PERF_VARIANT '+json.dumps(args.variant)+'\n#define PERF_SCANLINE '+str(int(args.scanline))+'\n',encoding='ascii')
    for n in ('gui.cpp','feasibility_bt.cpp'):
        p=OUT/'src'/n
        if not p.exists():continue
        p.write_text('#if 0 // Replaced by WROVER runtime.\n'+p.read_text(encoding='utf-8')+'\n#endif\n',encoding='utf-8')
    p=OUT/'src/emu_atari800.cpp';t=p.read_text(encoding='utf-8')
    t=t.replace('#include "emu.h"','#include "emu.h"\n#include <esp_heap_caps.h>\n#include "dropzone.h"')
    t=body(t,'void init_screen()','        drop_memory_init(&_lines);')
    t=body(t,'void clear_screen()','        drop_clear();')
    t=body(t,'string get_cfg(const string& path)','        return "-xl -ntsc -nobasic \\\"" + path + "\\\"";'.replace('\\\\','\\'))
    t=body(t,'virtual int make_default_media(const string& path)','        return 0; // No extraction, mounts or formatting.')
    t=t.replace('        if (!_lines)\n            init_screen();','        if (!_lines) { init_screen(); if (!_lines) return -1; }')
    t=t.replace('        libatari800_init(argc,&argv[0]);\n        return 0;','        return libatari800_init(argc,&argv[0]) ? 0 : -2;')
    t=t.replace('atari_4_phase_ntsc_ram = new uint32_t[256];','atari_4_phase_ntsc_ram = (uint32_t*)heap_caps_malloc(256*4,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);\n            if(!atari_4_phase_ntsc_ram) abort();')
    p.write_text(t,encoding='utf-8')
    # The complete ANTIC and its full-frame overlays stay unmodified.
    p=OUT/'src/atari800/config.h';p.write_text('#define NO_SIMPLE_PAL_BLENDING 1\n'+p.read_text(encoding='utf-8'),encoding='utf-8')
    # Reuse the exact tested IRQ driver, not the half-height ANTIC pilot.
    shutil.copy2(HERE/'backend/video_out.h',OUT/'src/video_out.h')
    p=OUT/'src/emu.cpp';t=p.read_text(encoding='utf-8')
    t=body(t,'uint8_t* map_file(const char* path, int len)','    return nullptr; // No flash cartridge cache in the XEX checkpoint.')
    t=body(t,'FILE* mkfile(const char* path)','    return nullptr; // Read-only checkpoint.')
    p.write_text(t,encoding='utf-8')
    game=b'' # Game assets never enter the firmware preparation stage.
    gui=(SOURCE/'src/gui.cpp').read_text(encoding='utf-8')
    font=re.search(r'^const uint8_t _font\[2048\]\s*=\s*\{(.*?)\};',gui,re.M|re.S)
    assert font
    values=re.findall(r'0x[0-9a-fA-F]+',font[1]);assert len(values)==2048
    license=gui[:gui.index('#include')]
    (OUT/'src/drop-font.h').write_text(license+'\nconst uint8_t drop_font[2048]={'+','.join(values)+'};\n',encoding='utf-8')
    assert (OUT/'src/atari800/antic.c').read_bytes()==(SOURCE/'src/atari800/antic.c').read_bytes()
    if args.scanline:
        p=OUT/'src/atari800/antic.c';t=p.read_text(encoding='utf-8')
        assert t.count('scrn_ptr = (UWORD *) Screen_atari;')==1 and t.count('scrn_ptr += Screen_WIDTH / 2;')==3
        t='#include "../drop-scanline.h"\n'+t
        t=t.replace('scrn_ptr = (UWORD *) Screen_atari;','scrn_ptr = drop_scanline_begin(draw_display);')
        t=t.replace('scrn_ptr += Screen_WIDTH / 2;','scrn_ptr = drop_scanline_next();')
        p.write_text(t,encoding='utf-8')
        for name in ('atari.c','libatari800_main.c'):
            p=OUT/'src/atari800'/name;t=p.read_text(encoding='utf-8')
            match=re.search(r'(?m)^(\t+)INPUT_DrawMousePointer\(\);.*?^\t+Screen_Draw1200LED\(\);',t,re.S)
            assert match,name
            tabs=match[1];block=match[0]
            t=t[:match.start()]+tabs+'if (drop_overlay_begin()) {\n'+block+'\n'+tabs+'drop_overlay_end();\n'+tabs+'}'+t[match.end():]
            p.write_text('#include "../drop-scanline.h"\n'+t,encoding='utf-8')
    if args.pc_pointer:
        p=OUT/'src/atari800/cpu.c';p.write_text('#define PC_PTR\n#define WRAP_64K\n'+p.read_text(encoding='utf-8'),encoding='utf-8')
    spec=importlib.util.spec_from_file_location('av',HERE/'prepare-av.py');av=importlib.util.module_from_spec(spec);spec.loader.exec_module(av)
    for p,(old,atime,mtime) in previous.items():
        if p.exists() and p.read_bytes()==old:os.utime(p,(atime,mtime))
    manifest={'fqbn':av.FQBN,'firmware_version':'WroverDropzone-v5.0','variant':args.variant,'render_mode':'scanline' if args.scanline else 'legacy',
        'cpu_pc_pointer':args.pc_pointer,'benchmark_protocol':1,'application':'dropzone','video_standard':'ntsc',
        'build_properties':{'upload.maximum_size':'2097152','compiler.c.elf.extra_flags':'-Wl,--wrap=esp_partition_erase_range','build.defines':'-DBOARD_HAS_PSRAM'},
        'game_sha256':sha(game),'files':{p.relative_to(OUT).as_posix():sha(p.read_bytes()) for p in OUT.rglob('*') if p.is_file()}}
    (WORK/'av-source.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('Generated',len(manifest['files']),'files:',OUT)
if __name__=='__main__':main()
