#pragma once
#include "transport_catalogue.h"
#include "json.h"
#include <iostream>
#include <vector>
#include <cstddef>
#include "map_renderer.h"
#include "transport_router.h"


/*
 * Здесь можно разместить код наполнения транспортного справочника данными из JSON,
 * а также код обработки запросов к базе и формирование массива ответов в формате JSON
 */

namespace json_reader{
struct RenderSettings {
    double width = 0;
    double height = 0;
    double padding = 0;
    double line_width = 0;
    double stop_radius = 0;
    int bus_label_font_size = 0;
    svg::Point bus_label_offset;
    int stop_label_font_size = 0;
    svg::Point stop_label_offset;
    svg::Color underlayer_color;
    double underlayer_width = 0;
    std::vector<svg::Color> color_palette;
};
class JsonReader{
public:
    JsonReader(transport_catalogue::TransportCatalogue& catalogue): catalogue_(catalogue){}
    void LoadData(std::istream& input);
    void ProcessRequests(std::ostream& output);
    void RenderMap(std::ostream& output) const;
    json::Dict PrinMapInf(const json::Dict& dict);
    json::Dict PrintRouteInf(const json::Dict& dict);
private:
    transport_catalogue::TransportCatalogue& catalogue_;
    RenderSettings render_settings_;
    std::vector<json::Dict> stats_;
    transport::RoutingSettings routing_settings_;
    std::unique_ptr<transport::TransportRouter> router_;
};
}
