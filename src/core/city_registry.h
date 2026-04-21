#pragma once
#include <string>

namespace core {

struct CityGeo {
    const char* names;       // comma-separated lowercase aliases for matching
    const char* yandexSlug;
    int         yandexRid;
    const char* twoGisSlug;
    double lon, lat;         // city center
    double bboxLon0, bboxLon1, bboxLat0, bboxLat1;
    int zoom;                // Yandex cell zoom
};

class CityRegistry {
public:
    // Case-insensitive partial match against city names/aliases.
    // Input is normalized (lowercase, ё→е). Returns nullptr if not found.
    static const CityGeo* find(const std::string& cityName);
};

} // namespace core
