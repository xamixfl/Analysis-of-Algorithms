"""Run the lab executable and decode its CSV output."""
import csv
import subprocess

from graphs import ROOT

EXE = ROOT / "lab6.exe"


def solve(path, mode="both", extra=()):
    run = subprocess.run([str(EXE), str(path), mode, *map(str, extra)], check=True,
                         capture_output=True, text=True)
    return list(csv.DictReader(run.stdout.splitlines()))
