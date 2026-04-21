#include <gtest/gtest.h>
#include "core/scrape_stats.h"

TEST(ScrapeStats, AvgCardMsZeroCards) {
    core::ScrapeStats s;
    EXPECT_EQ(s.avgCardMs(), 0);
}

TEST(ScrapeStats, AvgCardMsNonZero) {
    core::ScrapeStats s;
    s.cardCount = 4;
    s.totalCardProcessMs = 200;
    EXPECT_EQ(s.avgCardMs(), 50);
}

TEST(ScrapeStats, InterCardMsZeroCards) {
    core::ScrapeStats s;
    EXPECT_EQ(s.interCardMs(), 0);
}

TEST(ScrapeStats, InterCardMsNonZero) {
    core::ScrapeStats s;
    s.cardCount = 2;
    s.parsingMs = 300;
    s.totalCardProcessMs = 100;
    EXPECT_EQ(s.interCardMs(), 100);
}

TEST(ScrapeStats, ToJsonFields) {
    core::ScrapeStats s;
    s.source = "yandex";
    s.query  = "кафе";
    s.city   = "москва";
    s.gridN = 3; s.gridCells = 9;
    s.collectionMs = 5000; s.parsingMs = 3000;
    s.queuedCards = 10; s.cardCount = 8;
    s.failedCards = 1; s.blockedCards = 1;
    s.probeRetries = 2;
    s.minCardMs = 100; s.maxCardMs = 500;
    s.totalCardProcessMs = 2400;

    nlohmann::json j = s;
    EXPECT_EQ(j["source"], "yandex");
    EXPECT_EQ(j["query"],  "кафе");
    EXPECT_EQ(j["city"],   "москва");
    EXPECT_EQ(j["grid_n"], 3);
    EXPECT_EQ(j["grid_cells"], 9);
    EXPECT_EQ(j["total_ms"], 8000);
    EXPECT_EQ(j["collection_ms"], 5000);
    EXPECT_EQ(j["parsing_ms"], 3000);
    EXPECT_EQ(j["queued_cards"], 10);
    EXPECT_EQ(j["card_count"], 8);
    EXPECT_EQ(j["failed_cards"], 1);
    EXPECT_EQ(j["blocked_cards"], 1);
    EXPECT_EQ(j["probe_retries"], 2);
    EXPECT_EQ(j["min_card_ms"], 100);
    EXPECT_EQ(j["max_card_ms"], 500);
    EXPECT_EQ(j["avg_card_ms"], 300);
}

TEST(ScrapeStats, ToJsonMinCardMsNegativeNormalizesToZero) {
    core::ScrapeStats s;
    s.minCardMs = -1;
    nlohmann::json j = s;
    EXPECT_EQ(j["min_card_ms"], 0);
}

TEST(ScrapeStats, ToJsonInterCardDelay) {
    core::ScrapeStats s;
    s.cardCount = 5;
    s.parsingMs = 1000;
    s.totalCardProcessMs = 500;
    nlohmann::json j = s;
    EXPECT_EQ(j["inter_card_delay_ms"], 100);
}
