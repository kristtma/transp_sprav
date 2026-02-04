#include "transport_catalogue.h"
#include <numeric>
#include <iostream>
#include <string>
#include <vector>

namespace transport_catalogue
{
void TransportCatalogue::AddStop(const Stop& stop) {
    stops_.push_back(stop);
    stopname_to_stop_[stops_.back().name] = &stops_.back();
}

void TransportCatalogue::AddBus(const Bus& bus) {
    buses_.push_back(bus);
    busname_to_bus_[buses_.back().name] = &buses_.back();
    for (const auto& stop_name : bus.route) {
        stopname_to_buses_[stop_name].insert(bus.name);
    }
}

const Stop* TransportCatalogue::FindStop(const std::string& name) const {
    auto it = stopname_to_stop_.find(name);
    return it != stopname_to_stop_.end() ? it->second : nullptr;
}

const Bus* TransportCatalogue::FindBus(const std::string& name) const {
    auto it = busname_to_bus_.find(name);
    return it != busname_to_bus_.end() ? it->second : nullptr;
}

const std::unordered_set<std::string>& TransportCatalogue::GetBusesByStop(const std::string& name) const {
    static std::unordered_set<std::string> empty;
    auto it = stopname_to_buses_.find(name);
    return it != stopname_to_buses_.end() ? it->second : empty;
}


BusRouteInfo TransportCatalogue::GetBusRoute(const std::string& name) const {
    const Bus* bus = FindBus(name);
    if (!bus || bus->route.empty()) {
        return {0, 0, 0, 0.0};
    }

    std::unordered_set<std::string> unique_stops(bus->route.begin(), bus->route.end());
    double geo_length = 0.0;
    int fact_length = 0;

    // Рассчитываем длину прямого маршрута
    for (size_t i = 0; i < bus->route.size() - 1; ++i) {
        const Stop* from = FindStop(bus->route[i]);
        const Stop* to = FindStop(bus->route[i + 1]);
        if (from && to) {
            geo_length += ComputeDistance(from->coordinates, to->coordinates);
            fact_length += GetDistanceToStops(from, to);
        }
    }

    // Для некольцевого маршрута добавляем обратный путь
    if (!bus->is_roundtrip) {
        for (size_t i = bus->route.size() - 1; i > 0; --i) {
            const Stop* from = FindStop(bus->route[i]);
            const Stop* to = FindStop(bus->route[i - 1]);
            if (from && to) {
                geo_length += ComputeDistance(from->coordinates, to->coordinates);
                fact_length += GetDistanceToStops(from, to);
            }
        }
    }

    double curvature = geo_length > 0 ? fact_length / geo_length : 0.0;
    return {bus->route.size(), unique_stops.size(), fact_length, curvature};
}
void TransportCatalogue::SetDistanceToStops(std::string_view from, std::string_view to, int distance) {
    const Stop* from_stop = FindStop(std::string(from));
    const Stop* to_stop = FindStop(std::string(to));
    if (from_stop && to_stop) {
        distance_to_stops[{from_stop, to_stop}] = distance;
    } else {
    }
}

int TransportCatalogue::GetDistanceToStops(const Stop* stop1, const Stop* stop2) const {
    auto it1 = distance_to_stops.find({stop1, stop2});
    if (it1 != distance_to_stops.end()) {
        return it1->second;
    } else {
        auto it2 = distance_to_stops.find({stop2, stop1});
        if (it2 != distance_to_stops.end()) {
            return it2->second;
        } else {
            return 0;
        }
    }
}
}
