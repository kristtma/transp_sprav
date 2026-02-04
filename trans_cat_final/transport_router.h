#pragma once

#include "transport_catalogue.h"
#include "graph.h"
#include "router.h"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <variant>
#include <memory>

namespace transport {

struct RoutingSettings {
    double bus_wait_time = 0;  // в минутах
    double bus_velocity = 0;   // в км/ч
};

// Структуры для элементов маршрута
struct WaitItem {
    std::string stop_name;
    double time;
};

struct BusItem {
    std::string bus;
    int span_count;
    double time;
};

struct RouteInfo {
    double total_time = 0;
    std::vector<std::variant<WaitItem, BusItem>> items;
};

class TransportRouter {
public:
    TransportRouter(const transport_catalogue::TransportCatalogue& catalogue,
                    const RoutingSettings& settings);

    std::optional<RouteInfo> FindRoute(std::string_view from, std::string_view to) const;

private:
    void BuildGraph();
    void AddBusEdge(std::string_view bus_name,std::string_view from_stop,std::string_view to_stop, double time, int span_count);
    double CalculateSegmentTime(std::string_view from_stop, std::string_view to_stop, double speed_m_per_min) const;


    const transport_catalogue::TransportCatalogue& catalogue_;
    RoutingSettings settings_;

    graph::DirectedWeightedGraph<double> graph_;
    std::unique_ptr<graph::Router<double>> router_;

    // Две вершины для каждой остановки: wait vertex и bus vertex
    std::unordered_map<std::string, size_t> stop_to_wait_vertex_;
    std::unordered_map<std::string, size_t> stop_to_bus_vertex_;
    std::unordered_map<size_t, std::string> vertex_to_stop_;

    std::unordered_map<size_t, std::pair<std::string, int>> edge_to_bus_info_;
};

} // namespace transport
