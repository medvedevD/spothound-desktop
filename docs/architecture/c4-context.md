# System Context — Spothound

Диаграмма системного контекста для десктопного приложения Spothound.

## Диаграмма

```mermaid
C4Context
  title System Context — Spothound Desktop

  Person(user, "Пользователь", "Исследователь / маркетолог, собирает списки организаций по городу и запросу")

  System(spothound, "Spothound Desktop", "Qt-приложение для скрапинга карт и экспорта результатов")

  System_Ext(yandex, "Яндекс.Карты", "Публичный сайт maps.yandex.ru")
  System_Ext(twogis, "2ГИС", "Публичный сайт 2gis.ru")
  System_Ext(googlemaps, "Google Maps", "Публичный сайт google.com/maps")

  System_Ext(fs, "Локальная ФС", "CSV / XLSX экспорт, стоп-слова, профиль браузера")

  Rel(user, spothound, "Задаёт запрос, город, ключевые и стоп-слова; решает CAPTCHA")
  Rel(spothound, yandex, "Парсит карточки организаций", "HTTPS через QtWebEngine")
  Rel(spothound, twogis, "Парсит карточки организаций", "HTTPS через QtWebEngine")
  Rel(spothound, googlemaps, "Парсит карточки организаций", "HTTPS через QtWebEngine")
  Rel(spothound, fs, "Пишет / читает", "CSV, XLSX, JSON")
```

## Пояснение

- **Пользователь** — один человек, запускает приложение локально.
- **Spothound Desktop** — единый Qt-бинарник с встроенным Chromium (QtWebEngine).
- **Внешние системы карт** — не имеют публичного API; доступ только через веб-страницы, отсюда браузерный скрапинг.
- **Локальная ФС** — вся персистентность (результаты, стоп-слова, cookies браузера). Нет серверной части, нет облака.

Контекст отражает текущее состояние: single-user desktop tool.
