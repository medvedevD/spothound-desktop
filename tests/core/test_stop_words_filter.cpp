#include <gtest/gtest.h>
#include "core/stop_words_filter.h"

TEST(StopWordsFilter, EmptyListNoMatch) {
    core::StopWordsFilter f;
    EXPECT_FALSE(f.matches("кафе бар ресторан"));
}

TEST(StopWordsFilter, WholeWordMatch) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    EXPECT_TRUE(f.matches("лучшее кафе в городе"));
}

TEST(StopWordsFilter, NoPartialMatch) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    EXPECT_FALSE(f.matches("кафетерий"));
}

TEST(StopWordsFilter, MatchAtStart) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    EXPECT_TRUE(f.matches("кафе на углу"));
}

TEST(StopWordsFilter, MatchAtEnd) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    EXPECT_TRUE(f.matches("открытое кафе"));
}

TEST(StopWordsFilter, CaseInsensitive) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    EXPECT_TRUE(f.matches("Лучшее КАФЕ в городе"));
}

TEST(StopWordsFilter, YoNormalization) {
    core::StopWordsFilter f;
    f.setWords({"ежик"});
    EXPECT_TRUE(f.matches("ёжик в тумане"));
}

TEST(StopWordsFilter, MultipleWords) {
    core::StopWordsFilter f;
    f.setWords({"автомойка", "парикмахерская"});
    EXPECT_TRUE(f.matches("лучшая автомойка"));
    EXPECT_TRUE(f.matches("женская парикмахерская"));
    EXPECT_FALSE(f.matches("обычное кафе"));
}

TEST(StopWordsFilter, SetWordsReplaces) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    f.setWords({"бар"});
    EXPECT_FALSE(f.matches("хорошее кафе"));
    EXPECT_TRUE(f.matches("хороший бар"));
}

TEST(StopWordsFilter, MatchesPlaceOnName) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    core::PlaceRow r;
    r.name = "Лучшее кафе";
    EXPECT_TRUE(f.matchesPlace(r));
}

TEST(StopWordsFilter, MatchesPlaceOnDescription) {
    core::StopWordsFilter f;
    f.setWords({"бар"});
    core::PlaceRow r;
    r.descr = "Уютный бар для встреч";
    EXPECT_TRUE(f.matchesPlace(r));
}

TEST(StopWordsFilter, MatchesPlaceNoStopWord) {
    core::StopWordsFilter f;
    f.setWords({"кафе"});
    core::PlaceRow r;
    r.name = "Парикмахерская";
    r.descr = "Стрижка и укладка";
    EXPECT_FALSE(f.matchesPlace(r));
}

TEST(StopWordsFilter, MatchesPlaceOnHostname) {
    core::StopWordsFilter f;
    f.setWords({"example"});
    core::PlaceRow r;
    r.name = "Название";
    r.site = "https://example.com/page";
    EXPECT_TRUE(f.matchesPlace(r));
}

TEST(StopWordsFilter, MatchesPlaceMultipleHostnames) {
    core::StopWordsFilter f;
    f.setWords({"blocked"});
    core::PlaceRow r;
    r.site = "https://ok.com/, https://blocked.ru/x";
    EXPECT_TRUE(f.matchesPlace(r));
}

TEST(StopWordsFilter, MatchesPlaceBareHostNotExtracted) {
    // Matches previous QUrl behavior: bare hostnames without scheme are not
    // extracted as hosts, but the bare string still appears in the text blob.
    core::StopWordsFilter f;
    f.setWords({"example"});
    core::PlaceRow r;
    r.site = "example.com";
    EXPECT_TRUE(f.matchesPlace(r));
}

TEST(StopWordsFilter, MatchesPlaceStripsPortAndUserInfo) {
    core::StopWordsFilter f;
    f.setWords({"host"});
    core::PlaceRow r;
    r.site = "https://user:pass@host.com:8080/path";
    EXPECT_TRUE(f.matchesPlace(r));
}
