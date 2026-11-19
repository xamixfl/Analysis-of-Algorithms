from pathlib import Path
import ctypes as c
import datetime as dt
import hashlib
import json
import os
import platform
import subprocess
import winreg
ROOT = Path(__file__).resolve().parents[1]
class Memory(c.Structure):
    _fields_ = [('length', c.c_ulong), ('load', c.c_ulong)] + [(n,c.c_ulonglong) for n in
        ['total_physical','available_physical','total_pagefile','available_pagefile','total_virtual','available_virtual','extended']]
class Processor(c.Structure):
    _fields_ = [('mask',c.c_size_t),('relationship',c.c_int),('data',c.c_ulonglong*2)]
memory = Memory(); memory.length = c.sizeof(memory)
assert c.windll.kernel32.GlobalMemoryStatusEx(c.byref(memory))
length = c.c_ulong()
c.windll.kernel32.GetLogicalProcessorInformation(None,c.byref(length))
buffer = c.create_string_buffer(length.value)
assert c.windll.kernel32.GetLogicalProcessorInformation(buffer,c.byref(length))
processors = (Processor*(length.value//c.sizeof(Processor))).from_buffer(buffer)
cores = sum(p.relationship==0 for p in processors)
with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,r'HARDWARE\DESCRIPTION\System\CentralProcessor\0') as key:
    name = winreg.QueryValueEx(key,'ProcessorNameString')[0].strip()
k = os.cpu_count()
qs, q = [], 1
while q <= 8*k:
    qs.append(q); q *= 2
if qs[-1] != 8*k: qs.append(8*k)
info = {
 'measured_local_time':dt.datetime.now(dt.timezone(dt.timedelta(hours=3))).isoformat(),
 'cpu':name,'logical_processors':os.cpu_count(),'physical_cores':cores,
 'smt':'enabled' if os.cpu_count()>cores else 'not detected',
 'ram_bytes':memory.total_physical,'available_ram_bytes':memory.available_physical,
 'os':platform.platform(),'compiler':subprocess.check_output(['gcc','--version'],text=True).splitlines()[0],
 'flags':'-std=c11 -O0 -Wall -Wextra -Wpedantic -pthread',
 'timer':'QueryPerformanceCounter, elapsed wall time',
 'q':qs, 'repetitions':100,'seed':20261112,
 'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'src').glob('*'))},
 'executable_sha256':hashlib.sha256((ROOT/'segment.exe').read_bytes()).hexdigest(),
 'conditions':'Normal process priority; no affinity pinning; SMT and OS settings unchanged; concurrent desktop background tasks possible.'
}
(ROOT/'results/environment.json').write_text(json.dumps(info,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(info,ensure_ascii=False,indent=2))
