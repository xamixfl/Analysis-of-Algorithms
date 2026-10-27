"""End-to-end checks of the compiled C++ program; Python is only a test runner."""
from pathlib import Path
import subprocess

exe = Path(__file__).resolve().parents[1] / "reverse_number.exe"
cases = 0


def run(mode, data, expected=None, code=0):
    global cases
    result = subprocess.run([str(exe), mode], input=data, text=True, capture_output=True)
    assert result.returncode == code, (mode, data, result)
    if expected is not None:
        assert result.stdout == expected, (mode, data, result.stdout)
    if code:
        assert result.stderr and not result.stdout
    cases += 1


for mode in ("iterative", "recursive"):
    for data, expected in (("179\n", "971\n"), ("9", "9\n"),
                           ("  12321  \n", "12321\n"),
                           ("9999999999999999999\n", "9999999999999999999\n")):
        run(mode, data, expected)
    for data in ("", " \n", "0", "10", "101", "-179", "+179", "1.5", "abc",
                 "12x", "12 34", "01", "10000000000000000000",
                 "18446744073709551615", "999999999999999999999999999999"):
        run(mode, data, code=1)
run("both", "179", "iterative: 971\nrecursive: 971\n")
run("unknown", "179", code=2)
print(f"PASS: {cases} CLI cases.")
