#include "algorithms.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <tuple>
#include <utility>

namespace {
constexpr double pheromone_min = 0.05;
constexpr double pheromone_max = 10.0;
constexpr double exploration = 0.05;
constexpr double deposit_scale = 100.0;
constexpr double elite_weight = 4.0;

using Trails = std::vector<std::vector<double>>;

bool improves(const Route& route, const std::optional<Route>& best) {
    return !best || std::tie(route.hours, route.cost) < std::tie(best->hours, best->cost);
}

std::optional<Route> build_route(const Graph& graph, const Trails& pheromone,
                                 const AntParameters& settings, std::mt19937_64& generator) {
    const int n = static_cast<int>(graph.ports.size());
    const int start = std::uniform_int_distribution<int>(0, n - 1)(generator);
    Route route{{start}};
    route.ports.reserve(n);
    std::vector<bool> visited(n, false);
    visited[start] = true;

    std::vector<int> candidates;
    std::vector<double> weights;
    candidates.reserve(n);
    weights.reserve(n);

    while (static_cast<int>(route.ports.size()) < n) {
        const int from = route.ports.back();
        const auto steps_left = static_cast<std::size_t>(n) - route.ports.size();
        const double funds = static_cast<double>(graph.budget - route.cost)
            / static_cast<double>(steps_left) + 1.0;
        candidates.clear();
        weights.clear();
        double sum = 0.0;
        for (int to = 0; to < n; ++to) {
            const Edge& edge = graph.edges[from][to];
            if (visited[to] || edge.hours == 0) {
                continue;
            }
            const double weight = std::pow(pheromone[from][to], settings.alpha)
                * std::pow(1.0 / static_cast<double>(edge.hours), settings.beta)
                * (funds / (funds + static_cast<double>(edge.cost)));
            candidates.push_back(to);
            weights.push_back(weight);
            sum += weight;
        }

        if (candidates.empty()) {
            return std::nullopt;
        }
        const double uniform_share = exploration / static_cast<double>(weights.size());
        for (double& weight : weights) {
            weight = (1.0 - exploration) * (weight / sum) + uniform_share;
        }
        std::discrete_distribution<std::size_t> choose(weights.begin(), weights.end());
        const int to = candidates[choose(generator)];
        const Edge& edge = graph.edges[from][to];
        route.cost += edge.cost;
        if (route.cost > graph.budget) {
            return std::nullopt;
        }
        route.hours += edge.hours;
        route.ports.push_back(to);
        visited[to] = true;
    }
    return route;
}

void deposit(Trails& delta, const Route& route, double multiplier) {
    if (route.ports.size() < 2) {
        return;
    }
    const double amount = multiplier * deposit_scale / static_cast<double>(route.hours);
    for (std::size_t i = 1; i < route.ports.size(); ++i) {
        const int from = route.ports[i - 1];
        const int to = route.ports[i];
        delta[from][to] += amount;
        delta[to][from] += amount;
    }
}
}

std::optional<Route> exhaustive(const Graph& graph) {
    Route route;
    route.ports.resize(graph.ports.size());
    std::iota(route.ports.begin(), route.ports.end(), 0);
    std::optional<Route> best;

    do {
        route.hours = 0;
        route.cost = 0;
        bool feasible = true;
        for (std::size_t i = 1; i < route.ports.size(); ++i) {
            const Edge& edge = graph.edges[route.ports[i - 1]][route.ports[i]];
            route.cost += edge.cost;
            if (edge.hours == 0 || route.cost > graph.budget) {
                feasible = false;
                break;
            }
            route.hours += edge.hours;
        }
        if (feasible && improves(route, best)) {
            best = route;
        }
    } while (std::next_permutation(route.ports.begin(), route.ports.end()));
    return best;
}

std::optional<Route> ant_colony(const Graph& graph, const AntParameters& settings,
                                std::uint64_t seed) {
    const std::size_t n = graph.ports.size();
    std::mt19937_64 generator(seed);
    Trails pheromone(n, std::vector<double>(n, 1.0));
    std::optional<Route> best;

    for (int iteration = 0; iteration < settings.iterations; ++iteration) {
        Trails delta(n, std::vector<double>(n, 0.0));
        for (int ant = 0; ant < settings.ants; ++ant) {
            auto route = build_route(graph, pheromone, settings, generator);
            if (!route) {
                continue;
            }
            deposit(delta, *route, 1.0);
            if (improves(*route, best)) {
                best = std::move(route);
            }
        }
        if (best) {
            deposit(delta, *best, elite_weight);
        }
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                if (graph.edges[i][j].hours > 0) {
                    pheromone[i][j] = std::clamp(
                        (1.0 - settings.evaporation) * pheromone[i][j] + delta[i][j],
                        pheromone_min, pheromone_max);
                    pheromone[j][i] = pheromone[i][j];
                }
            }
        }
    }
    return best;
}
