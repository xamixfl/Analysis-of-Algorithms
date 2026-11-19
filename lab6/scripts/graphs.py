"""Prepare the synthetic port graphs used in the experiments."""
from itertools import combinations
from pathlib import Path
import random

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
PORT_GROUPS = (
    "Yokohama Busan Shanghai Singapore Sydney Auckland Honolulu Los_Angeles",
    "Lisbon Southampton Rotterdam Reykjavik Halifax Boston New_York Norfolk",
    "Cape_Town Durban Maputo Mombasa Colombo Mumbai Male Port_Louis",
)
PARAM_BUDGET = 165


def write_graph(path, names, budget, edges):
    rows = [f"{len(names)} {budget}", " ".join(names)]
    for i in range(len(names) - 1):
        rows.append("  ".join(f"{hours} {cost}" for hours, cost in (
            edges[i, j] for j in range(i + 1, len(names))
        )))
    path.write_text("\n".join(rows) + "\n", encoding="ascii")


def read_graph(path):
    tokens = iter(path.read_text(encoding="ascii").split())
    n, budget = int(next(tokens)), int(next(tokens))
    names = [next(tokens) for _ in range(n)]
    weights = {(names[i], names[j]): (int(next(tokens)), int(next(tokens)))
               for i, j in combinations(range(n), 2)}
    weights.update({(b, a): edge for (a, b), edge in tuple(weights.items())})
    return names, budget, weights


def evaluate(route, edges):
    weights = [edges[a, b] for a, b in zip(route, route[1:])]
    if any(hours == 0 for hours, _ in weights):
        return None
    return sum(hours for hours, _ in weights), sum(cost for _, cost in weights)


def generated_edges(n, seed):
    rng = random.Random(seed)
    pairs = list(combinations(range(n), 2))
    values = [(rng.randint(24, 120), rng.randint(10, 50)) for _ in pairs]
    values[0] = (24, values[0][1])
    values[1] = (120, values[1][1])
    values[2] = (values[2][0], 10)
    values[3] = (values[3][0], 50)
    rng.shuffle(values)
    return dict(zip(pairs, values))


def main():
    for i, ports in enumerate(PORT_GROUPS, start=1):
        names = ports.split()
        write_graph(DATA / f"G{i}.txt", names, PARAM_BUDGET, generated_edges(len(names), 600 + i))
    for n in range(5, 12):
        names = [f"P{i + 1}" for i in range(n)]
        edges = generated_edges(n, 1000 + n)
        write_graph(DATA / f"N{n}.txt", names, 23 * (n - 1), edges)


if __name__ == "__main__":
    main()
