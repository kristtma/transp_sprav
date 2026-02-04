#pragma once

namespace transport_catalogue {

struct Coordinates {
    double lat; // Широта
    double lng; // Долгота
};

double ComputeDistance(Coordinates from, Coordinates to);

}  // namespace geo