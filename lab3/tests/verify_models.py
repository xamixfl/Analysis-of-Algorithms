"""Compare information graphs with an independent provenance trace for n=179."""
from collections import deque
from pathlib import Path
import ast
import json
import re

root = Path(__file__).resolve().parents[1]
models = json.loads((root / "results/graph_models.json").read_text(encoding="utf-8"))


def acyclic(nodes, edges):
    degree = dict.fromkeys(nodes, 0)
    successors = {node: [] for node in nodes}
    for a, b in edges:
        assert a in degree and b in degree
        degree[b] += 1
        successors[a].append(b)
    queue = deque(node for node in nodes if degree[node] == 0)
    seen = 0
    while queue:
        a = queue.popleft()
        seen += 1
        for b in successors[a]:
            degree[b] -= 1
            if degree[b] == 0:
                queue.append(b)
    assert seen == len(nodes), "Cycle in information history"


edges_i = set()
nodes_i = {"I0", "I1"}
n, a, n_source, a_source = 179, 0, "I0", "I1"
iteration = 0
while n:
    test_id = f"I2_{iteration}"
    nodes_i.add(test_id)
    edges_i.add((n_source, test_id))
    d_id, q_id, a_id, n_id = (f"I{j}_{iteration}" for j in (3, 4, 5, 6))
    nodes_i.update((d_id, q_id, a_id, n_id))
    edges_i.update(((n_source, d_id), (n_source, q_id), (d_id, a_id),
                    (a_source, a_id), (q_id, n_id)))
    d, q = n % 10, n // 10
    a, n = 10 * a + d, q
    a_source, n_source = a_id, n_id
    iteration += 1
nodes_i.add("I7")
nodes_i.add(f"I2_{iteration}")
edges_i.add((n_source, f"I2_{iteration}"))
edges_i.add((a_source, "I7"))
assert a == 971

nodes_r, edges_r = set(), set()


def trace(n, a, depth=0):
    def vertex(j):
        identifier = f"R{j}_{depth}"
        nodes_r.add(identifier)
        return identifier

    params = vertex(0)
    predicate = vertex(1)
    edges_r.add((params, predicate))
    if n == 0:
        result = vertex(2)
        edges_r.add((params, result))
        return a, result
    d_id, q_id, b_id, call_id = (vertex(j) for j in (3, 4, 5, "6a"))
    edges_r.update(((params, d_id), (params, q_id), (params, b_id),
                    (d_id, b_id), (q_id, call_id), (b_id, call_id)))
    edges_r.add((call_id, f"R0_{depth+1}"))
    result, source = trace(n // 10, 10 * a + n % 10, depth + 1)
    get_id, ret_id = vertex("6b"), vertex(7)
    edges_r.update(((source, get_id), (get_id, ret_id)))
    return result, ret_id


assert trace(179, 0)[0] == 971
for key, nodes, edges in (("iterative", nodes_i, edges_i), ("recursive", nodes_r, edges_r)):
    model = models[key]
    assert set(model["history_nodes"]) == nodes
    assert set(map(tuple, model["history_edges"])) == edges
    assert len(model["history_edges"]) == len(edges), "Repeated edge"
    acyclic(nodes, edges)
    compact = {(re.sub(r"^[IR]|_\d+$", "", a), re.sub(r"^[IR]|_\d+$", "", b))
               for a, b in edges}
    assert compact == set(map(tuple, model["information"]))
    print(f"PASS: {key} history: {len(nodes)} vertices, {len(edges)} edges; compact projection agrees.")
assert ("0", "6b") not in map(tuple, models["recursive"]["information"])
assert ("6a", "6b") not in map(tuple, models["recursive"]["information"])

assert (len(nodes_i), len(edges_i)) == (5 * 3 + 4, 6 * 3 + 2)
assert (len(nodes_r), len(edges_r)) == (8 * 3 + 3, 10 * 3 + 2)
assert ("0", "2") in map(tuple, models["iterative"]["information"])
assert ("6", "2") in map(tuple, models["iterative"]["information"])
assert ("0", "1") in map(tuple, models["recursive"]["information"])
for name in ("iter_operational", "iter_dynamic", "rec_operational", "rec_dynamic"):
    figure = (root / "figures" / f"{name}.tex").read_text(encoding="utf-8")
    assert r"\cdots" in figure, name
print("PASS: parameterized figures contain ellipses; conditions consume n.")

# Check the formulas in the report throughout the supported digit range.
# Extract only trace functions, without executing figure generation on import.
syntax = ast.parse((root / "generate_figures.py").read_text(encoding="utf-8"))
trace_defs = [item for item in syntax.body if isinstance(item, ast.FunctionDef)
              and item.name in ("iterative_trace", "recursive_trace")]
namespace = {}
exec(compile(ast.Module(body=trace_defs, type_ignores=[]), "trace_models", "exec"), namespace)
for digits in range(1, 20):
    number = int(("123456789" * 3)[:digits])
    for key, function in (("iterative", "iterative_trace"), ("recursive", "recursive_trace")):
        nodes, edges, *_ = namespace[function](number)
        expected_nodes = 5 * digits + 4 if key == "iterative" else 8 * digits + 3
        expected_edges = 6 * digits + 2 if key == "iterative" else 10 * digits + 2
        assert (len(nodes), len(edges)) == (expected_nodes, expected_edges)
        assert len(nodes) == len(set(nodes)) and len(edges) == len(set(edges))
        acyclic(nodes, edges)
        compact = {(re.sub(r"^[IR]|_\d+$", "", a), re.sub(r"^[IR]|_\d+$", "", b))
                   for a, b in edges}
        if digits >= 2:
            assert compact == set(map(tuple, models[key]["information"]))
print("PASS: history sizes and compact projections checked for 1..19 digits.")
