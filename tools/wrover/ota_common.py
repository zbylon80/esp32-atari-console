"""Shared artifact validation for USB migration and subsequent network updates."""
from pathlib import Path
import hashlib,json,struct,subprocess,re
ROOT=Path(__file__).resolve().parents[2]
from project import CLI, PYTHON, ESPTOOL, NM, CONFIG as PRIVATE
MAC='' # Board selection is explicit; no developer device identity.
LAYOUT=[('nvs',1,2,0x9000,0x6000),('phy_init',1,1,0xf000,0x1000),
    ('factory',0,0,0x10000,0x200000),('updater',0,16,0x210000,0x200000),
    ('game_a',0,17,0x410000,0x200000),('game_b',0,18,0x610000,0x200000),('otadata',1,0,0x810000,0x2000)]
def sha(data):return hashlib.sha256(data).hexdigest()
def partitions(data):
    out=[]
    for pos in range(0,len(data)-31,32):
        magic,kind,sub,off,size,label,flags=struct.unpack_from('<HBBII16sI',data,pos)
        if magic!=0x50aa:break
        out.append((label.rstrip(b'\0').decode(),kind,sub,off,size))
    return out
def artifact(kind):
    name,folder={'game':('WroverDropzone','wrover-dropzone-ota'),'updater':('WroverUpdater','wrover-updater'),
                 'installer':('WroverServiceInstaller','wrover-service-installer'),'archon':('WroverArchon','wrover-archon'),
                 'sd-probe':('WroverSdProbe','wrover-sd-probe'),
                 'sd-transfer':('WroverSdTransfer','wrover-sd-transfer')}[kind]
    work=ROOT/'.build'/folder;build=work/'av-build'
    manifest=json.loads((work/'av-source.json').read_text(encoding='utf-8'))
    options=json.loads((build/'build.options.json').read_text(encoding='utf-8'))
    assert options['fqbn']==manifest['fqbn'],'FQBN differs'
    properties=options['customBuildProperties']
    for k,v in manifest['build_properties'].items():assert k+'='+v in properties,k
    latest=0
    for path,digest in manifest['files'].items():
        p=work/name/path;assert sha(p.read_bytes())==digest,'Staged source changed: '+path
        latest=max(latest,p.stat().st_mtime)
    binary=build/(name+'.ino.bin');assert binary.stat().st_mtime>=latest,'Stale binary'
    data=binary.read_bytes();assert 1024<len(data)<=0x200000 and data[0]==0xe9
    assert partitions((build/(name+'.ino.partitions.bin')).read_bytes())==LAYOUT,'Unsafe partition layout'
    symbols=subprocess.check_output([str(NM),str(build/(name+'.ino.elf'))]).decode()
    assert '__wrap_esp_partition_erase_range' in symbols,'NVS erase guard absent'
    for symbol in manifest.get('iram_renderer_functions',[]):
        match=re.search(r'^([0-9a-fA-F]+)\s+\w\s+'+re.escape(symbol)+r'[ \t\r]*$',symbols,re.M)
        assert match and 0x40080000<=int(match[1],16)<0x400a0000,'Renderer escaped IRAM: '+symbol
    if manifest.get('media_storage') in ('microSD-PSRAM-cache-v1','microSD-demand-cache-v1'):
        assert 'game_asset_' not in symbols,'Game image embedded in firmware'
        for symbol in ('spi_hal_setup_device','spi_hal_setup_trans','spi_hal_prepare_data'):
            match=re.search(r'^([0-9a-fA-F]+)\s+\w\s+'+symbol+r'[ \t\r]*$',symbols,re.M)
            assert match and 0x400d0000<=int(match[1],16)<0x40400000,'SD-only HAL must stay in flash: '+symbol
    if kind in ('game','archon'):
        assert b'ATARI-WROVER-GAME-OTA-v1' in data,'Game compatibility marker absent'
        assert ' esp_wifi_init' not in symbols and ' httpd_start' not in symbols,'Network code linked into game'
    elif kind=='updater':
        assert ' esp_wifi_init' in symbols and 'uni_init' not in symbols,'Updater must have Wi-Fi and no gamepad stack'
    else:
        assert b'ATARI-WROVER-GAME-OTA-v1' in data and 'uni_init' not in symbols and ' esp_wifi_init' not in symbols
    return {'kind':kind,'name':name,'work':work,'build':build,'binary':binary,'bytes':len(data),'sha256':sha(data),'manifest':manifest}
def evidence(item):
    return {k:(str(v.relative_to(ROOT)) if isinstance(v,Path) else v) for k,v in item.items() if k!='manifest'}
def compile_sketch(folder,name):
    work=ROOT/'.build'/folder;meta=json.loads((work/'av-source.json').read_text(encoding='utf-8'))
    args=[str(CLI),'compile','--config-file',str(__import__('project').ARDUINO_CONFIG),'--fqbn',meta['fqbn'],
        '--build-path',str(work/'av-build')]
    if name in ('WroverDropzone','WroverArchon'):args+=['--library',str(ROOT)]
    for k,v in meta['build_properties'].items():args+=['--build-property',k+'='+v]
    subprocess.run(args+[str(work/name)],cwd=ROOT,check=True)
