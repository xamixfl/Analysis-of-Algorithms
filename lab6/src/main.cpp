#include "algorithms.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace {
constexpr int max_ports = 200;
constexpr int max_exact_ports = 11;
constexpr std::int64_t max_weight = 1000000000;
constexpr std::int64_t max_budget = max_ports * max_weight;

Graph read_graph(const std::string& filename) {
    std::ifstream input(filename);
    int n = 0;
    Graph graph{};
    if (!(input >> n >> graph.budget) || n < 1 || n > max_ports ||
        graph.budget < 0 || graph.budget > max_budget) {
        throw std::runtime_error("expected N in [1, 200] and budget in [0, 200000000000]");
    }
    graph.ports.resize(n);
    std::unordered_set<std::string> names;
    for (std::string& name : graph.ports) {
        if (!(input >> name) || name.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") !=
                std::string::npos || !names.insert(name).second) {
            throw std::runtime_error("expected N unique port names using A-Z, a-z, 0-9, _ or -");
        }
    }

    graph.edges.assign(n, std::vector<Edge>(n));
    // The file lists (i,j) for i < j. Store both directions.
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Edge edge;
            if (!(input >> edge.hours >> edge.cost) || edge.hours < 0 ||
                edge.hours > max_weight || edge.cost < 0 || edge.cost > max_weight ||
                ((edge.hours == 0) != (edge.cost == 0))) {
                throw std::runtime_error("invalid edge: expected positive hours and cost or 0 0");
            }
            graph.edges[i][j] = edge;
            graph.edges[j][i] = edge;
        }
    }
    std::string extra;
    if (input >> extra) {
        throw std::runtime_error("unexpected token after the last edge");
    }
    return graph;
}

std::uint64_t integer(const std::string& arg, std::uint64_t min, std::uint64_t max) {
    if (arg.empty() || arg.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("expected a nonnegative integer: " + arg);
    }
    const std::uint64_t value = std::stoull(arg);
    if (value < min || value > max) {
        throw std::runtime_error("integer argument out of range: " + arg);
    }
    return value;
}

double decimal(const std::string& arg, double max) {
    std::size_t end = 0;
    const double value = std::stod(arg, &end);
    if (end != arg.size() || !std::isfinite(value) || value < 0.0 || value > max) {
        throw std::runtime_error("decimal argument out of range: " + arg);
    }
    return value;
}

void print_result(const char* method, const Graph& graph, const std::optional<Route>& route,
                  std::uint64_t seed, int trial, std::int64_t elapsed_ns) {
    std::cout << method << ',' << trial << ',' << seed << ',' << elapsed_ns << ',';
    if (!route) {
        std::cout << (std::string(method) == "exact" ? "infeasible" : "not_found")
                  << ",,,\n";
        return;
    }
    std::cout << "ok," << route->hours << ',' << route->cost << ',';
    for (std::size_t i = 0; i < route->ports.size(); ++i) {
        if (i != 0) {
            std::cout << " -> ";
        }
        std::cout << graph.ports[route->ports[i]];
    }
    std::cout << '\n';
}
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 3 || (argc > 5 && argc != 10)) {
            throw std::runtime_error("usage: lab6 GRAPH exact|ant|both "
                                     "[REPEATS [SEED [ALPHA BETA RHO ITERATIONS ANTS]]]");
        }
        const std::string method = argv[2];
        if (method != "exact" && method != "ant" && method != "both") {
            throw std::runtime_error("expected mode exact, ant or both");
        }
        const Graph graph = read_graph(argv[1]);
        if (method != "ant" && graph.ports.size() > max_exact_ports) {
            throw std::runtime_error("exact mode supports at most 11 ports");
        }
        const int repeats = argc >= 4 ? static_cast<int>(integer(argv[3], 1, 10000)) : 1;
        const auto max_seed = std::numeric_limits<std::uint64_t>::max();
        const auto first_seed = argc >= 5 ? integer(argv[4], 0, max_seed - (repeats - 1)) : 1;
        AntParameters settings;
        if (argc == 10) {
            settings.alpha = decimal(argv[5], 5.0);
            settings.beta = decimal(argv[6], 5.0);
            settings.evaporation = decimal(argv[7], 1.0);
            settings.iterations = static_cast<int>(integer(argv[8], 1, 10000));
            settings.ants = static_cast<int>(integer(argv[9], 1, 10000));
        }

        using Clock = std::chrono::steady_clock;
        std::cout << "method,trial,seed,time_ns,status,hours,cost,path\n";
        for (int trial = 0; trial < repeats; ++trial) {
            const auto seed = first_seed + trial;
            if (method != "ant") {
                const auto start = Clock::now();
                const auto route = exhaustive(graph);
                const auto stop = Clock::now();
                print_result("exact", graph, route, seed, trial + 1,
                             std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
            }
            if (method != "exact") {
                const auto start = Clock::now();
                const auto route = ant_colony(graph, settings, seed);
                const auto stop = Clock::now();
                print_result("ant", graph, route, seed, trial + 1,
                             std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
    return 0;
}
