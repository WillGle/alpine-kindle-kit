#!/usr/bin/env python3
"""Build the stock-default staged installer bundle; no device command or boot arming."""
from pathlib import Path
import argparse
import hashlib
import shutil
import tarfile

linux=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--modules',type=Path,required=True,help='Exact captured stock-modules.tar')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
assert not args.output.exists(),'Keep existing stages/evidence'
args.output.mkdir(parents=True,mode=0o700)
for source,name in [(linux/'bootstrap/bootstrap-HOST-ONLY','bootstrap'),(linux/'bootstrap/trial-dispatch.in.sh','dispatcher.sh'),(linux/'bootstrap/install-default-trial.sh','install-default-trial.sh'),(linux/'bootstrap/restore-stock-init.sh','restore-stock-init.sh'),(linux/'bootstrap/return-control.conf','return-control.conf'),(linux/'pid1-handoff/gleworks_pid1_reset.ko','gleworks_pid1_reset.ko')]:
    assert source.is_file(),source
    shutil.copyfile(source,args.output/name)
with tarfile.open(args.modules) as tar:
    for name,rel in [('fsl_otg_arc','otg/fsl_otg_arc.ko'),('arcotg_udc','gadget/arcotg_udc.ko'),('g_file_storage','gadget/g_file_storage.ko')]:
        data=tar.extractfile('lib/modules/3.0.35-lab126/kernel/drivers/usb/'+rel).read()
        assert b'vermagic=3.0.35-lab126 preempt mod_unload ARMv7 ' in data
        (args.output/(name+'.ko')).write_bytes(data)
manifest=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in sorted(args.output.iterdir()))
(args.output/'SHA256SUMS').write_text(manifest)
print('Prepared stock-default entry bundle:',args.output)
print('Installer preserves stock init and does not create try.once or boot.enabled.')
