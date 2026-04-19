#pragma once
#include "placerow.h"
#include "core/place_row.h"

inline core::PlaceRow toCore(const PlaceRow& r)
{
    return { r.source.toStdString(), r.query.toStdString(), r.name.toStdString(),
             r.address.toStdString(), r.phone.toStdString(), r.site.toStdString(),
             r.descr.toStdString(), r.score, r.why.toStdString() };
}
