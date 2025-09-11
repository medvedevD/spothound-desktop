# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Qt 5.15 or Qt 6 with `WebEngineWidgets` is required. QXlsx is a git submodule — run `git submodule update --init` if it's missing. On Linux also requires `qtbase5-private-dev` (for QXlsx zip support); on Windows the Qt installer includes these headers automatically.

CI builds Windows executables via GitHub Actions (`.github/workflows/qt-ci.yml`).

## No Tests

There is no automated test infrastructure. All verification is manual through the GUI.

## Architecture

**What it is:** A Qt desktop app called **Spothound** that scrapes map services (Yandex Maps, 2GIS, Google Maps) for business listings by search query and city, then exports results to CSV/XLSX. CMake project name and binary are `spothound`.

### Class hierarchy

```
ScrapeTask (QObject, abstract)
  ├─ YandexScraper   — Yandex Maps, geographic grid + card parsing
  ├─ TwoGisScraper   — 2GIS, sidebar scroll + card parsing
  └─ GoogleMapsScraper — Google Maps, sidebar scroll + card parsing
```

`MainWindow` works exclusively through `ScrapeTask*` — it does not know which source is active. A factory in `onStart()` instantiates the right adapter based on `sourceCombo`.

### Key components

- `scrapetask.h/.cpp` — Abstract base. Holds `m_query`, `m_city`, `m_profile`, `m_stopWordsStore`. Declares all progress signals (`phaseChanged`, `gridProgress`, `queueSized`, `parseProgress`, `finishedAll`, `result`, `finished`, `preview`, `captchaRequested`). Provides `isBlocked()` helper.
- `yandexscraper.cpp` — Core Yandex adapter. Two phases: (1) 5×5 geographic grid → scroll sidebar per cell, collect `/maps/org/` hrefs; (2) open each card URL, extract data with JS. Uses `CityRegistry` for city bbox/slug. `setupProfile()` injects Yandex cookies.
- `twogisscraper.cpp` — 2GIS adapter. No grid. Scrolls `[class*="searchList"]` sidebar, collects `a[href*="/firm/"]` links, then parses each firm page.
- `googlemapsscraper.cpp` — Google Maps adapter. No grid. Scrolls `[role="feed"]` sidebar, collects `a[href*="/maps/place/"]` links, then parses each place page.
- `cityregistry.h/.cpp` — Static table of 15 Russian cities with Yandex slug/rid, 2GIS slug, bbox, and zoom level. `CityRegistry::find()` does case-insensitive partial match.
- `captchaawarepage.h` — `QWebEnginePage` subclass. CAPTCHA detection is injected as `std::function<bool(const QUrl&)>` — each adapter passes its own predicate.
- `mainwindow.cpp` — Main GUI. Factory in `onStart()`, connects all signals via `ScrapeTask*`. `btnClear` calls `m_currentScraper->reset()`. Reads `scoreKeywords` field (comma-separated) and passes to scraper constructor.
- `placesmodel.cpp` — `QAbstractTableModel` holding in-memory results. Handles CSV and XLSX export.
- `rules.cpp` — Scoring and deduplication. Normalizes Cyrillic text (ё→е), computes relevance scores from configurable `QStringList keywords` (empty by default — score=0 for all results).
- `stopwordsstore.cpp` — Persists a stop-words list to app-data dir; filters results via regex. Default list is empty — user fills it manually.

### Data flow

```
User sets query + city + source
  → MainWindow::onStart() creates ScrapeTask* via factory
  → scraper emits gridProgress / queueSized / parseProgress
  → scraper emits result(PlaceRow) → PlacesModel::addRow()
  → scraper emits finishedAll() / finished()
  → User exports to CSV / XLSX
```

### Async model

Single-threaded Qt event loop. All web operations are async via `QWebEngine` callbacks and signals/slots — no `QThread`. Each adapter uses a `QTimer::singleShot` chain to drive its scraping loop.

### Note on JS selectors for 2GIS / Google Maps

Selectors in `twogisscraper.cpp` and `googlemapsscraper.cpp` (e.g. `[class*="searchList"]`, `[role="feed"]`, `[data-item-id*="address"]`) may need adjustment after testing — these sites change their DOM structure periodically.

### 3rdparty

`QXlsx` is included as a CMake subdirectory under `3rdparty/QXlsx/`.

### App icon

`main-icon.png` in the project root is the app icon, embedded via `resources.qrc`. To update the icon — replace `main-icon.png` and rebuild. For Windows CI, `spothound.rc` + `spothound.ico` embed the icon into the `.exe`.
