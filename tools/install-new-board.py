"""Explicit first installation on a new board. Refuses an installed console."""
import argparse,subprocess,sys,time,re,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent/'wrover'))
from ota_common import artifact,ESPTOOL,LAYOUT,partitions,ROOT
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',required=True);p.add_argument('--confirm-mac',required=True)
p.add_argument('--initialize-new-board',action='store_true',required=True)
p.add_argument('--dry-run',action='store_true');a=p.parse_args()
game=artifact('archon');updater=artifact('updater')
base=[str(ESPTOOL),'--chip','esp32','--port',a.port,'--baud','115200']
def read_command(args):
    r=subprocess.run(base+args,capture_output=True);s=(r.stdout+r.stderr).decode(errors='replace')
    if r.returncode:raise SystemExit(s)
    return s
s=read_command(['read_mac']);mac=re.search(r'MAC: ([0-9a-f:]+)',s,re.I)
if not mac or mac[1].lower()!=a.confirm_mac.lower():raise SystemExit('MAC nie odpowiada potwierdzonej płytce.')
if not re.search(r'revision v[3-9]',s):raise SystemExit('Wymagany ESP32 ECO3 lub nowszy, zgodny z instrukcją.')
if '16MB' not in read_command(['flash_id']):raise SystemExit('Wymagana pamięć flash16MB.')
backup=ROOT/'.build/backups'/time.strftime('%Y%m%d-%H%M%S');backup.mkdir(parents=True)
prefix=backup/'prefix.bin';read_command(['read_flash','0x0','0x10000',str(prefix)])
if partitions(prefix.read_bytes()[0x8000:0x9000])==LAYOUT:
    raise SystemExit('To już zainstalowana konsolka. Użyj aktualizacji Wi-Fi; nie zmieniaj jej partycji.')
images=[('0x1000',game['build']/'WroverArchon.ino.bootloader.bin'),
        ('0x8000',game['build']/'WroverArchon.ino.partitions.bin'),
        ('0x10000',game['binary']),('0x210000',updater['binary'])]
if a.dry_run:
    print('DRY RUN: poprawna nowa płytka; planowane adresy:',[(o,p.name) for o,p in images]);sys.exit()
# Preserve all previous contents before the explicitly requested first install.
read_command(['read_flash','0x0','0x1000000',str(backup/'flash-before-install.bin')])
subprocess.run(base+['erase_region','0x810000','0x2000'],check=True)
command=base+['write_flash','--flash_mode','dio','--flash_freq','40m','--flash_size','16MB']
for offset,path in images:command.extend([offset,str(path)])
subprocess.run(command,check=True)
print('Pierwsza instalacja gotowa. Nie użyto erase_flash ani boot_app0 pod0xE000. Kopia:',backup)
