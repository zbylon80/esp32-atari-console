"""Store Wi-Fi details locally; never prints the password or OTA token."""
import getpass,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent/'wrover'))
from project import CONFIG
c=json.loads(CONFIG.read_text());c['ssid']=input('Nazwa sieci Wi-Fi 2,4 GHz: ').strip()
c['password']=getpass.getpass('Hasło Wi-Fi (ukryte): ')
if not c['ssid'] or not c['password']:raise SystemExit('Podaj nazwę i hasło sieci.')
CONFIG.write_text(json.dumps(c,indent=2));print('Zapisano local/config.json. Nie dodawaj tego pliku do Gita.')
