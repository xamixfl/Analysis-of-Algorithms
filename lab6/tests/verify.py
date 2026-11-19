"""Verify the executable against independently enumerated routes and edge cases."""
from itertools import permutations
from pathlib import Path
import random
import subprocess
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from graphs import DATA, evaluate, generated_edges, read_graph, write_graph
from solver import EXE, solve


def optimum(ports, budget, edges):
    weights = (evaluate(route, edges) for route in permutations(ports))
    return min((pair for pair in weights if pair is not None and pair[1] <= budget),
               default=None)


def check(path, ant_expectation=None, extra=()):
    ports, budget, edges = read_graph(path)
    expected = optimum(ports, budget, edges)
    for row in solve(path, extra=extra):
        if row["status"] != "ok":
            assert row["status"] == ("infeasible" if row["method"] == "exact" else "not_found")
            assert not any(row[key] for key in ("hours", "cost", "path"))
            assert expected is None if row["method"] == "exact" else ant_expectation is None
            continue

        route = row["path"].split(" -> ")
        assert len(route) == len(ports) and set(route) == set(ports)
        metrics = int(row["hours"]), int(row["cost"])
        assert evaluate(route, edges) == metrics and metrics[1] <= budget
        assert expected is not None and metrics >= expected
        if row["method"] == "exact" or ant_expectation == "optimal":
            assert metrics == expected
    return expected


def main():
    with TemporaryDirectory(dir=ROOT / "build") as folder:
        path = Path(folder) / "case.txt"

        write_graph(path, ["Solo"], 0, {})
        assert check(path, "optimal") == (0, 0)

        write_graph(path, ["A", "B"], 9, {(0, 1): (4, 9)})
        assert check(path, "optimal") == (4, 9)

        triangle = {(0, 1): (1, 8), (0, 2): (6, 2), (1, 2): (2, 2)}
        write_graph(path, list("ABC"), 4, triangle)
        assert check(path, "optimal") == (8, 4)
        write_graph(path, list("ABC"), 12, triangle)
        assert check(path, "optimal") == (3, 10)

        write_graph(path, list("ABC"), 100, {(0, 1): (5, 3), (0, 2): (0, 0),
                                              (1, 2): (7, 4)})
        assert check(path, "optimal") == (12, 7)
        write_graph(path, list("ABC"), 100, {(0, 1): (5, 3), (0, 2): (0, 0),
                                              (1, 2): (0, 0)})
        assert check(path) is None
        write_graph(path, list("ABC"), 3, triangle)
        assert check(path) is None

        # A single ant and a single iteration can miss an existing feasible path.
        write_graph(path, list("ABC"), 4, triangle)
        series = solve(path, "ant", (100, 1, 5, 5, 1, 1, 1))
        assert {row["status"] for row in series} == {"ok", "not_found"}

        for seed in range(10):
            budget = random.Random(seed).randint(30, 160)
            write_graph(path, list("ABCDEF"), budget, generated_edges(6, seed))
            check(path, extra=(3, seed))

        invalid_inputs = [
            "0 10\n", "2 -1\nA B\n5 9\n", "2 10\nA A\n5 9\n",
            "2 10\nA B\n0 9\n", "2 10\nA B\n5\n", "2 10\nA B\n5 9\n7\n",
        ]
        for invalid in invalid_inputs:
            path.write_text(invalid, encoding="ascii")
            run = subprocess.run([str(EXE), str(path), "both"], capture_output=True)
            assert run.returncode == 2 and run.stderr.startswith(b"Error:")

    for i in range(1, 4):
        path = DATA / f"G{i}.txt"
        expected = check(path, extra=(5, 50))
        assert expected is not None
        first = solve(path, "ant", (4, 280))
        second = solve(path, "ant", (4, 280))
        for row in first + second:
            del row["time_ns"]
        assert first == second

    print("PASS: 20 independent optimum cases; 6 malformed inputs; budget, open path,")
    print("sparse graph, failed ant search and reproducibility checks.")


if __name__ == "__main__":
    main()
