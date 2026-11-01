"""Build the console and separate updater; never uploads."""
import subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent/'wrover';sys.path.insert(0,str(HERE))
from project import CLI,NM,SDK,ROOT
from ota_common import compile_sketch,artifact
for p in (CLI,NM,SDK):
    if not p.exists():raise SystemExit('Brak narzędzia/pakietu: '+str(p)+'; wykonaj kroki w docs/getting-started.md')
for script,folder,name,kind in [('prepare-archon.py','wrover-archon','WroverArchon','archon'),('prepare-updater.py','wrover-updater','WroverUpdater','updater')]:
    subprocess.run([sys.executable,str(HERE/script)],cwd=ROOT,check=True)
    compile_sketch(folder,name);a=artifact(kind)
    print('PASS',kind,a['bytes'],'bytes',a['sha256'])
print('Gotowe. Nie wgrano niczego na płytkę.')
