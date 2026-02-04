#pragma once
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "geo.h"
#include "domain.h"

namespace transport_catalogue
{
struct BusRouteInfo {
    size_t stops_on_route;
    size_t unique_stops;
    int fact_length;
    double curvature;
};

struct Hasher {
    size_t operator()(const std::pair<const Stop*, const Stop*>& pair) const {
        return ptr_hasher(pair.first) * 37 + ptr_hasher(pair.second) * 37 * 37;
    }
    std::hash<const void*> ptr_hasher;
};

class TransportCatalogue {
public:
    void AddStop(const Stop& stop);
    void AddBus(const Bus& bus);
    const Stop* FindStop(const std::string& name) const;
    const Bus* FindBus(const std::string& name) const;
    BusRouteInfo GetBusRoute(const std::string& name) const;
    const std::unordered_set<std::string>& GetBusesByStop(const std::string& name) const;
    void SetDistanceToStops(std::string_view from, std::string_view to, int distance);
    int GetDistanceToStops(const Stop* stop1, const Stop* stop2) const;
    const std::unordered_map<std::string,const Stop*>& GetAllStops() const {
        return stopname_to_stop_;
    }

    const std::unordered_map<std::string,const Bus*>& GetAllBuses() const {
        return busname_to_bus_;
    }

private:
    std::deque<Stop> stops_;
    std::unordered_map<std::string, const Stop*> stopname_to_stop_;
    std::deque<Bus> buses_;
    std::unordered_map<std::string, const Bus*> busname_to_bus_;
    std::unordered_map<std::string, std::unordered_set<std::string>> stopname_to_buses_;
    std::unordered_map<std::pair<const Stop*, const Stop*>, int, Hasher> distance_to_stops;
};
}
