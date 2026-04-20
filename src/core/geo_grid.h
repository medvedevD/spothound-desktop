#pragma once
#include "city_registry.h"
#include <vector>

namespace core {

struct GeoPoint { double lon, lat; };

struct GeoGrid {
    std::vector<GeoPoint> cells;
    int    zoom       = 14;
    int    yandexRid  = 213;
    std::string yandexSlug = "moscow";
};

// Builds an n×n center-point grid over the city's bounding box.
// Falls back to Moscow defaults when city == nullptr.
GeoGrid buildGrid(const CityGeo* city, int n);

} // namespace core
