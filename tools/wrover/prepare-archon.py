"""Isolated ESP32 Archon/two-pad build derived from the accepted fast backend."""
from pathlib import Path
import hashlib,json,shutil,sys,importlib.util,os,re,subprocess
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
WORK=ROOT/'.build/wrover-archon';OUT=WORK/'WroverArchon'
def main():
    spec=importlib.util.spec_from_file_location('base',HERE/'prepare-dropzone.py');base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
    base.WORK=WORK;base.OUT=OUT
    sys.argv=['prepare-dropzone.py','--refresh','--variant','console-v1-14-0-pad-management'];base.main()
    # Keep scanline scheduling and ordinary ANTIC rasterizers in internal
    # instruction RAM. No pixel/collision/CPU semantics are changed.
    p=OUT/'src/atari800/antic.c';t=p.read_text(encoding='utf-8')
    names=['ANTIC_Frame']+['draw_antic_'+mode for mode in ('2','4','6','8','9','a','c','e','f')]
    for name in names:
        pattern=r'(?m)^((?:static )?void) '+name+r'\('
        t,n=re.subn(pattern,r'\1 IRAM_ATTR '+name+'(',t);assert n>=1,name
    p.write_text('#include "esp_attr.h"\n'+t,encoding='utf-8')
    p=OUT/'src/atari800/gtia.c';t=p.read_text(encoding='utf-8');assert t.count('void GTIA_NewPmScanline(void)')==1
    p.write_text('#include "esp_attr.h"\n'+t.replace('void GTIA_NewPmScanline(void)','void IRAM_ATTR GTIA_NewPmScanline(void)'),encoding='utf-8')
    shutil.copy2(HERE/'perf-report.h',OUT/'src/perf-report.h')
    (OUT/'WroverDropzone.ino').replace(OUT/'WroverArchon.ino')
    for p in (HERE/'archon').iterdir():shutil.copy2(p,OUT/'src'/p.name)
    for name in ('sd-storage.cpp','sd-storage.h'):shutil.copy2(HERE/name,OUT/'src'/name)
    # User owns gameplay testing. Production never runs automatic game probes.
    (OUT/'src/library-smoke.cpp').write_text('#include "library-smoke.h"\nint library_test_begin(){return -1;}\nbool library_test_active(){return false;}\nvoid library_test_input(){}\nvoid library_test_frame(uint32_t){}\nvoid library_test_fail(){}\n')
    library=json.loads((HERE/'library.json').read_text(encoding='utf-8'))
    source=['#include <Arduino.h>','#include "game-catalog.h"'];entries=[];media=[]
    for index,item in enumerate(library['games']):
        data=range(item['bytes']) # Length metadata only; no game file required
        symbol='game_asset_'+str(index)
        fields=[json.dumps(item[k]) for k in ('id','title','file','hint')]
        second=item.get('disk2')
        help_lines=item['help']
        assert 1<=len(help_lines)<=10 and all(len(line)<=38 and line.isascii() for line in help_lines),item['id']+' help layout'
        about=item['about']
        assert 1<=len(about)<=10 and all(len(line)<=38 and line.isascii() for line in about),item['id']+' about layout'
        entries.append('{'+','.join(fields)+',GameProfile::'+item['profile']+','+str(item['cartridge_type'])+',nullptr,'+str(len(data))+','+json.dumps(item['players'])+','+(json.dumps(second['file']) if second else 'nullptr')+','+('true' if item.get('original_os') else 'false')+','+json.dumps('\n'.join(help_lines))+','+json.dumps('\n'.join(about))+'}')
        media.append('{'+json.dumps(item['file'])+',nullptr,'+str(len(data))+','+json.dumps(item['sha256'])+'}')
        if second:
            raw=range(second['bytes'])
            media.append('{'+json.dumps(second['file'])+',nullptr,'+str(len(raw))+','+json.dumps(second['sha256'])+'}')
    source.append('const GameInfo console_games[]={'+','.join(entries)+'};')
    source.append('const int console_game_count='+str(len(entries))+';')
    source.append('const GameMedia console_media[]={'+','.join(media)+'};')
    source.append('const int console_media_count='+str(len(media))+';')
    (OUT/'src/library-assets.cpp').write_text('\n'.join(source)+'\n',encoding='ascii')
    bios_path=ROOT/library['bios']['path']
    has_original=bios_path.is_file()
    if has_original:
        bios=bios_path.read_bytes();assert hashlib.sha256(bios).hexdigest()==library['bios']['sha256'],'Original ROM hash mismatch'
        (OUT/'src/atari800/console_os.h').write_text('static const unsigned char console_os_rev2[]={'+','.join(str(v) for v in bios)+'};')
        p=OUT/'src/atari800/sysrom.c';t=p.read_text();old='{ osbb01r2_filename, 0x4000, 0x1f9cd270, NULL, TRUE }';assert old in t
        p.write_text('#include "console_os.h"\n'+t.replace(old,'{ osbb01r2_filename, 0x4000, 0x1f9cd270, console_os_rev2, TRUE }'))
    (OUT/'src/console-build-config.h').write_text('#pragma once\n#define CONSOLE_HAS_ORIGINAL_OS '+str(int(has_original))+'\n')
    game=b''
    p=OUT/'src/emu_atari800.cpp';t='extern bool library_original_os(const char*);\n'+p.read_text(encoding='utf-8')
    t=t.replace('#include "atari800/memory.h"','#include "atari800/memory.h"\n#include "atari800/sio.h"\n#include "atari800/binload.h"')
    t=t.replace('        CARTRIDGE_Remove();','        SIO_Exit(); // Detach previous disk and in-progress XEX handles.\n        if(BINLOAD_bin_file){fclose(BINLOAD_bin_file);BINLOAD_bin_file=nullptr;}\n        BINLOAD_start_binloading=BINLOAD_loading_basic=BINLOAD_wait_active=BINLOAD_pause_loading=0;\n        CARTRIDGE_Remove();')
    t=base.body(t,'string get_cfg(const string& path)','        if(path==":basic:")return "-xl -ntsc -basic -xl-rev altirra -basic-rev altirra";\n        return "-xl -ntsc -nobasic -xl-rev " + string(library_original_os(path.c_str())?"2 ":"altirra ") + string(get_ext(path)=="car"?"-cart ":get_ext(path)=="atr"?"":"-run ") + "\\"" + path + "\\"";')
    p.write_text(t,encoding='utf-8')
    p=OUT/'src/emu.cpp';t=p.read_text(encoding='utf-8')
    t='extern "C" unsigned char* library_map_file(const char*,int);\n'+t
    t=base.body(t,'uint8_t* map_file(const char* path, int len)','    return library_map_file(path,len);')
    p.write_text(t,encoding='utf-8')
    p=OUT/'src/framebuffer.cpp';t=p.read_text(encoding='utf-8').replace('PAUZA - DROPZONE','PAUZA - ARCHON');p.write_text(t,encoding='utf-8')
    p=OUT/'src/video_out.h';t=p.read_text(encoding='utf-8')
    t=t.replace('void IRAM_ATTR video_isr(volatile void* vbuf)', 'volatile uint32_t _diag_isr_calls=0,_diag_isr_gap_max=0,_diag_isr_late=0;\nvoid IRAM_ATTR video_isr(volatile void* vbuf)',1)
    t=t.replace('    ISR_BEGIN();','    static uint32_t lastTick;uint32_t nowTick=cpu_ticks();\n    if(lastTick){uint32_t gap=nowTick-lastTick;if(gap>_diag_isr_gap_max)_diag_isr_gap_max=gap;if(gap>240*100)++_diag_isr_late;}\n    lastTick=nowTick;++_diag_isr_calls;\n    ISR_BEGIN();',1)
    p.write_text(t,encoding='utf-8')
    p=OUT/'src/framebuffer.cpp';t=p.read_text(encoding='utf-8')
    t=t.replace('static bool menuSaved;','static bool menuSaved;\nextern volatile int _line_counter;\nvolatile uint32_t archon_lead_min=262,archon_crossings=0;')
    t=t.replace('    if(scanY<240){\n        auto dst=', '    if(scanY<240){\n        unsigned lead=(scanY-(_line_counter%262)+262)%262;\n        if(lead<archon_lead_min)archon_lead_min=lead;if(lead<=3)++archon_crossings;\n        auto dst=',1)
    p.write_text(t,encoding='utf-8')
    (OUT/'src/performance.cpp').write_text('// Per-game library smoke tests use library-smoke.cpp.\n',encoding='ascii')
    # Preserve the fast renderer's IRAM budget. The pinned SPI HAL object is
    # byte-identical to the SDK's; linking it directly avoids the archive-name
    # linker rule that otherwise puts it in IRAM. SD uses non-IRAM interrupts
    # during the boot display, then is unmounted before gameplay. The video
    # ISR stays in IRAM and the app makes no flash writes while reading SD.
    # No installed SDK files or function instructions are changed.
    from ota_common import NM
    ar=NM.with_name('xtensa-esp32-elf-ar.exe')
    hal=__import__('project').SDK/'lib/libhal.a'
    spi_object=OUT/'console-sd-spi-hal.o'
    extract=WORK/'sdk-object';extract.mkdir(exist_ok=True)
    # ar's stdout uses Windows text mode; extract to a binary file instead.
    subprocess.run([str(ar),'x',str(hal),'spi_hal_iram.c.obj'],cwd=extract,check=True)
    shutil.copy2(extract/'spi_hal_iram.c.obj',spi_object)
    manifest=json.loads((WORK/'av-source.json').read_text())
    manifest['build_properties']['compiler.c.elf.extra_flags']+=' "'+str(spi_object)+'"'
    manifest.update({'firmware_version':'WroverArchon-v1.14.0-pad-management','application':'archon','media_storage':'microSD-demand-cache-v1','boot_video':True,'basic_fallback':True,'sd_clock_khz':4000,'sd_spi_hal_sha256':hashlib.sha256(spi_object.read_bytes()).hexdigest(),'benchmark_protocol':0,'iram_renderer_functions':names+['GTIA_NewPmScanline'],
        'library':library,'game_sha256':hashlib.sha256(game).hexdigest(),'files':{p.relative_to(OUT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.rglob('*') if p.is_file()}})
    (WORK/'av-source.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('Archon CART/two-player sketch staged separately')
if __name__=='__main__':main()
