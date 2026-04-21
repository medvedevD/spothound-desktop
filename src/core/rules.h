#pragma once
#include <string>
#include <utility>
#include <vector>

namespace core {
namespace Rules {
    std::string norm(std::string s);
    std::pair<int, std::string> score(const std::string& name, const std::string& descr,
                                      bool hasSite, const std::vector<std::string>& keywords);
}
}
