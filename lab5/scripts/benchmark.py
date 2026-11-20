import csv
import hashlib
import os
import platform
import random
import shutil
import statistics as stats
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / ("lab5.exe" if os.name == "nt" else "lab5")
RESULTS = ROOT / "results"
FIGURES = ROOT / "figures"
STAGES = ("read", "segment", "write")
SIZES = (512, 2048)
BATCHES = (1, 4, 16, 32)
REPEATS = 15


def run(directory, mode, inputs, side):
    output = directory / "out"
    result = subprocess.run([str(EXE), mode, "out", *inputs], cwd=directory,
                            check=True, capture_output=True, text=True, timeout=60)
    elapsed = float(result.stdout.split("elapsed_ms=")[1])
    assert f"successful={len(inputs)} " in result.stdout
    trace = (output / "trace.csv").read_bytes()
    rows = list(csv.DictReader(trace.decode().splitlines()))
    assert len(rows) == len(inputs) and all(row["status"] == "ok" for row in rows)
    mask_bytes = len(f"P5\n{side} {side}\n255\n".encode()) + side ** 2
    assert all((output / f"{i:06d}.pgm").stat().st_size == mask_bytes
               for i in range(1, len(inputs) + 1))
    durations = {
        f"{stage}_ms": stats.mean(float(row[f"{stage}_end_ms"]) -
                                 float(row[f"{stage}_start_ms"]) for row in rows)
        for stage in STAGES
    }
    latency = stats.median(float(row["write_end_ms"]) - float(row["submitted_ms"])
                           for row in rows)
    shutil.rmtree(output)
    return {"elapsed_ms": elapsed, **durations, "latency_ms": latency}, trace


def collect():
    records, selected = [], {}
    rng = random.Random(52026)
    with TemporaryDirectory(dir=ROOT / "build") as scratch:
        directory = Path(scratch)
        for side in SIZES:
            names = []
            for i in range(4):
                name = f"input_{i}.pgm"
                (directory / name).write_bytes(
                    f"P5\n{side} {side}\n255\n".encode() + rng.randbytes(side ** 2))
                names.append(name)
            for count in BATCHES:
                inputs = [names[i % len(names)] for i in range(count)]
                for mode in ("seq", "pipe"):
                    run(directory, mode, inputs, side)
                for repeat in range(1, REPEATS + 1):
                    modes = ("seq", "pipe") if repeat % 2 else ("pipe", "seq")
                    for position, mode in enumerate(modes, 1):
                        row, trace = run(directory, mode, inputs, side)
                        records.append({
                            "side": side, "requests": count, "repeat": repeat,
                            "position": position, "mode": mode, **row,
                        })
                        if side == 2048 and count == 16:
                            selected.setdefault(mode, []).append((row["elapsed_ms"], trace))
                print(f"{side} x {side}, {count:2} requests: {REPEATS} measured pairs",
                      flush=True)
    for mode, candidates in selected.items():
        _, trace = sorted(candidates, key=lambda pair: pair[0])[len(candidates) // 2]
        (RESULTS / f"timeline_{mode}.csv").write_bytes(trace)
    return records


def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


def summarize(records):
    summary = []
    for side in SIZES:
        for count in BATCHES:
            selected = [row for row in records if row["side"] == side and row["requests"] == count]
            result = {"side": side, "requests": count}
            for mode in ("seq", "pipe"):
                rows = [row for row in selected if row["mode"] == mode]
                for field in ("elapsed_ms", "latency_ms", *(f"{s}_ms" for s in STAGES)):
                    result[f"{mode}_{field}"] = stats.median(row[field] for row in rows)
                low, _, high = stats.quantiles([row["elapsed_ms"] for row in rows],
                                               n=4, method="inclusive")
                result[f"{mode}_q1_ms"] = low
                result[f"{mode}_q3_ms"] = high
            result["speedup"] = result["seq_elapsed_ms"] / result["pipe_elapsed_ms"]
            summary.append(result)
    return summary


def draw_times(summary):
    figure, axes = plt.subplots(1, 2, figsize=(7.4, 3.35), layout="constrained")
    for side, axis in zip(SIZES, axes):
        selected = [row for row in summary if row["side"] == side]
        for mode, label, style in (("seq", "Последовательно", "o--"),
                                   ("pipe", "Конвейер", "s-")):
            y = [row[f"{mode}_elapsed_ms"] for row in selected]
            low = [v - row[f"{mode}_q1_ms"] for v, row in zip(y, selected)]
            high = [row[f"{mode}_q3_ms"] - v for v, row in zip(y, selected)]
            axis.errorbar(BATCHES, y, yerr=(low, high), fmt=style, capsize=3, label=label)
        axis.set(xscale="log", yscale="log", xlabel="Число заявок", title=f"{side} × {side}")
        axis.set_xticks(BATCHES, [str(count) for count in BATCHES])
        axis.grid(axis="y", alpha=0.25, which="both")
    axes[0].set_ylabel("Время пакета, мс")
    axes[0].legend(loc="upper left", fontsize=9)
    figure.savefig(FIGURES / "times.pdf")
    plt.close(figure)


def draw_timeline():
    figure, axes = plt.subplots(2, 1, figsize=(7.4, 4.3), sharex=True, layout="constrained")
    colors = ("#28577e", "#ce6730", "#407843")
    for mode, label, axis in zip(("seq", "pipe"), ("Последовательно", "Конвейер"), axes):
        with (RESULTS / f"timeline_{mode}.csv").open(newline="") as file:
            rows = list(csv.DictReader(file))[:8]
        for lane, (stage, color) in enumerate(zip(STAGES, colors)):
            for row in rows:
                start = float(row[f"{stage}_start_ms"])
                end = float(row[f"{stage}_end_ms"])
                axis.barh(lane, end - start, left=start, height=0.58, color=color)
                axis.text((start + end) / 2, lane, row["id"], va="center", ha="center",
                          fontsize=7, color="white", clip_on=True)
        axis.set_yticks(range(3), ("Чтение", "Сегментация", "Запись"))
        axis.invert_yaxis()
        axis.set_title(label, loc="left", fontsize=10)
        axis.set_xlim(left=0)
        axis.set_axisbelow(True)
        axis.grid(axis="x", alpha=0.25)
    axes[1].set_xlabel("Время от начала замера, мс")
    figure.savefig(FIGURES / "timeline.pdf")
    plt.close(figure)


def write_report_data(summary):
    def number(value):
        return f"{value:.2f}".replace(".", "{,}")

    def tex_row(fields):
        return " & ".join(fields) + r" \\\hline%" + "\n"

    with (RESULTS / "times.tex").open("w", encoding="utf-8") as file:
        file.write(r"\newcommand{\TimeRows}{%" + "\n")
        for row in summary:
            file.write(tex_row([str(row["side"]), str(row["requests"]),
                                *(number(row[f"{mode}_elapsed_ms"]) for mode in ("seq", "pipe")),
                                number(row["speedup"])]))
        file.write("}\n")
    with (RESULTS / "stages.tex").open("w", encoding="utf-8") as file:
        file.write(r"\newcommand{\StageRows}{%" + "\n")
        for row in summary:
            if row["requests"] == 32:
                file.write(tex_row([str(row["side"]),
                                    *(number(row[f"pipe_{stage}_ms"]) for stage in STAGES),
                                    number(row["pipe_latency_ms"])]))
        file.write("}\n")


def main():
    for path in (ROOT / "build", RESULTS, FIGURES):
        path.mkdir(exist_ok=True)
    data = collect()
    assert len(data) == len(SIZES) * len(BATCHES) * REPEATS * 2
    summary = summarize(data)
    write_csv(RESULTS / "raw.csv", data)
    write_csv(RESULTS / "summary.csv", summary)
    draw_times(summary)
    draw_timeline()
    write_report_data(summary)
    gcc = subprocess.check_output(["gcc", "--version"], text=True).splitlines()[0]
    (RESULTS / "environment.txt").write_text(
        f"OS: {platform.platform()}\nCPU: {platform.processor()}\n"
        f"Logical CPUs: {os.cpu_count()}\nCompiler: {gcc}\n"
        "Build flags: -std=c11 -O2 -Wall -Wextra -Wpedantic -pthread\n"
        f"Executable SHA-256: {hashlib.sha256(EXE.read_bytes()).hexdigest()}\n",
        encoding="utf-8",
    )
    print(f"Saved {len(data)} measured runs.")


if __name__ == "__main__":
    main()
