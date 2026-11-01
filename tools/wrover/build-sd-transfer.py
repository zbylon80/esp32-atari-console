"""Build a temporary USB library copier, restricted to the pinned catalog."""
import hashlib, importlib.util, json, shutil
from ota_common import ROOT, compile_sketch, artifact
HERE=ROOT/'tools/wrover'
WORK=ROOT/'.build/wrover-sd-transfer';OUT=WORK/'WroverSdTransfer'

def media_files():
    library=json.loads((HERE/'library.json').read_text(encoding='utf-8'))
    files=[]
    for game in library['games']:
        files.append(game)
        if game.get('disk2'):files.append(game['disk2'])
    assert len({f['file'] for f in files})==len(files)
    for f in files:
        assert f['file'].isascii() and '/' not in f['file'] and '\\' not in f['file'] and '..' not in f['file']
        data=(ROOT/f['path']).read_bytes()
        assert len(data)==f['bytes'] and hashlib.sha256(data).hexdigest()==f['sha256']
    return files

def main():
    files=media_files()
    spec=importlib.util.spec_from_file_location('av',HERE/'prepare-av.py')
    av=importlib.util.module_from_spec(spec);spec.loader.exec_module(av)
    OUT.mkdir(parents=True,exist_ok=True)
    for source,target in [('sd-transfer/WroverSdTransfer.ino','WroverSdTransfer.ino'),
                          ('updater/service-boot.cpp','service-boot.cpp'),
                          ('sd-storage.h','sd-storage.h'),('sd-storage.cpp','sd-storage.cpp'),
                          ('protect-nvs.cpp','protect-nvs.cpp'),('partitions-ota.csv','partitions.csv')]:
        shutil.copy2(HERE/source,OUT/target)
    rows=[json.dumps(f['file'])+','+str(f['bytes'])+','+json.dumps(f['sha256']) for f in files]
    (OUT/'transfer-catalog.h').write_text(
        'struct Media {const char* file;size_t bytes;const char* sha;};\n'
        'static const Media media[]={'+','.join('{'+r+'}' for r in rows)+'};\n'
        'static constexpr int media_count='+str(len(files))+';\n',encoding='ascii')
    manifest={'fqbn':av.FQBN,'firmware_version':'WroverSdTransfer-v1',
        'build_properties':{'upload.maximum_size':'2097152','compiler.c.elf.extra_flags':'-Wl,--wrap=esp_partition_erase_range','build.defines':'-DBOARD_HAS_PSRAM'},
        'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.iterdir() if p.is_file()}}
    (WORK/'av-source.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    compile_sketch('wrover-sd-transfer','WroverSdTransfer')
    a=artifact('sd-transfer');print('PASS SD transfer build:',a['bytes'],a['sha256'])
if __name__=='__main__':main()
