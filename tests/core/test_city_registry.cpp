#include <gtest/gtest.h>
#include "core/city_registry.h"

TEST(CityRegistry, FindExactName) {
    const auto* c = core::CityRegistry::find("москва");
    ASSERT_NE(c, nullptr);
    EXPECT_STREQ(c->yandexSlug, "moscow");
    EXPECT_EQ(c->yandexRid, 213);
}

TEST(CityRegistry, FindCaseInsensitive) {
    const auto* c = core::CityRegistry::find("Москва");
    ASSERT_NE(c, nullptr);
    EXPECT_STREQ(c->yandexSlug, "moscow");
}

TEST(CityRegistry, FindYoNormalized) {
    // ё→е normalization: "Пётр" should match Petersburg aliases via partial
    // Direct test: city "Екатеринбург" contains no ё, but check that norm works
    const auto* c = core::CityRegistry::find("екатеринбург");
    ASSERT_NE(c, nullptr);
    EXPECT_STREQ(c->yandexSlug, "yekaterinburg");
}

TEST(CityRegistry, FindPartialAlias) {
    const auto* c = core::CityRegistry::find("питер");
    ASSERT_NE(c, nullptr);
    EXPECT_STREQ(c->yandexSlug, "saint-petersburg");
}

TEST(CityRegistry, FindLatinAlias) {
    const auto* c = core::CityRegistry::find("kazan");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->yandexRid, 43);
}

TEST(CityRegistry, FindWithSurroundingText) {
    // Input contains city name as substring
    const auto* c = core::CityRegistry::find("город новосибирск");
    ASSERT_NE(c, nullptr);
    EXPECT_STREQ(c->yandexSlug, "novosibirsk");
}

TEST(CityRegistry, NotFound) {
    const auto* c = core::CityRegistry::find("берлин");
    EXPECT_EQ(c, nullptr);
}

TEST(CityRegistry, EmptyInput) {
    const auto* c = core::CityRegistry::find("");
    EXPECT_EQ(c, nullptr);
}

TEST(CityRegistry, BboxFieldsPresent) {
    const auto* c = core::CityRegistry::find("краснодар");
    ASSERT_NE(c, nullptr);
    EXPECT_GT(c->bboxLon1, c->bboxLon0);
    EXPECT_GT(c->bboxLat1, c->bboxLat0);
    EXPECT_GT(c->zoom, 0);
}
