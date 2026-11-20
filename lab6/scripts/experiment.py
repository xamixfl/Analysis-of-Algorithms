"""Run parameter tuning and timing, then generate report tables from the data."""
import argparse
import csv
from itertools import product
import math
import os
import platform
from pathlib import Path
from statistics import median
from tempfile import TemporaryDirectory
import subprocess

from graphs import DATA, ROOT, evaluate, read_graph
from solver import solve

RESULTS = ROOT / "results"
GRAPH_IDS = ("G1", "G2", "G3")
PARAM_REPEATS = 20
BENCH_REPEATS = 7
ITERATIONS = 32
ANTS = 12
G1_PORT_NAMES = {
    "Auckland": "Окленд",
    "Busan": "Пусан",
    "Los_Angeles": "Лос-Анджелес",
    "Yokohama": "Йокогама",
    "Singapore": "Сингапур",
    "Sydney": "Сидней",
    "Shanghai": "Шанхай",
    "Honolulu": "Гонолулу",
}
GRID = tuple(product((0, 1, 2), (0, 1, 3), (0.2, 0.5, 0.8)))


def save_csv(name, rows):
    with (RESULTS / name).open("w", newline="", encoding="utf8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def percent(value, digits=1):
    if math.isinf(value):
        return r"\ensuremath{\infty}"
    return f"{value:.{digits}f}".replace(".", "{,}")


def error(row, exact, graph):
    if row["status"] == "not_found":
        return math.inf
    assert row["status"] == "ok"
    route = row["path"].split(" -> ")
    ports, budget, edges = graph
    assert len(route) == len(ports) and set(route) == set(ports)
    hours, cost = int(row["hours"]), int(row["cost"])
    assert evaluate(route, edges) == (hours, cost) and cost <= budget
    assert hours >= exact > 0
    return 100 * (hours - exact) / exact


def scan_grid():
    raw = []
    summary = []
    references = []
    graphs = {name: read_graph(DATA / f"{name}.txt") for name in GRAPH_IDS}
    for name, graph in graphs.items():
        row, = solve(DATA / f"{name}.txt", "exact")
        assert row["status"] == "ok"
        assert evaluate(row["path"].split(" -> "), graph[2]) == (
            int(row["hours"]), int(row["cost"]))
        assert int(row["cost"]) <= graph[1]
        references.append({"graph": name, "budget": graph[1], **row})

    for alpha, beta, rho in GRID:
        parameters = {"alpha": alpha, "beta": beta, "rho": rho, "iterations": ITERATIONS,
                      "ants": ANTS}
        stats = {}
        for index, ref in enumerate(references):
            name = ref["graph"]
            first_seed = 10000 + index * 1000
            rows = solve(DATA / f"{name}.txt", "ant",
                         (PARAM_REPEATS, first_seed, *parameters.values()))
            errors = [error(row, int(ref["hours"]), graphs[name]) for row in rows]
            raw.extend({"graph": name, **parameters, **row, "error_pct": deviation}
                       for row, deviation in zip(rows, errors))
            stats[f"{name}_max"] = max(errors)
            stats[f"{name}_median"] = median(errors)
            stats[f"{name}_misses"] = errors.count(math.inf)

        summary.append({**parameters, **stats,
                        "class_max": max(stats[f"{name}_max"] for name in GRAPH_IDS),
                        "class_median": median(stats[f"{name}_median"] for name in GRAPH_IDS)})
    save_csv("references.csv", references)
    save_csv("parameters_raw.csv", raw)
    save_csv("parameters_summary.csv", summary)
    return summary, references


def find_best(summary):
    return min(summary, key=lambda row: (row["class_max"], row["class_median"], row["alpha"],
                                         row["beta"], row["rho"]))


def measure(best):
    raw = []
    summary = []
    parameters = tuple(best[key] for key in ("alpha", "beta", "rho", "iterations", "ants"))
    for n in range(5, 12):
        budget = 23 * (n - 1)
        path = DATA / f"N{n}.txt"
        graph = read_graph(path)
        first_seed = 30000 + 100 * n
        solve(path, "both", (1, first_seed - 1, *parameters))
        series = solve(path, "both", (BENCH_REPEATS, first_seed, *parameters))
        exact_rows = [row for row in series if row["method"] == "exact"]
        ant_rows = [row for row in series if row["method"] == "ant"]
        assert all(row["status"] == "ok" for row in exact_rows)
        exact_values = {int(row["hours"]) for row in exact_rows}
        assert len(exact_values) == 1
        exact, = exact_values
        errors = [error(row, exact, graph) for row in ant_rows]
        exact_us = median(int(row["time_ns"]) for row in exact_rows) / 1000
        ant_us = median(int(row["time_ns"]) for row in ant_rows) / 1000
        raw.extend({"n": n, "budget": budget, **row,
                    "error_pct": 0.0 if row["method"] == "exact" else error(row, exact, graph)}
                   for row in series)
        summary.append({"n": n, "budget": budget, "exact_hours": exact, "exact_us": exact_us,
                        "ant_us": ant_us, "ant_error_max": max(errors),
                        "ant_error_median": median(errors), "ant_misses": errors.count(math.inf)})
    save_csv("benchmark_raw.csv", raw)
    save_csv("benchmark_summary.csv", summary)
    return summary


def tex_rows(filename, rows):
    (RESULTS / filename).write_text("".join(" & ".join(map(str, row)) + r" \\" + "\n"
                                            for row in rows), encoding="utf8")


def solve_without_budget(graph_id):
    path = DATA / f"{graph_id}.txt"
    names, _, weights = read_graph(path)
    max_cost = max(cost for _, cost in weights.values())
    lines = path.read_text(encoding="ascii").splitlines()
    lines[0] = f"{len(names)} {(len(names) - 1) * max_cost}"
    with TemporaryDirectory() as folder:
        unrestricted = Path(folder) / path.name
        unrestricted.write_text("\n".join(lines) + "\n", encoding="ascii")
        row, = solve(unrestricted, "exact")
    assert row["status"] == "ok"
    return row


def write_route_tex(reference):
    _, _, edges = read_graph(DATA / f"{reference['graph']}.txt")
    ports = reference["path"].split(" -> ")
    assert len(ports) == 8
    names = [G1_PORT_NAMES.get(port, port.replace("_", " ")) for port in ports]
    labels = [r"$%d;\,%d$" % edges[a, b] for a, b in zip(ports, ports[1:])]
    macros = [("RoutePort", names), ("RouteLeg", labels)]
    (RESULTS / "reference_route.tex").write_text("".join(
        rf"\newcommand\{prefix}{letter}{{{value}}}" + "\n"
        for prefix, values in macros for letter, value in zip("ABCDEFGH", values)
    ), encoding="utf8")


def write_tex(grid, references, best, times):
    write_route_tex(references[0])
    unrestricted = [solve_without_budget(name) for name in GRAPH_IDS]
    tex_rows("parameters.tex", (
        [row["alpha"], row["beta"], percent(row["rho"]),
         *(percent(row[f"{name}_{key}"]) for name in GRAPH_IDS for key in ("max", "median")),
         percent(row["class_max"])]
        for row in grid
    ))
    tex_rows("reference.tex", [
        [f"$G_{i}$", row["budget"], row["hours"], row["cost"]]
        for i, row in enumerate(references, start=1)
    ])
    tex_rows("winner.tex", [
        [f"$G_{i}$", percent(best[f"{name}_max"]), percent(best[f"{name}_median"]),
         best[f"{name}_misses"]]
        for i, name in enumerate(GRAPH_IDS, start=1)
    ])
    tex_rows("benchmark.tex", (
        [row["n"], row["budget"], row["exact_hours"], percent(row["exact_us"], 2),
         percent(row["ant_us"], 2), percent(row["ant_error_max"]),
         percent(row["ant_error_median"]), row["ant_misses"]]
        for row in times
    ))
    plot = "n exact ant\n" + "".join(
        f'{r["n"]} {r["exact_us"]:.3f} {r["ant_us"]:.3f}\n' for r in times)
    (RESULTS / "benchmark_plot.dat").write_text(plot, encoding="ascii")

    values = {
        "BestAlpha": best["alpha"], "BestBeta": best["beta"], "BestRho": percent(best["rho"]),
        "WorstWinner": percent(best["class_max"]), "MedianWinner": percent(best["class_median"]),
        "AllWithoutMiss": sum(math.isfinite(row["class_max"]) for row in grid),
        "Configurations": len(grid), "TotalAntRuns": len(grid) * len(GRAPH_IDS) * PARAM_REPEATS,
        "BestTies": sum(row["class_max"] == best["class_max"] for row in grid),
        "GridWorstFinite": percent(max((row["class_max"] for row in grid
                                       if math.isfinite(row["class_max"])), default=math.inf)),
        "BenchmarkMisses": sum(row["ant_misses"] for row in times),
        "BenchmarkMax": percent(max((row["ant_error_max"] for row in times if math.isfinite(row["ant_error_max"])), default=math.inf)),
        "FirstFasterSize": next((r["n"] for r in times if r["ant_us"] < r["exact_us"]), "---"),
        "SpeedupLargest": percent(times[-1]["exact_us"] / times[-1]["ant_us"], 1),
        "LastExact": percent(times[-1]["exact_us"] / 1000, 2),
        "LastAnt": percent(times[-1]["ant_us"] / 1000, 2),
        "UnboundedHours": ", ".join(row["hours"] for row in unrestricted[:-1]) + " и " + unrestricted[-1]["hours"],
        "UnboundedCost": ", ".join(row["cost"] for row in unrestricted[:-1]) + " и " + unrestricted[-1]["cost"],
    }
    (RESULTS / "statistics.tex").write_text(
        "".join(rf"\newcommand\{key}{{{value}}}" + "\n" for key, value in values.items()),
        encoding="utf8")
    print(f"Best settings: alpha={best['alpha']}, beta={best['beta']}, rho={best['rho']}")
    print(f"Worst error: {best['class_max']:.2f}%; median criterion: {best['class_median']:.2f}%")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++', help='GCC used to build lab6.exe')
    args = parser.parse_args()
    grid, references = scan_grid()
    best = find_best(grid)
    times = measure(best)
    write_tex(grid, references, best, times)
    compiler = subprocess.check_output([args.compiler, "-dumpfullversion"], text=True).strip()
    info = (f"OS: {platform.platform()}\n"
            f"Processor: {os.environ.get('PROCESSOR_IDENTIFIER', platform.processor())}\n"
            f"Compiler: GCC {compiler}, -std=c++17 -O2 -Wall -Wextra -Wpedantic\n"
            f"Python: {platform.python_version()}\n"
            "Timer: std::chrono::steady_clock; graph I/O and formatting excluded\n")
    (RESULTS / "environment.txt").write_text(info, encoding="utf8")
    print(info, end="")


if __name__ == "__main__":
    main()
