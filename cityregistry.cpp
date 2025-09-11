#include "cityregistry.h"
#include <QStringList>

static const CityGeo kCities[] = {
    {
        "москва,москв,moscow,мск",
        "moscow", 213, "moscow",
        37.6173, 55.7558,
        37.30, 37.90, 55.55, 55.95, 14
    },
    {
        "санкт-петербург,питер,спб,петербург,saint-petersburg,spb",
        "saint-petersburg", 2, "spb",
        30.3141, 59.9386,
        30.10, 30.55, 59.83, 60.07, 13
    },
    {
        "новосибирск,novosibirsk,нск",
        "novosibirsk", 65, "novosibirsk",
        82.9357, 54.9885,
        82.82, 83.10, 54.82, 55.10, 13
    },
    {
        "екатеринбург,yekaterinburg,екб,ekb",
        "yekaterinburg", 54, "ekaterinburg",
        60.6122, 56.8389,
        60.50, 60.82, 56.72, 56.95, 13
    },
    {
        "казань,kazan",
        "kazan", 43, "kazan",
        49.1233, 55.7887,
        48.93, 49.28, 55.72, 55.88, 13
    },
    {
        "краснодар,krasnodar,кдр",
        "krasnodar", 35, "krasnodar",
        38.9765, 45.0448,
        38.88, 39.07, 44.97, 45.12, 13
    },
    {
        "нижний новгород,нижний,nizhny novgorod,нн",
        "nizhny-novgorod", 47, "nnov",
        44.0020, 56.3287,
        43.80, 44.10, 56.20, 56.42, 13
    },
    {
        "самара,samara",
        "samara", 51, "samara",
        50.1683, 53.1959,
        50.05, 50.32, 53.13, 53.27, 13
    },
    {
        "уфа,ufa",
        "ufa", 172, "ufa",
        55.9721, 54.7388,
        55.87, 56.08, 54.67, 54.82, 13
    },
    {
        "ростов-на-дону,ростов,rostov",
        "rostov-na-donu", 39, "rostov_na_donu",
        39.7015, 47.2357,
        39.60, 39.83, 47.15, 47.30, 13
    },
    {
        "челябинск,chelyabinsk",
        "chelyabinsk", 56, "chelyabinsk",
        61.4291, 55.1644,
        61.33, 61.53, 55.10, 55.24, 13
    },
    {
        "омск,omsk",
        "omsk", 66, "omsk",
        73.3686, 54.9885,
        73.25, 73.53, 54.91, 55.09, 13
    },
    {
        "красноярск,krasnoyarsk",
        "krasnoyarsk", 62, "krasnoyarsk",
        92.8932, 56.0153,
        92.76, 93.03, 55.96, 56.08, 13
    },
    {
        "пермь,perm",
        "perm", 50, "perm",
        56.2502, 58.0105,
        56.14, 56.37, 57.95, 58.07, 13
    },
    {
        "воронеж,voronezh",
        "voronezh", 193, "voronezh",
        39.1843, 51.6755,
        39.10, 39.28, 51.61, 51.74, 13
    },
};

static constexpr int kCityCount = static_cast<int>(sizeof(kCities) / sizeof(kCities[0]));

const CityGeo* CityRegistry::find(const QString& cityName)
{
    const QString norm = cityName.trimmed().toLower()
                            .replace(QChar(0x0451), QChar(0x0435)); // ё→е

    for (int i = 0; i < kCityCount; ++i) {
        const QString aliases = QString::fromUtf8(kCities[i].names);
        const QStringList parts = aliases.split(',');
        for (const QString& alias : parts) {
            if (norm.contains(alias) || alias.contains(norm))
                return &kCities[i];
        }
    }
    return nullptr;
}
