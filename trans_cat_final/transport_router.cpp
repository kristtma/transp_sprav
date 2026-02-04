#include "transport_router.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace transport {

TransportRouter::TransportRouter(const transport_catalogue::TransportCatalogue& catalogue,
                                 const RoutingSettings& settings)
    : catalogue_(catalogue), settings_(settings) {
    BuildGraph();
}

// Вспомогательный метод для добавления ребра автобусного маршрута
void TransportRouter::AddBusEdge(std::string_view bus_name, std::string_view from_stop, std::string_view to_stop, double time, int span_count) {
    size_t from_bus_vertex = stop_to_bus_vertex_.at(std::string(from_stop));
    size_t to_wait_vertex = stop_to_wait_vertex_.at(std::string(to_stop));

    size_t edge_id = graph_.AddEdge({from_bus_vertex, to_wait_vertex, time});
    edge_to_bus_info_[edge_id] = {std::string(bus_name), span_count};
}

// Вспомогательный метод для расчета времени сегмента маршрута
double TransportRouter::CalculateSegmentTime(std::string_view from_stop, std::string_view to_stop, double speed_m_per_min) const {
    const auto* from = catalogue_.FindStop(std::string(from_stop));
    const auto* to = catalogue_.FindStop(std::string(to_stop));

    if (!from || !to) return 0;

    int distance = catalogue_.GetDistanceToStops(from, to);
    return distance / speed_m_per_min;
}

void TransportRouter::BuildGraph() {
    const auto& all_stops = catalogue_.GetAllStops();

    // Создаем 2 вершины для каждой остановки
    size_t vertex_id = 0;
    for (const auto& [name, stop] : all_stops) {
        stop_to_wait_vertex_[name] = vertex_id;
        vertex_to_stop_[vertex_id] = name;
        vertex_id++;

        stop_to_bus_vertex_[name] = vertex_id;
        vertex_to_stop_[vertex_id] = name;
        vertex_id++;
    }

    graph_ = graph::DirectedWeightedGraph<double>(vertex_id);

    // 1. Добавляем ребра ожидания
    for (const auto& [name, wait_vertex] : stop_to_wait_vertex_) {
        size_t bus_vertex = stop_to_bus_vertex_.at(name);
        graph_.AddEdge({wait_vertex, bus_vertex, settings_.bus_wait_time});
    }

    // 2. Добавляем ребра для автобусных маршрутов
    double speed_m_per_min = (settings_.bus_velocity * 1000) / 60;

    for (const auto& [bus_name, bus] : catalogue_.GetAllBuses()) {
        const auto& stops = bus->route;
        if (stops.empty()) continue;

        // Проверяем, что все остановки существуют
        bool all_stops_exist = true;
        for (const auto& stop_name : stops) {
            if (stop_to_bus_vertex_.count(stop_name) == 0) {
                all_stops_exist = false;
                break;
            }
        }
        if (!all_stops_exist) continue;

        if (bus->is_roundtrip) {
            // Для кольцевых маршрутов - только последовательные остановки
            for (size_t i = 0; i + 1 < stops.size(); ++i) {
                double accumulated_time = 0;

                for (size_t j = i + 1; j < stops.size(); ++j) {
                    accumulated_time += CalculateSegmentTime(stops[j-1], stops[j], speed_m_per_min);
                    AddBusEdge(bus_name, stops[i], stops[j], accumulated_time, j - i);
                }
            }
        } else {
            // Для некольцевых маршрутов - прямое направление
            for (size_t i = 0; i + 1 < stops.size(); ++i) {
                double accumulated_time = 0;

                for (size_t j = i + 1; j < stops.size(); ++j) {
                    accumulated_time += CalculateSegmentTime(stops[j-1], stops[j], speed_m_per_min);
                    AddBusEdge(bus_name, stops[i], stops[j], accumulated_time, j - i);
                }
            }

            // Для некольцевых маршрутов - обратное направление
            for (size_t i = stops.size() - 1; i > 0; --i) {
                double accumulated_time = 0;

                for (size_t j = i; j > 0; --j) {
                    accumulated_time += CalculateSegmentTime(stops[j], stops[j-1], speed_m_per_min);
                    AddBusEdge(bus_name, stops[i], stops[j-1], accumulated_time, i - j + 1);

                    if (j == 0) break; // Защита от underflow
                }
            }
        }
    }

    router_ = std::make_unique<graph::Router<double>>(graph_);
}

std::optional<RouteInfo> TransportRouter::FindRoute(std::string_view from, std::string_view to) const {
    if (stop_to_wait_vertex_.count(std::string(from)) == 0 || stop_to_wait_vertex_.count(std::string(to)) == 0) {
        return std::nullopt;
    }

    size_t from_vertex = stop_to_wait_vertex_.at(std::string(from));
    size_t to_vertex = stop_to_wait_vertex_.at(std::string(to));

    auto route = router_->BuildRoute(from_vertex, to_vertex);
    if (!route) {
        return std::nullopt;
    }

    RouteInfo result;
    result.total_time = route->weight;

    // Восстанавливаем маршрут из ребер
    for (size_t edge_id : route->edges) {
        const auto& edge = graph_.GetEdge(edge_id);

        if (edge_to_bus_info_.count(edge_id)) {
            // Это ребро автобуса
            const auto& [bus_name, span_count] = edge_to_bus_info_.at(edge_id);
            result.items.push_back(BusItem{bus_name, span_count, edge.weight});
        } else {
            // Это ребро ожидания
            std::string stop_name = vertex_to_stop_.at(edge.from);
            result.items.push_back(WaitItem{stop_name, edge.weight});
        }
    }

    return result;
}

} // namespace transport
