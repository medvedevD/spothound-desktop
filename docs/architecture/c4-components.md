# Component Diagram — Spothound Desktop App

Внутреннее устройство единственного контейнера Spothound после рефакторинга на три слоя.

## Правила зависимостей

```
GUI → Adapters → Core
```

- **Core** не знает про Qt, браузер, файлы. Чистый C++17.
- **Adapters** реализуют контракты Core; могут зависеть от Qt/QtWebEngine/QFile.
- **GUI** знает про Adapters и Core; содержит только пользовательский интерфейс.

Нарушение направления (например, core ссылается на Qt) должно ломать сборку — `spothound-core` линкуется без Qt ([src/core/CMakeLists.txt](../../src/core/CMakeLists.txt)).

## Диаграмма

```mermaid
C4Component
  title Component Diagram — Spothound Desktop App

  Person(user, "Пользователь")

  Container_Boundary(gui, "GUI Layer (Qt Widgets)") {
    Component(mainwindow, "MainWindow", "QMainWindow", "Стартовая форма, страница результатов, браузерная панель")
    Component(placesmodel, "PlacesModel", "QAbstractTableModel", "Отображение результатов в таблице, делегирует экспорт в core")
    Component(dialogs, "Dialogs", "QDialog", "FilterDialog, StopWordsDialog")
  }

  Container_Boundary(adapters, "Adapter Layer (Qt / QtWebEngine)") {
    Component(scrapetask, "ScrapeTask", "QObject", "Базовый класс: Qt-сигналы прогресса, профиль браузера")
    Component(yandex, "YandexScraper", "QtWebEngine", "Сетка N×N + парсинг карточек /maps/org/")
    Component(twogis, "TwoGisScraper", "QtWebEngine", "Скролл sidebar + парсинг /firm/")
    Component(googlemaps, "GoogleMapsScraper", "QtWebEngine", "Скролл feed + парсинг /maps/place/")
    Component(stopwordsstore, "StopWordsStore", "QFile / QStandardPaths", "Персистентность стоп-слов, обёртка над core::StopWordsFilter")
    Component(captchapage, "CaptchaAwarePage", "QWebEnginePage", "Детектирует CAPTCHA через пользовательский предикат")
    Component(cityregistry, "CityRegistry", "C++", "Статическая таблица городов (bbox, slug, rid)")
  }

  Container_Boundary(core, "Core (spothound-core, чистый C++17)") {
    Component(rules, "Rules", "C++", "Нормализация кириллицы, scoring по ключевым словам")
    Component(stopfilter, "StopWordsFilter", "C++", "Фильтрация по списку стоп-слов")
    Component(placerow, "PlaceRow", "C++ POD", "Доменная структура результата")
    Component(csvexport, "CsvExport", "C++", "Генерация CSV-строки из vector<PlaceRow>")
    Component(iscraper, "IScraper + ScraperEvent", "C++ contract", "Интерфейс скрапера и протокол событий (std::variant)")
    Component(placerowjson, "PlaceRowJson", "nlohmann/json", "JSON round-trip PlaceRow")
  }

  Rel(user, mainwindow, "Взаимодействует")
  Rel(mainwindow, placesmodel, "Читает")
  Rel(mainwindow, dialogs, "Открывает")
  Rel(mainwindow, scrapetask, "Создаёт через фабрику")
  Rel(mainwindow, stopwordsstore, "list / setList")

  Rel(yandex, scrapetask, "Наследует")
  Rel(twogis, scrapetask, "Наследует")
  Rel(googlemaps, scrapetask, "Наследует")
  Rel(scrapetask, iscraper, "Реализует")

  Rel(yandex, captchapage, "Использует")
  Rel(twogis, captchapage, "Использует")
  Rel(googlemaps, captchapage, "Использует")
  Rel(yandex, cityregistry, "Запрашивает bbox")
  Rel(twogis, cityregistry, "Запрашивает slug")

  Rel(scrapetask, rules, "score / norm")
  Rel(scrapetask, stopwordsstore, "matchesRow")
  Rel(stopwordsstore, stopfilter, "Делегирует")

  Rel(placesmodel, placerow, "Хранит")
  Rel(placesmodel, csvexport, "toCsv()")
  Rel(scrapetask, placerow, "Эмитит")
```

## Пояснение слоёв

### Core (`src/core/`)
Статическая библиотека без Qt. Если в любой файл core попадёт `#include <Qt*>` — сборка упадёт. Сюда переносится всё, что должно переиспользоваться на сервере.

- `Rules` ([src/core/rules.h](../../src/core/rules.h)) — нормализация ё→е, scoring по ключевым словам.
- `StopWordsFilter` ([src/core/stop_words_filter.h](../../src/core/stop_words_filter.h)) — чистая фильтрация.
- `PlaceRow` ([src/core/place_row.h](../../src/core/place_row.h)) — POD.
- `CsvExport` ([src/core/csv_export.h](../../src/core/csv_export.h)) — генерация CSV.
- `IScraper` + `ScraperEvent` ([src/core/i_scraper.h](../../src/core/i_scraper.h), [src/core/scraper_events.h](../../src/core/scraper_events.h)) — контракт скрапера и протокол событий.

### Adapter layer (`src/`)
Реализации, которые зависят от Qt или внешних библиотек.

- Скраперы (`YandexScraper`, `TwoGisScraper`, `GoogleMapsScraper`) наследуют `ScrapeTask`, который реализует `core::IScraper`.
- `StopWordsStore` — Qt-обёртка над `core::StopWordsFilter` с персистентностью через `QStandardPaths`.
- XLSX-экспорт (в `PlacesModel`) остаётся здесь — `QXlsx` тянет Qt.

### GUI layer (`src/`)
Окна, диалоги, модели для отображения. `MainWindow` не знает, какой именно скрапер создаётся — работает через фабрику по `sourceCombo`.

## Тестирование

Core покрыт unit-тестами на GoogleTest ([tests/core/](../../tests/core/)), запускаются без Qt и GUI. На момент написания: 24 теста.
