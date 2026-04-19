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
