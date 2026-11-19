"""Verify that saved measurements describe the delivered implementation."""
from pathlib import Path
import csv
import hashlib
import json
import math
import statistics
ROOT=Path(__file__).resolve().parents[1]
env=json.loads((ROOT/'results/environment.json').read_text(encoding='utf-8'))
for name,expected in env['source_sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==expected,name
assert hashlib.sha256((ROOT/'segment.exe').read_bytes()).hexdigest()==env['executable_sha256']
with (ROOT/'results/raw.csv').open(newline='') as f:
    raw=list(csv.DictReader(f))
with (ROOT/'results/summary.csv').open(newline='') as f:
    summary=list(csv.DictReader(f))
assert len(raw)==3500 and len(summary)==35
for row in summary:
    group=[r for r in raw if (r['side'],r['q'])==(row['side'],row['q'])]
    assert len(group)==100 and {int(r['repeat']) for r in group}==set(range(100))
    assert len({(r['threshold'],r['checksum']) for r in group})==1
    assert math.isclose(statistics.mean(float(r['seconds'])*1000 for r in group),float(row['mean_ms']),rel_tol=1e-12)
for side in {r['side'] for r in raw}:
    group=[r for r in raw if r['side']==side]
    assert {int(r['q']) for r in group}=={0,*env['q']}
    assert len({(r['threshold'],r['checksum']) for r in group})==1
result='PASS: measured source/executable hashes, 3500 records, 35 complete groups, recomputed means, identical masks/checksums.\n'
(ROOT/'results/artifact-verification.txt').write_text(result,encoding='utf-8')
print(result,end='')
