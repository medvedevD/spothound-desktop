# Future State — Desktop + Server

Forward-looking диаграмма. Показывает, как архитектура эволюционирует, когда появится серверная версия и веб-UI. **Десктоп-приложение сохраняется** — сервер не заменяет его, а дополняет.

Документ описывает целевое состояние, а не план на ближайший квартал.

## Ключевая идея

`spothound-core` — общий доменный словарь между двумя мирами:

- **Desktop** продолжает жить на Qt + QtWebEngine. Локальный инструмент для одиночного пользователя, решает CAPTCHA руками.
- **Server** использует headless-браузер (Playwright) + прокси + CAPTCHA-солверы. Масштабируется горизонтально.

Общее между ними: формат `PlaceRow`, протокол событий `ScraperEvent`, алгоритмы scoring и фильтрации. Реализации скрапера — разные.

## Диаграмма контейнеров (будущее состояние)

```mermaid
C4Container
  title Future Container Diagram — Spothound (Desktop + Server)

  Person(localUser, "Локальный пользователь", "Использует десктоп-версию")
  Person(webUser, "Веб-пользователь", "Работает через браузер с SaaS-версией")

  System_Boundary(desktop, "Desktop (существующий)") {
    Container(app, "Spothound Desktop", "C++17, Qt, QtWebEngine", "Локальный single-user бинарник, переиспользует core")
  }

  System_Boundary(server, "Server (будущее)") {
    Container(webui, "Web UI", "React / Vue + TS", "SPA, показывает прогресс через WebSocket/SSE")
    Container(api, "API Gateway", "Python FastAPI / Node", "REST + WebSocket, эмитит ScraperEvent наружу")
    ContainerQueue(queue, "Task Queue", "Redis / RabbitMQ", "Задачи скрапинга")
    Container(worker, "Scraper Worker", "Python + Playwright", "Headless Chromium, реализует протокол IScraper")
    ContainerDb(db, "Results DB", "PostgreSQL", "Результаты, стоп-слова, пользователи")
    Container(proxy, "Proxy Pool", "Residential / mobile", "Ротация IP для скрапинга")
    Container(captcha, "CAPTCHA Solver", "2Captcha / Anti-Captcha", "Автоматическое решение капчи")
  }

  System_Ext(maps, "Yandex / 2GIS / Google Maps", "Публичные сайты карт")

  Rel(localUser, app, "Использует", "Qt GUI")
  Rel(app, maps, "Скрапит напрямую", "HTTPS через QtWebEngine")

  Rel(webUser, webui, "Использует", "HTTPS")
  Rel(webui, api, "Запросы / прогресс", "REST + WebSocket")
  Rel(api, queue, "Ставит задачи")
  Rel(api, db, "Читает результаты", "SQL")
  Rel(worker, queue, "Забирает задачи")
  Rel(worker, proxy, "Использует IP")
  Rel(worker, captcha, "Решает CAPTCHA")
  Rel(worker, maps, "Скрапит через headless Chromium", "HTTPS")
  Rel(worker, db, "Пишет результаты", "SQL")

  UpdateRelStyle(app, maps, $lineColor="green")
  UpdateRelStyle(worker, maps, $lineColor="blue")
```

## Что общее между Desktop и Server

Не показано на диаграмме отдельно, но по сути:

- **`PlaceRow` + JSON-схема** — общий формат результата. POD из core сериализуется в JSON одинаково в обеих реализациях.
- **`ScraperEvent` протокол** — enum событий `PhaseChanged / GridProgress / Result / CaptchaRequested / ...` одинаковый. В Desktop он эмитится как Qt-сигнал, на сервере — как WebSocket-сообщение.
- **`Rules` и `StopWordsFilter`** — одни и те же алгоритмы. На сервере — порт на Python/Node (алгоритм маленький, переписывается за день), либо переиспользование `spothound-core` через pybind11.

## Почему именно так

- **Desktop остаётся** потому что он решает задачу, которую сервер решает плохо и дорого: реальный пользовательский профиль с живыми cookies, ручное решение CAPTCHA, низкие объёмы без прокси. Для одного пользователя с небольшими запросами десктоп дешевле и надёжнее.
- **Сервер** нужен для масштабирования на многих пользователей, регулярных запусков, веб-интерфейса, API. QtWebEngine на сервере — дорого и плохо параллелится; Playwright решает эти задачи на порядок лучше ([см. обсуждение в CLAUDE.md / conversation logs]).
- **Общий core** обеспечивает, что результаты и логика скрапинга идентичны в обеих версиях. Пользователь не видит разницы в формате данных между desktop-CSV и веб-экспортом.

## Переход

Рефакторинг, выполненный в [c4-components.md](c4-components.md), подготавливает миграцию:

1. Core уже отделён от Qt → портируется либо через pybind11, либо переписывается на языке сервера.
2. `IScraper` / `ScraperEvent` — готовый контракт для второй реализации (Playwright).
3. `PlaceRow` с JSON — готовый wire-формат API.

Задачи, которые **ещё не сделаны** и нужны для миграции:

- Веб-UI с нуля.
- Сервер: API, очередь, воркеры, БД, деплой.
- Пул прокси и интеграция с CAPTCHA-солвером.
- Бизнес-модель (SaaS, аутентификация, биллинг) — вне технической архитектуры.
