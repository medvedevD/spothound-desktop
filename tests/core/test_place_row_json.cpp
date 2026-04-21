#include <gtest/gtest.h>
#include "core/place_row_json.h"

TEST(PlaceRowJson, RoundTrip) {
    core::PlaceRow original;
    original.source  = "yandex";
    original.query   = "кафе москва";
    original.name    = "Кафе Центральное";
    original.address = "ул. Ленина, 1";
    original.phone   = "+7-999-000-00-00";
    original.site    = "https://example.ru";
    original.descr   = "Хорошее место";
    original.score   = 3;
    original.why     = "kw:кафе, site";

    nlohmann::json j = original;
    const core::PlaceRow restored = j.get<core::PlaceRow>();

    EXPECT_EQ(restored.source,  original.source);
    EXPECT_EQ(restored.query,   original.query);
    EXPECT_EQ(restored.name,    original.name);
    EXPECT_EQ(restored.address, original.address);
    EXPECT_EQ(restored.phone,   original.phone);
    EXPECT_EQ(restored.site,    original.site);
    EXPECT_EQ(restored.descr,   original.descr);
    EXPECT_EQ(restored.score,   original.score);
    EXPECT_EQ(restored.why,     original.why);
}

TEST(PlaceRowJson, DefaultValues) {
    core::PlaceRow r;
    nlohmann::json j = r;
    const core::PlaceRow restored = j.get<core::PlaceRow>();
    EXPECT_EQ(restored.score, 0);
    EXPECT_TRUE(restored.name.empty());
}
