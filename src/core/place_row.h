#pragma once
#include <string>

namespace core {

struct PlaceRow {
    std::string source;
    std::string query;
    std::string name;
    std::string address;
    std::string phone;
    std::string site;
    std::string descr;
    int score = 0;
    std::string why;
};

} // namespace core
