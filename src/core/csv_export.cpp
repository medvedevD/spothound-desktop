#include "csv_export.h"
#include <sstream>

std::string core::toCsv(const std::vector<core::PlaceRow>& rows)
{
    std::ostringstream out;
    out << "Источник;Запрос;Название;Адрес;Телефон;Сайт;Баллы;Причины;Описание\n";
    for (const auto& r : rows) {
        out << r.source  << ";"
            << r.query   << ";"
            << r.name    << ";"
            << r.address << ";"
            << r.phone   << ";"
            << r.site    << ";"
            << r.score   << ";"
            << r.why     << ";"
            << r.descr   << "\n";
    }
    return out.str();
}
