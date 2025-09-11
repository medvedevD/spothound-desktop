#ifndef CITYREGISTRY_H
#define CITYREGISTRY_H

#include <QString>

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
    // Returns nullptr if not found (caller should fall back to Moscow defaults).
    static const CityGeo* find(const QString& cityName);
};

#endif // CITYREGISTRY_H
