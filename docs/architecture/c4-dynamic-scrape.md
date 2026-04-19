# Dynamic Diagram — Scraping Flow

Последовательность вызовов при запуске скрапинга пользователем (на примере YandexScraper).

## Диаграмма

```mermaid
C4Dynamic
  title Dynamic Diagram — Scrape Flow (Yandex, одна сессия)

  Person(user, "Пользователь")

  Container_Boundary(app, "Spothound Desktop") {
    Component(mw, "MainWindow", "QMainWindow")
    Component(factory, "Scraper Factory", "onStart()")
    Component(scraper, "YandexScraper", "ScrapeTask / IScraper")
    Component(page, "CaptchaAwarePage", "QWebEnginePage")
    Component(rules, "core::Rules", "scoring")
    Component(swf, "core::StopWordsFilter", "фильтрация")
    Component(model, "PlacesModel", "QAbstractTableModel")
    Component(csv, "core::CsvExport", "toCsv")
  }

  System_Ext(yandex, "maps.yandex.ru")
  SystemDb(fs, "Локальная ФС")

  Rel(user, mw, "1. Задаёт запрос + город + ключевые слова, жмёт Start")
  Rel(mw, factory, "2. Выбирает источник по sourceCombo")
  Rel(factory, scraper, "3. Создаёт YandexScraper(query, city, keywords)")
  Rel(scraper, page, "4. Открывает страницу поиска")
  Rel(page, yandex, "5. Загружает карту и sidebar", "HTTPS")

  Rel(scraper, yandex, "6. Фаза Grid: N×N ячеек, скролл sidebar, сбор /maps/org/ hrefs", "JS eval")
  Rel(scraper, mw, "7. Эмитит gridProgress(total, done)")
  Rel(scraper, mw, "8. Эмитит queueSized(total) по окончании фазы Grid")

  Rel(scraper, yandex, "9. Фаза Parse: открывает каждую карточку, извлекает поля", "JS eval")
  Rel(scraper, rules, "10. score(name, descr, keywords)")
  Rel(scraper, swf, "11. matches(text) — отбрасывает если match")
  Rel(scraper, mw, "12. Эмитит result(PlaceRow)")
  Rel(mw, model, "13. addRow(row)")
  Rel(scraper, mw, "14. Эмитит parseProgress(total, done)")

  Rel(scraper, mw, "15. Эмитит finishedAll() при завершении")
  Rel(user, mw, "16. Нажимает Export CSV")
  Rel(mw, model, "17. exportCsv(path)")
  Rel(model, csv, "18. toCsv(rows)")
  Rel(model, fs, "19. Пишет файл", "UTF-8")
```

## Комментарии к шагам

- **Шаги 6–8 (Grid-фаза)** специфичны для YandexScraper. TwoGisScraper и GoogleMapsScraper пропускают Grid-фазу: у них один sidebar, который прокручивается до конца. Переходят сразу к фазе Parse.
- **Шаг 11**: фильтрация стоп-слов применяется **перед** эмитом `result`. Отфильтрованные записи не попадают в модель и не видны пользователю.
- **CAPTCHA-branch (не показан)**: если `CaptchaAwarePage` детектирует капчу, скрапер эмитит `captchaRequested(page)`. MainWindow автоматически показывает браузерную панель с этой страницей, пользователь решает капчу руками, скрапинг продолжается.
- **Шаги 12–14**: `result` и `parseProgress` эмитятся на каждую успешно распарсенную карточку; модель обновляется инкрементально, пользователь видит результаты по мере появления.

## Что изменится на сервере

На серверной версии (см. [c4-future.md](c4-future.md)) шаги 6–14 выполняются в Playwright-воркере. Вместо Qt-сигналов скрапер вызывает `m_eventCallback(ScraperEvent)` из `core::IScraper`; API-gateway пересылает эти события клиенту через WebSocket. Протокол событий — тот же.
