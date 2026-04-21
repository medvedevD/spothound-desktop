#include "geo_grid.h"

namespace core {

static constexpr double kDefaultLon0 = 37.30, kDefaultLon1 = 37.90;
static constexpr double kDefaultLat0 = 55.55, kDefaultLat1 = 55.95;
static constexpr int    kDefaultZoom = 14;

GeoGrid buildGrid(const CityGeo* city, int n)
{
    double lon0, lon1, lat0, lat1;
    GeoGrid g;

    if (city) {
        lon0 = city->bboxLon0; lon1 = city->bboxLon1;
        lat0 = city->bboxLat0; lat1 = city->bboxLat1;
        g.zoom       = city->zoom;
        g.yandexRid  = city->yandexRid;
        g.yandexSlug = city->yandexSlug;
    } else {
        lon0 = kDefaultLon0; lon1 = kDefaultLon1;
        lat0 = kDefaultLat0; lat1 = kDefaultLat1;
        g.zoom = kDefaultZoom;
    }

    g.cells.reserve(static_cast<size_t>(n * n));
    for (int iy = 0; iy < n; ++iy)
        for (int ix = 0; ix < n; ++ix)
            g.cells.push_back({ lon0 + (lon1 - lon0) * (ix + 0.5) / n,
                                 lat0 + (lat1 - lat0) * (iy + 0.5) / n });
    return g;
}

} // namespace core
