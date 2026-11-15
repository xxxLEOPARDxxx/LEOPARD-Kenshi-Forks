"""Build deterministic production-ring interleavings under Od and O2; no game."""
from pathlib import Path
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[2]
FIXTURE=ROOT/'tools/tests/splice_ring_delivery.cpp'


def main():
    out=ROOT/'build/tests/splice_ring_delivery'
    out.mkdir(parents=True,exist_ok=True)
    has_api='SpliceRingDrainRequired(' in (ROOT/'src/navmesh/construction/splice_ring.h').read_text(encoding='utf-8-sig')
    for opt in ('Od','O2'):
        exe=out/('delivery_'+opt+'.exe')
        command=['cl','/nologo','/EHsc','/'+opt,'/Isrc','/DKEO_RING_HAS_REQUIRED_DRAIN='+str(int(has_api)),
                 str(FIXTURE),'/Fo'+str(out)+'\\','/Fe'+str(exe)]
        result=subprocess.run(command,cwd=str(ROOT),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        print(result.stdout,end='')
        if result.returncode:return result.returncode
        result=subprocess.run([str(exe)],cwd=str(ROOT),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=25)
        print(opt+': '+result.stdout,end='')
        if result.returncode:return result.returncode
    return 0


class ProductionSpliceDelivery(unittest.TestCase):
    def test_production_interleavings_od_and_o2(self):
        self.assertEqual(main(),0)


if __name__=='__main__':unittest.main()
