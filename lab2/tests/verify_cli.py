from pathlib import Path
import subprocess, csv, json
root=Path(__file__).resolve().parents[1]
exe=str(root/'matrix_lab.exe')
checks=[]
r=subprocess.run([exe,'--test'],capture_output=True)
assert r.returncode==0,r.stderr
(root/'results/tests.txt').write_bytes(r.stdout)
checks.append(r.stdout.decode().strip())
for algorithm in [1,2,3]:
    test=f'1\n2 3 2\n1 2 3 4 5 6\n7 8 9 10 11 12\n{algorithm}\n0\n'
    result=subprocess.run([exe],input=test.encode(),capture_output=True)
    assert result.returncode==0,result.stderr
    assert b'58 64 \r\n139 154' in result.stdout,result.stdout
checks.append('PASS: manual rectangular multiplication in all three algorithms')
r=subprocess.run([exe],input=b'abc\n1.5\n9999999999999999999999999\n-1\n0\n',capture_output=True)
assert r.returncode==0
assert r.stdout.decode('utf-8').count('Введите целое число')==4
checks.append('PASS: invalid token, fractional token, integer overflow and invalid menu option are rejected')
# n=2000, m=q=1 stresses the stated maximum shared dimension without huge cubic work.
for algorithm in [1,2,3]:
    test='1\n1 2000 1\n'+'1000000 '*2000+'\n'+'1000000 '*2000+f'\n{algorithm}\n0\n'
    r=subprocess.run([exe],input=test.encode(),capture_output=True)
    assert r.returncode==0 and b'2000000000000000 ' in r.stdout
checks.append('PASS: n=2000 and elements=1000000 produce 2000000000000000 in all algorithms')
for filename in ['raw.csv','summary.csv']:
    rows=list(csv.DictReader((root/'results'/filename).open()))
    assert len(rows)==(240 if filename=='raw.csv' else 48)
    for row in rows:
        assert int(row['k'])>=10
        if filename=='raw.csv':
            assert float(row['total_cpu_s'])>0
            assert abs(float(row['mean_ms'])-float(row['total_cpu_s'])*1000/int(row['k']))<1e-7
    checks.append(f'PASS: {filename}: {len(rows)} rows, K>=10, positive CPU times')
raw=list(csv.DictReader((root/'results/raw.csv').open()))
summary=list(csv.DictReader((root/'results/summary.csv').open()))
for row in summary:
    samples=sorted(float(x['mean_ms']) for x in raw if x['n']==row['n'] and x['algorithm']==row['algorithm'])
    assert len(samples)==5
    assert abs(samples[2]-float(row['median_ms']))<1e-8
checks.append('PASS: all CSV medians match the raw five-series measurements')
(root/'results/verification.txt').write_text('\n'.join(checks)+'\n',encoding='utf-8')
print('\n'.join(checks))
