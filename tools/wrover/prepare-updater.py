"""Stage the separate Wi-Fi updater; credentials remain in ignored files."""
from pathlib import Path
import hashlib,json,shutil,importlib.util
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
WORK=ROOT/'.build/wrover-updater'
OUT=WORK/'WroverUpdater'
from project import CONFIG as PRIVATE

def main():
    from project import generate_av
    generate_av()
    credentials=json.loads(PRIVATE.read_text(encoding='utf-8'))
    assert credentials['ssid'] and credentials['password'] and len(credentials['token'])==64
    OUT.mkdir(parents=True,exist_ok=True)
    for p in (HERE/'updater').iterdir():shutil.copy2(p,OUT/p.name)
    for name in ('video_out.h','test-palettes.h'):shutil.copy2(ROOT/'.build/generated-av'/name,OUT/name)
    shutil.copy2(ROOT/'.build/generated-av/drop-font.h',OUT/'drop-font.h')
    shutil.copy2(HERE/'partitions-ota.csv',OUT/'partitions.csv')
    shutil.copy2(HERE/'protect-nvs.cpp',OUT/'protect-nvs.cpp')
    shutil.copy2(HERE/'perf-report.h',OUT/'perf-report.h')
    header='#pragma once\n'+''.join('#define '+macro+' '+json.dumps(credentials[key],ensure_ascii=True)+'\n'
        for macro,key in [('WIFI_SSID','ssid'),('WIFI_PASSWORD','password'),('OTA_TOKEN','token')])
    (OUT/'private-network.h').write_text(header,encoding='ascii')
    spec=importlib.util.spec_from_file_location('av',HERE/'prepare-av.py');av=importlib.util.module_from_spec(spec);spec.loader.exec_module(av)
    manifest={'fqbn':av.FQBN,'firmware_version':'WroverUpdater-v2',
        'build_properties':{'upload.maximum_size':'2097152','compiler.c.elf.extra_flags':'-Wl,--wrap=esp_partition_erase_range','build.defines':'-DBOARD_HAS_PSRAM'},
        'files':{p.relative_to(OUT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.rglob('*') if p.is_file()}}
    (WORK/'av-source.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('Staged updater; credentials omitted from output')
if __name__=='__main__':main()
