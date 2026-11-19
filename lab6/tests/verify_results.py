"""Check that the saved trial files reproduce the experiment summaries."""
import csv
from collections import defaultdict
from itertools import combinations
import math
from pathlib import Path
from statistics import median
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from graphs import DATA, evaluate, read_graph

RESULTS = ROOT / "results"
GRAPH_IDS = ("G1", "G2", "G3")


def read_rows(name):
    with (RESULTS / name).open(encoding="utf8", newline="") as stream:
        return list(csv.DictReader(stream))


def verify_route(row, graph):
    names, budget, edges = graph
    route = row["path"].split(" -> ")
    hours, cost = int(row["hours"]), int(row["cost"])
    assert len(route) == len(names) and set(route) == set(names)
    assert evaluate(route, edges) == (hours, cost) and cost <= budget
    return hours


def route_error(row, graph, exact):
    if row["status"] == "not_found":
        assert not any(row[key] for key in ("hours", "cost", "path"))
        error = math.inf
    else:
        assert row["status"] == "ok"
        hours = verify_route(row, graph)
        error = 100 * (hours - exact) / exact
        assert error >= 0
    assert error == float(row["error_pct"])
    return error


def verify_tuning():
    graphs = {name: read_graph(DATA / f"{name}.txt") for name in GRAPH_IDS}
    reference = {}
    for row in read_rows("references.csv"):
        name = row["graph"]
        assert name not in reference and row["status"] == "ok"
        names, budget, edges = graphs[name]
        weights = [edges[pair] for pair in combinations(names, 2)]
        assert len(names) == 8 and budget == int(row["budget"]) == 165
        assert tuple(map(min, zip(*weights))) == (24, 10)
        assert tuple(map(max, zip(*weights))) == (120, 50)
        reference[name] = verify_route(row, graphs[name])

    grid_keys = ("alpha", "beta", "rho")
    series = defaultdict(dict)
    trials = read_rows("parameters_raw.csv")
    assert len(trials) == 1620 and set(reference) == set(GRAPH_IDS)
    for row in trials:
        name = row["graph"]
        key = tuple(row[field] for field in grid_keys) + (name,)
        seed = int(row["seed"])
        assert seed not in series[key] and row["iterations"] == "32" and row["ants"] == "12"
        series[key][seed] = route_error(row, graphs[name], reference[name])

    summary = read_rows("parameters_summary.csv")
    assert len(summary) == 27 and len(series) == 81
    for row in summary:
        key = tuple(row[field] for field in grid_keys)
        for index, name in enumerate(GRAPH_IDS):
            errors = series.pop(key + (name,))
            first_seed = 10000 + index * 1000
            assert set(errors) == set(range(first_seed, first_seed + 20))
            assert max(errors.values()) == float(row[f"{name}_max"])
            assert median(errors.values()) == float(row[f"{name}_median"])
            assert sum(math.isinf(error) for error in errors.values()) == int(row[f"{name}_misses"])
        assert max(float(row[f"{name}_max"]) for name in GRAPH_IDS) == float(row["class_max"])
        assert median(float(row[f"{name}_median"]) for name in GRAPH_IDS) == float(row["class_median"])
    assert not series


def verify_timing():
    timing = read_rows("benchmark_raw.csv")
    summary = read_rows("benchmark_summary.csv")
    assert len(timing) == 98 and {int(row["n"]) for row in summary} == set(range(5, 12))
    for row in summary:
        n, exact = int(row["n"]), int(row["exact_hours"])
        graph = read_graph(DATA / f"N{n}.txt")
        assert len(graph[0]) == n and graph[1] == int(row["budget"]) == 23 * (n - 1)
        exact_rows = [item for item in timing if item["n"] == row["n"] and item["method"] == "exact"]
        ant_rows = [item for item in timing if item["n"] == row["n"] and item["method"] == "ant"]
        assert len(exact_rows) == len(ant_rows) == 7
        assert all(item["status"] == "ok" and verify_route(item, graph) == exact
                   and float(item["error_pct"]) == 0 for item in exact_rows)
        errors = [route_error(item, graph, exact) for item in ant_rows]
        assert max(errors) == float(row["ant_error_max"])
        assert median(errors) == float(row["ant_error_median"])
        assert errors.count(math.inf) == int(row["ant_misses"])
        for method, trials in (("exact", exact_rows), ("ant", ant_rows)):
            times = [int(item["time_ns"]) for item in trials]
            assert min(times) > 0 and median(times) / 1000 == float(row[method + "_us"])


if __name__ == "__main__":
    verify_tuning()
    verify_timing()
    print("PASS: 3 references, 1,620 tuning trials across 81 series, and 98 timing records.")
