#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace core {

struct ScrapeStats {
    std::string source;
    std::string query;
    std::string city;
    int gridN = 0;
    int gridCells = 0;
    int64_t collectionMs = 0;
    int64_t parsingMs = 0;
    int64_t totalCardProcessMs = 0;
    int queuedCards = 0;
    int cardCount = 0;
    int failedCards = 0;
    int blockedCards = 0;
    int probeRetries = 0;
    int64_t minCardMs = -1;
    int64_t maxCardMs = 0;

    int64_t avgCardMs() const {
        return cardCount > 0 ? totalCardProcessMs / cardCount : 0;
    }

    int64_t interCardMs() const {
        return cardCount > 0 ? (parsingMs - totalCardProcessMs) / cardCount : 0;
    }
};

inline void to_json(nlohmann::json& j, const ScrapeStats& s) {
    const int64_t minMs = s.minCardMs < 0 ? 0 : s.minCardMs;
    j = {
        {"source", s.source},
        {"query", s.query},
        {"city", s.city},
        {"total_ms", s.collectionMs + s.parsingMs},
        {"collection_ms", s.collectionMs},
        {"parsing_ms", s.parsingMs},
        {"grid_n", s.gridN},
        {"grid_cells", s.gridCells},
        {"queued_cards", s.queuedCards},
        {"card_count", s.cardCount},
        {"failed_cards", s.failedCards},
        {"blocked_cards", s.blockedCards},
        {"probe_retries", s.probeRetries},
        {"min_card_ms", minMs},
        {"max_card_ms", s.maxCardMs},
        {"avg_card_ms", s.avgCardMs()},
        {"inter_card_delay_ms", s.interCardMs()},
    };
}

} // namespace core
