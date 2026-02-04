#include "json_reader.h"
#include <algorithm>
#include <string>
#include <iomanip>
#include <iomanip>
#include <sstream>
#include "json_builder.h"

using namespace std::literals;

namespace json_reader{
transport_catalogue::Bus ParseBus(const json::Dict& dict){
    transport_catalogue::Bus bus;
    bus.name = std::string(dict.at("name").AsString());
    const auto& stops_array = dict.at("stops").AsArray();
    bus.route.reserve(stops_array.size());
    for (const auto& stop_node : stops_array) {
        bus.route.emplace_back(stop_node.AsString());
    }
    bus.is_roundtrip = dict.at("is_roundtrip").AsBool();
    return bus;
}
transport_catalogue::Stop ParseStop(const json::Dict& dict){
    transport_catalogue::Stop stop;
    stop.name = dict.at("name").AsString();
    stop.coordinates = {dict.at("latitude").AsDouble(), dict.at("longitude").AsDouble()};
    return stop;
}
transport::RoutingSettings ParseRoutingSettings(const json::Dict& dict) {
    transport::RoutingSettings settings;
    settings.bus_wait_time = dict.at("bus_wait_time").AsInt();
    settings.bus_velocity = dict.at("bus_velocity").AsDouble();
    return settings;
}
svg::Color ParseColor(const json::Node& node) {

    if (node.IsString()) {
        return node.AsString();
    }

    const auto& arr = node.AsArray();
    if (arr.size() == 3) {
        return "rgb("s +
               std::to_string(arr[0].AsInt()) + ","s +
               std::to_string(arr[1].AsInt()) + ","s +
               std::to_string(arr[2].AsInt()) + ")"s;
    } else if (arr.size() == 4) {
        std::ostringstream rgba_stream;
        rgba_stream << "rgba("
                    << arr[0].AsInt() << ","
                    << arr[1].AsInt() << ","
                    << arr[2].AsInt() << ","
                    << std::setprecision(6) << arr[3].AsDouble() << ")";
        return rgba_stream.str();
    }
    return "none"s;
}
RenderSettings ParseRenderSettings(const json::Dict& dict) {
    RenderSettings settings;
    settings.width = dict.at("width").AsDouble();
    settings.height = dict.at("height").AsDouble();
    settings.padding = dict.at("padding").AsDouble();
    settings.line_width = dict.at("line_width").AsDouble();
    settings.stop_radius = dict.at("stop_radius").AsDouble();
    settings.bus_label_font_size = dict.at("bus_label_font_size").AsInt();

    const auto& bus_label_offset = dict.at("bus_label_offset").AsArray();
    settings.bus_label_offset = {bus_label_offset[0].AsDouble(),
                                 bus_label_offset[1].AsDouble()};

    settings.stop_label_font_size = dict.at("stop_label_font_size").AsInt();

    const auto& stop_label_offset = dict.at("stop_label_offset").AsArray();
    settings.stop_label_offset = {stop_label_offset[0].AsDouble(),
                                  stop_label_offset[1].AsDouble()};

    settings.underlayer_color = ParseColor(dict.at("underlayer_color"));
    settings.underlayer_width = dict.at("underlayer_width").AsDouble();

    const auto& palette = dict.at("color_palette").AsArray();
    settings.color_palette.reserve(palette.size());
    for (const auto& color_node : palette) {
        settings.color_palette.emplace_back(ParseColor(color_node));
    }

    return settings;
}
void JsonReader::LoadData(std::istream& input){
    json::Document doc = json::Load(input);
    const json::Dict& map = doc.GetRoot().AsMap();
    const json::Array& base_requests = map.at("base_requests").AsArray();
    struct DistanceStops{
        std::string to;
        std::string from;
        int dist;
    };
    std::vector<DistanceStops> road_distances_array;
    road_distances_array.reserve(base_requests.size());
    for(auto node : base_requests){
        const json::Dict& dict = node.AsMap();
        if(dict.at("type") == "Stop"){
            transport_catalogue::Stop stop = ParseStop(dict);
            catalogue_.AddStop(stop);
            json::Dict road_distances = dict.at("road_distances").AsMap();
            for(const auto& [to, distance] : road_distances){
                road_distances_array.push_back({stop.name, to, distance.AsInt()});
            }
        }
    }
    for(const auto& [from, to, distance] : road_distances_array){
        catalogue_.SetDistanceToStops(from, to, distance);
    }
    for(auto node : base_requests){
        const json::Dict dict = node.AsMap();
        if(dict.at("type") == "Bus"){
            catalogue_.AddBus(ParseBus(dict));
        }
    }
    render_settings_ = ParseRenderSettings(map.at("render_settings").AsMap());
    json::Array stat_requests = map.at("stat_requests").AsArray();
    for(auto node : stat_requests){
        const json::Dict& dict = node.AsMap();
        stats_.push_back(dict);
    }
    routing_settings_ = ParseRoutingSettings(map.at("routing_settings").AsMap());
    router_ = std::make_unique<transport::TransportRouter>(catalogue_, routing_settings_);
    //catalogue_.SetRouteSettings(ParseRouteSettings(map.at("routing_settings").AsMap()));
}

void JsonReader::RenderMap(std::ostream& output) const {
    // 1. Собираем все остановки, которые входят в маршруты
    std::unordered_set<const transport_catalogue::Stop*> stops_in_routes;
    std::vector<transport_catalogue::Coordinates> stops_coords;
    // Сначала собираем все автобусы и их остановки
    for (const auto& [name, bus_ptr] : catalogue_.GetAllBuses()) {
        for (const auto& stop_name : bus_ptr->route) {
            const auto* stop = catalogue_.FindStop(stop_name);
            if (stop && stops_in_routes.insert(stop).second) {
                stops_coords.emplace_back(stop->coordinates);
            }
        }
    }
    // 2. Инициализируем проектор только с остановками из маршрутов
    SphereProjector projector(stops_coords.begin(), stops_coords.end(),
                              render_settings_.width, render_settings_.height,
                              render_settings_.padding);

    svg::Document doc;

    // 3. Собираем и сортируем автобусы по имени
    std::vector<const transport_catalogue::Bus*> buses;
    for (const auto& [name, bus_ptr] : catalogue_.GetAllBuses()) {
        buses.emplace_back(bus_ptr);
    }
    std::sort(buses.begin(), buses.end(), [](const auto& lhs, const auto& rhs) {
        return lhs->name < rhs->name;
    });

    // 4. Рисуем линии маршрутов
    for (size_t i = 0; i < buses.size(); ++i) {
        const auto& bus = buses[i];
        if (bus->route.empty()) continue;

        svg::Polyline polyline;
        polyline.SetStrokeColor(render_settings_.color_palette[i % render_settings_.color_palette.size()]);
        polyline.SetStrokeWidth(render_settings_.line_width);
        polyline.SetStrokeLineCap(svg::StrokeLineCap::ROUND);
        polyline.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
        polyline.SetFillColor("none");

        // Добавляем точки для прямого маршрута
        for (const auto& stop_name : bus->route) {
            const auto* stop = catalogue_.FindStop(stop_name);
            if (stop) {
                polyline.AddPoint(projector(stop->coordinates));
            }
        }
        // Для некольцевого маршрута добавляем обратный путь (кроме последней остановки)
        if (!bus->is_roundtrip) {
            for (auto it = bus->route.rbegin() + 1; it != bus->route.rend(); ++it) {
                const auto* stop = catalogue_.FindStop(*it);
                if (stop) {
                    polyline.AddPoint(projector(stop->coordinates));
                }
            }
        }
        doc.Add(std::move(polyline));
    }
    for (size_t i = 0; i < buses.size(); ++i){
        const auto& bus = buses[i];
        if (bus->route.empty()) continue;
        const auto* first_stop = catalogue_.FindStop(bus->route.front());
        if (first_stop) {
            svg::Text underlayer;
            underlayer.SetPosition(projector(first_stop->coordinates));
            underlayer.SetOffset(render_settings_.bus_label_offset);
            underlayer.SetFillColor(render_settings_.underlayer_color);
            underlayer.SetStrokeColor(render_settings_.underlayer_color);
            underlayer.SetStrokeWidth(render_settings_.underlayer_width);
            underlayer.SetStrokeLineCap(svg::StrokeLineCap::ROUND);
            underlayer.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
            underlayer.SetFontSize(render_settings_.bus_label_font_size);
            underlayer.SetFontFamily("Verdana");
            underlayer.SetFontWeight("bold");
            underlayer.SetData(bus->name);
            doc.Add(std::move(underlayer));

            // Затем рисуем основной текст
            svg::Text text;
            text.SetPosition(projector(first_stop->coordinates));
            text.SetOffset(render_settings_.bus_label_offset);
            text.SetFillColor(render_settings_.color_palette[i % render_settings_.color_palette.size()]);
            text.SetFontSize(render_settings_.bus_label_font_size);
            text.SetFontFamily("Verdana");
            text.SetFontWeight("bold");
            text.SetData(bus->name);
            doc.Add(std::move(text));
        }
        if(!bus->is_roundtrip){
            const auto* last_stop = catalogue_.FindStop(bus->route.back());
            if (last_stop && last_stop != first_stop) {
                svg::Text underlayer;
                underlayer.SetPosition(projector(last_stop->coordinates));
                underlayer.SetOffset(render_settings_.bus_label_offset);
                underlayer.SetFillColor(render_settings_.underlayer_color);
                underlayer.SetStrokeColor(render_settings_.underlayer_color);
                underlayer.SetStrokeWidth(render_settings_.underlayer_width);
                underlayer.SetStrokeLineCap(svg::StrokeLineCap::ROUND);
                underlayer.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
                underlayer.SetFontSize(render_settings_.bus_label_font_size);
                underlayer.SetFontFamily("Verdana");
                underlayer.SetFontWeight("bold");
                underlayer.SetData(bus->name);
                doc.Add(std::move(underlayer));

                // Затем рисуем основной текст
                svg::Text text;
                text.SetPosition(projector(last_stop->coordinates));
                text.SetOffset(render_settings_.bus_label_offset);
                text.SetFillColor(render_settings_.color_palette[i % render_settings_.color_palette.size()]);
                text.SetFontSize(render_settings_.bus_label_font_size);
                text.SetFontFamily("Verdana");
                text.SetFontWeight("bold");
                text.SetData(bus->name);
                doc.Add(std::move(text));
            }
        }
    }
    std::vector<const transport_catalogue::Stop*> sorted_stops(stops_in_routes.begin(), stops_in_routes.end());
    std::sort(sorted_stops.begin(), sorted_stops.end(), [](const auto* lhs, const auto* rhs) {return lhs->name < rhs->name;});
    for (const auto& stop : sorted_stops){
        svg::Circle circle;
        circle.SetCenter(projector(stop->coordinates));
        circle.SetRadius(render_settings_.stop_radius);
        circle.SetFillColor("white");
        doc.Add(std::move(circle));
    }
    for (const auto& stop : sorted_stops) {
        svg::Text underlayer;
        underlayer.SetPosition(projector(stop->coordinates));
        underlayer.SetOffset(render_settings_.stop_label_offset);
        underlayer.SetFillColor(render_settings_.underlayer_color);
        underlayer.SetStrokeColor(render_settings_.underlayer_color);
        underlayer.SetStrokeWidth(render_settings_.underlayer_width);
        underlayer.SetStrokeLineCap(svg::StrokeLineCap::ROUND);
        underlayer.SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);
        underlayer.SetFontSize(render_settings_.stop_label_font_size);
        underlayer.SetFontFamily("Verdana");
        underlayer.SetData(stop->name);
        doc.Add(std::move(underlayer));
        svg::Text text;
        text.SetPosition(projector(stop->coordinates));
        text.SetOffset(render_settings_.stop_label_offset);
        text.SetFontSize(render_settings_.stop_label_font_size);
        text.SetFontFamily("Verdana");
        text.SetData(stop->name);
        text.SetFillColor("black");
        doc.Add(std::move(text));
    }
    doc.Render(output);
}

json::Dict PrintBusInf(const json::Dict& dict, transport_catalogue::TransportCatalogue& catalogue){
    int id  = dict.at("id").AsInt();
    json::Builder builder;
    const transport_catalogue::Bus* bus = catalogue.FindBus(dict.at("name").AsString());
    if (!bus) {
        return builder.StartDict().Key("request_id").Value(id).Key("error_message").Value("not found").EndDict().Build().AsMap();
    }
    transport_catalogue::BusRouteInfo bus_inf = catalogue.GetBusRoute(dict.at("name").AsString());
    size_t stop_count = bus->is_roundtrip ? bus_inf.stops_on_route : bus_inf.stops_on_route * 2 - 1;
    return builder.StartDict()
        .Key("curvature").Value(bus_inf.curvature)
        .Key("request_id").Value(id)
        .Key("route_length").Value(bus_inf.fact_length)
        .Key("stop_count").Value(static_cast<int>(stop_count))
        .Key("unique_stop_count").Value(static_cast<int>(bus_inf.unique_stops))
        .EndDict().Build().AsMap();
}
json::Dict PrintStopInf(const json::Dict& dict, transport_catalogue::TransportCatalogue& catalogue) {
    int id = dict.at("id").AsInt();
    json::Builder builder;
    const transport_catalogue::Stop* stop = catalogue.FindStop(dict.at("name").AsString());
    if (!stop) {
        return builder.StartDict().Key("request_id").Value(id).Key("error_message").Value("not found").EndDict().Build().AsMap();
    }

    const auto& buses = catalogue.GetBusesByStop(dict.at("name").AsString());
    std::vector<std::string> buses_vector(buses.begin(), buses.end());
    std::sort(buses_vector.begin(), buses_vector.end());

    json::Array buses_array;
    buses_array.reserve(buses_vector.size());
    for (const auto& bus : buses_vector) {
        buses_array.emplace_back(bus);
    }
    return builder.StartDict().Key("request_id").Value(id).Key("buses").Value(std::move(buses_array)).EndDict().Build().AsMap();
}
json::Dict JsonReader::PrinMapInf(const json::Dict& dict){
    int id = dict.at("id").AsInt();
    json::Builder builder;
    std::ostringstream map_stream;
    this->JsonReader::RenderMap(map_stream);
    std::string svg_content = map_stream.str();
    return builder.StartDict().Key("map").Value(std::move(svg_content)).Key("request_id").Value(id).EndDict().Build().AsMap();
}

// Обновить функцию PrintRouteInf
json::Dict JsonReader::PrintRouteInf(const json::Dict& dict) {
    int id = dict.at("id").AsInt();
    std::string from = dict.at("from").AsString();
    std::string to = dict.at("to").AsString();
    auto route_info = router_->FindRoute(from, to);
    if (!route_info) {
        return json::Builder{}
            .StartDict()
            .Key("request_id").Value(id)
            .Key("error_message").Value("not found")
            .EndDict()
            .Build()
            .AsMap();
    }

    // Создаем массив items
    json::Array items_array;
    for (const auto& item : route_info->items) {
        if (std::holds_alternative<transport::WaitItem>(item)) {
            const auto& wait_item = std::get<transport::WaitItem>(item);
            items_array.push_back(
                json::Builder{}
                    .StartDict()
                    .Key("type").Value("Wait")
                    .Key("stop_name").Value(wait_item.stop_name)
                    .Key("time").Value(wait_item.time)
                    .EndDict()
                    .Build()
                    .AsMap()
                );
        } else {
            const auto& bus_item = std::get<transport::BusItem>(item);
            items_array.push_back(
                json::Builder{}
                    .StartDict()
                    .Key("type").Value("Bus")
                    .Key("bus").Value(bus_item.bus)
                    .Key("span_count").Value(bus_item.span_count)
                    .Key("time").Value(bus_item.time)
                    .EndDict()
                    .Build()
                    .AsMap()
                );
        }
    }

    return json::Builder{}
        .StartDict()
        .Key("request_id").Value(id)
        .Key("total_time").Value(route_info->total_time)
        .Key("items").Value(std::move(items_array))
        .EndDict()
        .Build()
        .AsMap();
}
void JsonReader::ProcessRequests(std::ostream& output) {
    json::Array print_stats;
    for(auto node : stats_){
        const json::Dict& dict = node;
        if(dict.at("type") == "Bus"){
            print_stats.push_back(PrintBusInf(dict, catalogue_));
        }else if(dict.at("type") == "Stop"){
            print_stats.push_back(PrintStopInf(dict, catalogue_));
        }else if(dict.at("type") == "Map"){
            print_stats.push_back(this->PrinMapInf(dict));
        }
        else if(dict.at("type") == "Route"){
            print_stats.push_back(PrintRouteInf(dict));
        }
    }
    json::Print(json::Document{std::move(print_stats)}, output);
}
}
