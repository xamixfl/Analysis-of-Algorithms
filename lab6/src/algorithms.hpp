#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Edge {
    std::int64_t hours = 0;
    std::int64_t cost = 0;
};

struct Graph {
    std::vector<std::string> ports;
    std::vector<std::vector<Edge>> edges;
    std::int64_t budget;
};

struct Route {
    std::vector<int> ports;
    std::int64_t hours = 0;
    std::int64_t cost = 0;
};

struct AntParameters {
    double alpha = 1.0;
    double beta = 3.0;
    double evaporation = 0.2;
    int iterations = 32;
    int ants = 12;
};

std::optional<Route> exhaustive(const Graph& graph);
std::optional<Route> ant_colony(const Graph& graph, const AntParameters& settings,
                                std::uint64_t seed);
