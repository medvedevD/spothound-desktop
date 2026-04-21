#include <gtest/gtest.h>
#include "core/rules.h"

TEST(RulesNorm, Lowercase) {
    EXPECT_EQ(core::Rules::norm("КАФЕ"), "кафе");
}

TEST(RulesNorm, YoToYe) {
    EXPECT_EQ(core::Rules::norm("ёж"), "еж");
    EXPECT_EQ(core::Rules::norm("Ёж"), "еж");
}

TEST(RulesNorm, Trim) {
    EXPECT_EQ(core::Rules::norm("  кафе  "), "кафе");
}

TEST(RulesNorm, CollapseSpaces) {
    EXPECT_EQ(core::Rules::norm("кафе   бар"), "кафе бар");
}

TEST(RulesNorm, Empty) {
    EXPECT_EQ(core::Rules::norm(""), "");
}

TEST(RulesScore, NoKeywords) {
    auto [s, why] = core::Rules::score("кафе", "описание", false, {});
    EXPECT_EQ(s, 0);
    EXPECT_TRUE(why.empty());
}

TEST(RulesScore, KeywordMatch) {
    auto [s, why] = core::Rules::score("кафе центральное", "вкусно", false, {"кафе"});
    EXPECT_EQ(s, 1);
    EXPECT_NE(why.find("kw:кафе"), std::string::npos);
}

TEST(RulesScore, SiteBonus) {
    auto [s, why] = core::Rules::score("ресторан", "", true, {});
    EXPECT_EQ(s, 1);
    EXPECT_EQ(why, "site");
}

TEST(RulesScore, MultipleKeywordsAndSite) {
    auto [s, why] = core::Rules::score("кафе бар", "пиво", true, {"кафе", "бар"});
    EXPECT_EQ(s, 3);
}

TEST(RulesScore, KeywordNormalized) {
    // text is normalized before search; keyword "кафе" finds "КАФЕ" in name
    auto [s, why] = core::Rules::score("КАФЕ", "", false, {"кафе"});
    EXPECT_EQ(s, 1);
}
