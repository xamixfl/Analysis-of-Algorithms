"""Independent exact-rational Otsu oracle and CLI/PGM checks (stdlib only)."""
from pathlib import Path
from fractions import Fraction
import random
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'segment.exe'
def oracle(a):
    h = [a.count(v) for v in range(256)]
    total = sum(v * h[v] for v in range(256))
    best, maximum, count, weighted = 0, Fraction(-1), 0, 0
    for t in range(255):
        count += h[t]
        weighted += t * h[t]
        if count == 0 or count == len(a): continue
        score = Fraction((len(a)*weighted-count*total)**2, count*(len(a)-count))
        if score > maximum: best, maximum = t, score
    if maximum < 0: best = a[0]
    return best, bytes(255 if v > best else 0 for v in a)
def run(*args, success=True):
    r = subprocess.run([str(EXE), *map(str,args)], capture_output=True, text=True)
    assert (r.returncode == 0) == success, (args, r.returncode, r.stdout, r.stderr)
    return r
rng = random.Random(20261112)
cases = [bytes([0]), bytes([255]), bytes([77])*15, bytes([0,255]),
         bytes([10,10,200,200]), bytes(range(256)), bytes([0,1,254,255])*7,
         bytes([0])*101+bytes([255]), bytes([0,2,4]), bytes([9,10,13,32,35,255])]
cases += [bytes(rng.randrange(256) for _ in range(n)) for n in [3,7,17,31,33,63,65,129,257,1009,4097]]
cases += [bytes(rng.choice([20,70,120,170,220]) for _ in range(511)) for _ in range(9)]
checked = 0
with tempfile.TemporaryDirectory(dir=ROOT / 'tests') as directory:
    tmp = Path(directory); inp, out = tmp/'in.pgm', tmp/'out.pgm'
    for index, a in enumerate(cases):
        inp.write_bytes(f'P5\n# case {index}\n{len(a)} 1\n255\n'.encode() + a)
        threshold, mask = oracle(a)
        for args in [('seq',), ('par',1), ('par',2), ('par',3), ('par',4), ('par',8), ('par',32)]:
            result = run('segment', *args, inp, out)
            got = int(re.search(r'threshold=(\d+)', result.stdout)[1])
            assert got == threshold, (index,args,threshold,got)
            assert out.read_bytes().split(b'\n',3)[3] == mask, (index,args)
            checked += 1
    for first in [9,10,13,32,35]:
        a = bytes([first,200]); inp.write_bytes(b'P5\r\n2 1\r\n255\r\n' + a)
        run('segment','seq',inp,out)
        assert out.read_bytes().split(b'\n',3)[3] == oracle(a)[1]
    bad = [b'', b'P2\n1 1\n255\n0', b'P5\n0 1\n255\n', b'P5\n-1 1\n255\n',
           b'P5\n1 1\n65535\n\0\0', b'P5\n2 1\n255\n\0',
           b'P5\n1 1\n255\n\0extra', b'P5\n9999999999999999999999999 1\n255\n',
           b'P5\n18446744073709551615 2\n255\n', b'P5\n1 1\n255',
           b'P5\n1 1\nabc\n\0', b'P5\n1 1\n0\n\0']
    for a in bad:
        inp.write_bytes(a); run('segment','seq',inp,out,success=False)
    inp.write_bytes(b'P5\n1 1\n255\n\0')
    for q in ['0','-1','1.5','abc','4097','999999999999999999999999']:
        run('segment','par',q,inp,out,success=False)
    run('segment','seq',tmp/'missing.pgm',out,success=False)
    run('segment','seq',inp,tmp/'missing'/'out.pgm',success=False)
    run(success=False)
print(f'Oracle: {len(cases)} images x 7 modes = {checked} exact threshold/mask checks: PASS')
print('PGM: 5 binary-raster/CRLF cases, 12 malformed inputs: PASS')
print('CLI: 6 invalid q, missing input, unwritable output, missing arguments: PASS')
