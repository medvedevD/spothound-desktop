#ifndef CITYREGISTRY_H
#define CITYREGISTRY_H

#include "core/city_registry.h"
#include <QString>

using CityGeo = core::CityGeo;

class CityRegistry {
public:
    static const CityGeo* find(const QString& cityName);
};

#endif // CITYREGISTRY_H
