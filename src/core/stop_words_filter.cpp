#include "stop_words_filter.h"
#include "rules.h"
#include <cctype>

namespace {

// Returns true for bytes that belong to a "word" character:
// ASCII alphanumeric/underscore, or any byte of a multi-byte UTF-8 sequence
// (covers Cyrillic and all other non-ASCII scripts).
bool isWordByte(unsigned char c)
{
    return c > 127 || std::isalnum(c) || c == '_';
}

bool matchesWordBoundary(const std::string& text, const std::string& word)
{
    if (word.empty()) return false;
    size_t pos = 0;
    while ((pos = text.find(word, pos)) != std::string::npos) {
        bool leftOk  = (pos == 0)
                    || !isWordByte(static_cast<unsigned char>(text[pos - 1]));
        bool rightOk = (pos + word.size() >= text.size())
                    || !isWordByte(static_cast<unsigned char>(text[pos + word.size()]));
        if (leftOk && rightOk) return true;
        ++pos;
    }
    return false;
}

} // namespace

void core::StopWordsFilter::setWords(std::vector<std::string> words)
{
    m_words = std::move(words);
    m_normalized.clear();
    m_normalized.reserve(m_words.size());
    for (const auto& w : m_words) {
        auto start = w.find_first_not_of(" \t");
        if (start == std::string::npos) { m_normalized.emplace_back(); continue; }
        auto end = w.find_last_not_of(" \t");
        m_normalized.push_back(core::Rules::norm(w.substr(start, end - start + 1)));
    }
}

bool core::StopWordsFilter::matches(const std::string& text) const
{
    if (m_normalized.empty()) return false;
    const std::string norm = core::Rules::norm(text);
    for (const auto& w : m_normalized) {
        if (!w.empty() && matchesWordBoundary(norm, w)) return true;
    }
    return false;
}
