"""Local configuration and pinned, source-only dependency preparation."""
from pathlib import Path
import concurrent.futures, hashlib, json, os, re, secrets, shutil, sys, urllib.request
ROOT=Path(__file__).resolve().parents[2]
if hasattr(sys.stdout,'reconfigure'):sys.stdout.reconfigure(encoding='utf-8')
CONFIG=ROOT/'local/config.json'
DATA=Path(os.environ.get('ARDUINO_DATA_DIR',str(Path.home()/'AppData/Local/Arduino15')))
SDK=DATA/'packages/esp32-bluepad32/hardware/esp32/4.1.0/tools/sdk/esp32'
CLI=Path(os.environ.get('ARDUINO_CLI',shutil.which('arduino-cli') or str(Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe')))
PYTHON=Path(sys.executable)
ESPTOOL=DATA/'packages/esp32/tools/esptool_py/4.5.1/esptool.exe'
NM=DATA/'packages/esp32/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/bin/xtensa-esp32-elf-nm.exe'
ARDUINO_CONFIG=ROOT/'.tools/arduino-cli.yaml'
def prepare_config():
    ARDUINO_CONFIG.parent.mkdir(exist_ok=True)
    lines=['board_manager:','  additional_urls:',
           '    - https://espressif.github.io/arduino-esp32/package_esp32_index.json',
           '    - https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json',
           'directories:']
    for k,p in [('data',DATA),('downloads',ROOT/'.tools/downloads'),('user',ROOT/'.tools/sketchbook')]:
        lines.append('  '+k+': '+json.dumps(p.as_posix()))
    ARDUINO_CONFIG.write_text('\n'.join(lines)+'\n')
    if not CONFIG.exists():
        CONFIG.parent.mkdir(exist_ok=True)
        CONFIG.write_text(json.dumps({'ssid':'CONFIGURE_ME','password':'CONFIGURE_ME','token':secrets.token_hex(32)},indent=2))
def ensure_upstream():
    lock=json.loads((ROOT/'tools/wrover/upstream-lock.json').read_text())
    dest=ROOT/'.build/upstream/esp_8_bit'
    def one(item):
        path=dest/item['path']
        if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']:return
        url='https://raw.githubusercontent.com/'+lock['repository']+'/'+lock['commit']+'/'+item['path']
        data=urllib.request.urlopen(url,timeout=45).read()
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise RuntimeError('Dependency SHA256 mismatch: '+item['path'])
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:list(pool.map(one,lock['files']))
    # Upstream media.h contains bundled games, so it is never downloaded.
    (dest/'src/media.h').write_text('// No bundled games or commercial ROM images.\n')
    return dest
def generate_av():
    source=ensure_upstream();out=ROOT/'.build/generated-av';out.mkdir(parents=True,exist_ok=True)
    shutil.copy2(ROOT/'tools/wrover/backend/video_out.h',out/'video_out.h')
    gui=(source/'src/gui.cpp').read_text()
    m=re.search(r'^const uint8_t _font\[2048\]\s*=\s*\{(.*?)\};',gui,re.M|re.S)
    values=re.findall(r'0x[0-9a-fA-F]+',m[1]);assert len(values)==2048
    (out/'drop-font.h').write_text(gui[:gui.index('#include')]+'\nconst uint8_t drop_font[2048]={'+','.join(values)+'};\n')
    text=(source/'src/emu_atari800.cpp').read_text();parts=[text[:text.index('#include')]]
    for old,new,count in [('atari_4_phase_pal','test_pal',512),('atari_4_phase_ntsc','test_ntsc',256),('atari_palette_rgb','test_rgb',256)]:
        m=re.search(r'^const uint32_t '+old+r'\[[^]]*\]\s*=\s*\{(.*?)\};',text,re.M|re.S)
        values=re.findall(r'0x[0-9a-fA-F]+',m[1]);assert len(values)==count
        parts.append('const uint32_t '+new+'['+str(count)+']={'+','.join(values)+'};\n')
    (out/'test-palettes.h').write_text(''.join(parts))
prepare_config()
