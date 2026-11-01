"""Read board identity and flash size; does not write flash."""
import argparse,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent/'wrover'))
from project import ESPTOOL
p=argparse.ArgumentParser();p.add_argument('--port',required=True);a=p.parse_args()
for command in ('read_mac','flash_id'):
    subprocess.run([str(ESPTOOL),'--chip','esp32','--port',a.port,command],check=True)
