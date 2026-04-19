#include <gtest/gtest.h>
#include "core/csv_export.h"

TEST(CsvExport, HeaderOnly) {
    const std::string csv = core::toCsv({});
    EXPECT_EQ(csv, "Источник;Запрос;Название;Адрес;Телефон;Сайт;Баллы;Причины;Описание\n");
}

TEST(CsvExport, OneRow) {
    core::PlaceRow r;
    r.source  = "yandex";
    r.query   = "кафе";
    r.name    = "Тест";
    r.address = "ул. Тестовая, 1";
    r.phone   = "+7-000";
    r.site    = "test.ru";
    r.descr   = "описание";
    r.score   = 2;
    r.why     = "kw:кафе";

    const std::string csv = core::toCsv({r});
    const std::string expected =
        "Источник;Запрос;Название;Адрес;Телефон;Сайт;Баллы;Причины;Описание\n"
        "yandex;кафе;Тест;ул. Тестовая, 1;+7-000;test.ru;2;kw:кафе;описание\n";
    EXPECT_EQ(csv, expected);
}

TEST(CsvExport, MultipleRows) {
    core::PlaceRow r1, r2;
    r1.name = "Первый";
    r2.name = "Второй";

    const std::string csv = core::toCsv({r1, r2});
    EXPECT_NE(csv.find("Первый"), std::string::npos);
    EXPECT_NE(csv.find("Второй"), std::string::npos);
}
