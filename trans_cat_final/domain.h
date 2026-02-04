#include "geo.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace transport_catalogue {
struct Stop{
    std::string name;
    Coordinates coordinates;
};
struct Bus {
    std::string name;
    std::vector<std::string> route;
    bool is_roundtrip;
};
}
