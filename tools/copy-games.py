"""Copy user-supplied known game images to an attached FAT32 card reader."""
import argparse,hashlib,json,os,shutil,tempfile,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if hasattr(sys.stdout,'reconfigure'):sys.stdout.reconfigure(encoding='utf-8')
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--card',type=Path,required=True);p.add_argument('--verify-only',action='store_true');a=p.parse_args()
assert a.source.is_dir() and a.card.is_dir(),'Wskaż istniejące katalogi źródłowy i kartę.'
dest=a.card/'atari-console/games';library=json.loads((ROOT/'tools/wrover/library.json').read_text());count=0
for game in library['games']:
    for media in [game]+([game['disk2']] if game.get('disk2') else []):
        source=a.source/media['file'];target=dest/media['file']
        if not source.is_file():continue
        data=source.read_bytes()
        if len(data)!=media['bytes'] or hashlib.sha256(data).hexdigest()!=media['sha256']:
            print('POMINIĘTO: inna wersja pliku',media['file']);continue
        if target.exists():
            if hashlib.sha256(target.read_bytes()).hexdigest()!=media['sha256']:raise SystemExit('Nie nadpisuję istniejącego pliku: '+str(target))
        elif not a.verify_only:
            dest.mkdir(parents=True,exist_ok=True)
            fd,name=tempfile.mkstemp(prefix='.copy-',dir=dest)
            try:
                with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
                if target.exists():raise RuntimeError('Plik docelowy pojawił się podczas kopiowania')
                os.rename(name,target)
            finally:
                if Path(name).exists():Path(name).unlink()
        print('OK',media['file']);count+=1
print('Sprawdzono plików:',count,'; bez formatowania i bez pobierania gier.')
