#include <gtest/gtest.h>
#include "core/geo_grid.h"
#include "core/city_registry.h"

TEST(GeoGrid, CellCountMatchesN) {
    core::GeoGrid g = core::buildGrid(nullptr, 3);
    EXPECT_EQ(g.cells.size(), 9u);
}

TEST(GeoGrid, FallbackDefaults) {
    core::GeoGrid g = core::buildGrid(nullptr, 2);
    EXPECT_EQ(g.zoom, 14);
    EXPECT_EQ(g.yandexRid, 213);
    EXPECT_EQ(g.yandexSlug, "moscow");
}

TEST(GeoGrid, CellsWithinBbox) {
    core::GeoGrid g = core::buildGrid(nullptr, 5);
    for (const auto& p : g.cells) {
        EXPECT_GT(p.lon, 37.29);
        EXPECT_LT(p.lon, 37.91);
        EXPECT_GT(p.lat, 55.54);
        EXPECT_LT(p.lat, 55.96);
    }
}

TEST(GeoGrid, KnownCitySlug) {
    const core::CityGeo* c = core::CityRegistry::find("москва");
    ASSERT_NE(c, nullptr);
    core::GeoGrid g = core::buildGrid(c, 2);
    EXPECT_EQ(g.yandexSlug, c->yandexSlug);
    EXPECT_EQ(g.yandexRid, c->yandexRid);
    EXPECT_EQ(g.zoom, c->zoom);
    EXPECT_EQ(g.cells.size(), 4u);
}
