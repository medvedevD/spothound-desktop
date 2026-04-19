#pragma once
#include "place_row.h"
#include <string>
#include <vector>

namespace core {
    std::string toCsv(const std::vector<PlaceRow>& rows);
}
