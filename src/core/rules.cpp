#include "rules.h"
#include <cctype>

namespace {

// Lowercase Cyrillic UTF-8 bytes (А-Я → а-я, Ё → ё) plus ASCII.
// Operates byte-by-byte on UTF-8 encoded text.
std::string cyrillicToLower(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        auto b0 = static_cast<unsigned char>(s[i]);
        if (b0 == 0xD0 && i + 1 < s.size()) {
            auto b1 = static_cast<unsigned char>(s[i + 1]);
            if (b1 == 0x81) {
                // Ё (U+0401) → ё (U+0451)
                out += '\xD1'; out += '\x91'; i += 2; continue;
            } else if (b1 >= 0x90 && b1 <= 0x9F) {
                // А-П (U+0410-U+041F) → а-п (U+0430-U+043F)
                out += '\xD0'; out += static_cast<char>(b1 + 0x20); i += 2; continue;
            } else if (b1 >= 0xA0 && b1 <= 0xAF) {
                // Р-Я (U+0420-U+042F) → р-я (U+0440-U+044F)
                out += '\xD1'; out += static_cast<char>(b1 - 0x20); i += 2; continue;
            }
        }
        out += static_cast<char>(std::tolower(b0));
        ++i;
    }
    return out;
}

// Replace ё (U+0451, UTF-8: 0xD1 0x91) → е (U+0435, UTF-8: 0xD0 0xB5)
std::string replaceYo(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        auto b0 = static_cast<unsigned char>(s[i]);
        if (b0 == 0xD1 && i + 1 < s.size()
                && static_cast<unsigned char>(s[i + 1]) == 0x91) {
            out += '\xD0'; out += '\xB5'; i += 2; continue;
        }
        out += s[i]; ++i;
    }
    return out;
}

std::string trimStr(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string collapseSpaces(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    bool prev = false;
    for (unsigned char c : s) {
        bool sp = (c == ' ' || c == '\t' || c == '\r' || c == '\n');
        if (sp) {
            if (!prev) { out += ' '; prev = true; }
        } else {
            out += static_cast<char>(c); prev = false;
        }
    }
    return out;
}

} // namespace

std::string core::Rules::norm(std::string s)
{
    s = cyrillicToLower(s);
    s = trimStr(s);
    s = replaceYo(s);
    s = collapseSpaces(s);
    return s;
}

std::pair<int, std::string> core::Rules::score(
    const std::string& name, const std::string& descr,
    bool hasSite, const std::vector<std::string>& keywords)
{
    const std::string text = core::Rules::norm(name + " " + descr);
    int s = 0;
    std::string why;
    for (const auto& kw : keywords) {
        auto start = kw.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        auto end = kw.find_last_not_of(" \t");
        const std::string k = kw.substr(start, end - start + 1);
        if (!k.empty() && text.find(k) != std::string::npos) {
            ++s;
            if (!why.empty()) why += ", ";
            why += "kw:" + k;
        }
    }
    if (hasSite) {
        ++s;
        if (!why.empty()) why += ", ";
        why += "site";
    }
    return {s, why};
}
