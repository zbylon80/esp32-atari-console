"""Build/validate Archon separately; never uploads or changes the Dropzone build."""
import subprocess,sys
from ota_common import ROOT,compile_sketch,artifact
subprocess.run([sys.executable,str(ROOT/'tools/wrover/prepare-archon.py')],cwd=ROOT,check=True)
compile_sketch('wrover-archon','WroverArchon')
a=artifact('archon');print('PASS Archon fresh build:',a['bytes'],'bytes',a['sha256'])
