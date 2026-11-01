"""Authenticated update of an already provisioned console in Wi-Fi update mode."""
import argparse,hashlib,http.client,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent/'wrover'))
from project import CONFIG
from ota_common import artifact
p=argparse.ArgumentParser();p.add_argument('--host',required=True);a=p.parse_args()
image=artifact('archon');settings=json.loads(CONFIG.read_text())
c=http.client.HTTPConnection(a.host,80,timeout=120)
c.request('GET','/status');r=c.getresponse();status=json.loads(r.read())
assert r.status==200 and status['service']=='WroverUpdater-v2','Nieprawidłowy aktualizator';c.close()
c=http.client.HTTPConnection(a.host,80,timeout=120)
c.request('POST','/update',image['binary'].read_bytes(),{'Authorization':'Bearer '+settings['token'],'Content-Type':'application/octet-stream','X-Image-SHA256':image['sha256']})
r=c.getresponse();body=r.read();c.close();assert r.status==200 and json.loads(body).get('result')=='OK'
print('Wgrano i sprawdzono SHA256. Konsolka uruchamia się ponownie.')
