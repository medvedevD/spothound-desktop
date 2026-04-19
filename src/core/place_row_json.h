#pragma once
#include "place_row.h"
#include <nlohmann/json.hpp>

namespace core {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlaceRow,
    source, query, name, address, phone, site, descr, score, why)
}
