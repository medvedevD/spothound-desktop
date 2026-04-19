#include "cityregistry.h"

const CityGeo* CityRegistry::find(const QString& cityName)
{
    return core::CityRegistry::find(cityName.toStdString());
}
