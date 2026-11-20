import csv
import os
import random
import subprocess
from fractions import Fraction
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / ("lab5.exe" if os.name == "nt" else "lab5")
STAGES = ("read", "segment", "write")


def reference(pixels):
    # Recompute the exact between-class variance for each candidate.
    candidates = []
    histogram = [pixels.count(v) for v in range(256)]
    for t in range(255):
        left = sum(histogram[:t + 1])
        right = len(pixels) - left
        if left and right:
            mean_left = Fraction(sum(v * histogram[v] for v in range(t + 1)), left)
            mean_right = Fraction(sum(v * histogram[v] for v in range(t + 1, 256)), right)
            candidates.append((left * right * (mean_left - mean_right) ** 2, -t))
    threshold = -max(candidates)[1] if candidates else pixels[0]
    return threshold, bytes(255 if value > threshold else 0 for value in pixels)


def pgm(width, height, pixels):
    return f"P5\n{width} {height}\n255\n".encode() + pixels


def run(directory, mode, out, inputs, expected_return):
    result = subprocess.run([str(EXE), mode, out, *inputs], cwd=directory,
                            capture_output=True, text=True, timeout=15)
    assert result.returncode == expected_return, (result.stdout, result.stderr)
    return result


def verify_batch(directory, names, expected, failed):
    for mode in ("seq", "pipe"):
        output = directory / mode
        result = run(directory, mode, mode, names, int(bool(failed)))
        assert f"successful={len(names) - failed} " in result.stdout
        elapsed = float(result.stdout.split("elapsed_ms=")[1])
        with (output / "trace.csv").open(newline="") as file:
            rows = list(csv.DictReader(file))
        assert len(rows) == len(names)
        last_stage_end = dict.fromkeys(STAGES, 0.0)
        previous_write = 0.0
        files = {"trace.csv"}
        for index, (row, want) in enumerate(zip(rows, expected), 1):
            assert int(row["id"]) == index
            submitted = float(row["submitted_ms"])
            assert 0 <= submitted <= elapsed
            if mode == "seq":
                assert submitted >= previous_write
            stage_floor = submitted
            for stage in STAGES:
                start, end = row[f"{stage}_start_ms"], row[f"{stage}_end_ms"]
                if isinstance(want, str) and stage != "read":
                    assert not start and not end
                    continue
                start, end = float(start), float(end)
                assert max(stage_floor, last_stage_end[stage]) <= start <= end <= elapsed
                last_stage_end[stage] = stage_floor = end
            previous_write = stage_floor
            if isinstance(want, str):
                assert row["status"] == want and row["threshold"] == ""
                continue
            threshold, mask = want
            assert row["status"] == "ok" and int(row["threshold"]) == threshold
            filename = f"{index:06d}.pgm"
            assert (output / filename).read_bytes() == mask
            files.add(filename)
        assert {entry.name for entry in output.iterdir()} == files
        previous_trace = (output / "trace.csv").read_bytes()
        run(directory, mode, mode, names[:1], 1)
        assert (output / "trace.csv").read_bytes() == previous_trace


def main():
    rng = random.Random(52026)
    pictures = [
        bytes([77]) * 15, bytes([255]), bytes([0, 255]),
        bytes([10, 10, 200, 200]), bytes([0, 2, 4]), bytes(range(256)),
        bytes([10, 13, 32, 35, 80, 53]),
        *(rng.randbytes(rng.randint(8, 1000)) for _ in range(12)),
    ]
    with TemporaryDirectory(dir=ROOT / "build") as scratch:
        directory = Path(scratch)
        names, good = [], []
        for i, pixels in enumerate(pictures):
            name = f"input {i}.pgm"
            width, height = (5, 3) if i == 0 else (len(pixels), 1)
            content = pgm(width, height, pixels)
            if i == 6:
                content = b"P5\n# header comment\n6\t1\n255\n" + pixels
            (directory / name).write_bytes(content)
            threshold, mask = reference(pixels)
            names.append(name)
            good.append((threshold, pgm(width, height, mask)))

        invalid = {
            "plain.pgm": (b"P2\n1 1\n255\n0\n", "bad_header"),
            "zero.pgm": (b"P5\n0 1\n255\n", "bad_header"),
            "large.pgm": (b"P5\n4294967296 4294967296\n255\n", "bad_header"),
            "truncated.pgm": (b"P5\n2 1\n255\n\x00", "bad_raster"),
            "extra.pgm": (b"P5\n1 1\n255\n\x00\x01", "bad_raster"),
            "missing.pgm": (None, "open_input"),
        }
        for name, (content, _) in invalid.items():
            if content is not None:
                (directory / name).write_bytes(content)

        batch_names = names[:3] + list(invalid) + names[3:] + names[:3]
        batch_want = good[:3] + [error for _, error in invalid.values()] + good[3:] + good[:3]
        verify_batch(directory, batch_names, batch_want, len(invalid))
        for child_name, subset in (("single", slice(0, 1)), ("valid", slice(None))):
            child = directory / child_name
            child.mkdir()
            for name in names[subset]:
                (child / name).write_bytes((directory / name).read_bytes())
            verify_batch(child, names[subset], good[subset], 0)
        run(directory, "unknown", "unused", names[:1], 2)
    print("PASS: 2 modes, 3 batch types; 84 expected masks and 12 rejected requests.")
    print("PASS: exact independent Otsu reference, FIFO, time ordering, unique outputs,")
    print("      duplicate input, continuation after errors, and output-directory protection.")


if __name__ == "__main__":
    main()
