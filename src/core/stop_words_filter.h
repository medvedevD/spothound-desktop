#pragma once
#include "place_row.h"
#include <string>
#include <vector>

namespace core {

class StopWordsFilter {
public:
    void setWords(std::vector<std::string> words);
    const std::vector<std::string>& words() const { return m_words; }
    // Normalizes text internally (lowercase, ё→е) before matching.
    // Returns true if any stop-word appears at a word boundary in text.
    bool matches(const std::string& text) const;

    // Checks name/descr/address/site blob, then hostnames extracted from site
    // (comma-separated URLs). Returns true on the first hit.
    bool matchesPlace(const PlaceRow& r) const;

private:
    std::vector<std::string> m_words;
    std::vector<std::string> m_normalized;
};

} // namespace core
