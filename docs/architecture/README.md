# Spothound Architecture

C4-документация архитектуры Spothound. Рендерится на GitHub через Mermaid.

## Текущее состояние

- [c4-context.md](c4-context.md) — **Level 1: System Context.** Spothound, пользователь, внешние системы карт, ФС.
- [c4-containers.md](c4-containers.md) — **Level 2: Containers.** Десктоп-монолит, почему один контейнер.
- [c4-components.md](c4-components.md) — **Level 3: Components.** Трёхслойная структура: Core → Adapters → GUI. Основная техническая диаграмма.
- [c4-dynamic-scrape.md](c4-dynamic-scrape.md) — **Dynamic.** Последовательность вызовов при запуске скрапинга.

## Будущее состояние

- [c4-future.md](c4-future.md) — **Desktop + Server.** Как архитектура эволюционирует с появлением серверной версии и веб-UI. Desktop сохраняется.

## Порядок чтения

1. Новичку в проекте: `c4-context.md` → `c4-containers.md` → `c4-components.md`.
2. Разработчику перед правкой скрапера: `c4-components.md` + `c4-dynamic-scrape.md`.
3. Для стратегических решений о сервере: `c4-future.md`.

## Как обновлять

Диаграммы — источник истины по архитектуре. При изменениях в слоях/контрактах обновлять соответствующий `.md` в этой папке. Проверка на актуальность — при ревью PR, затрагивающих `src/core/` или границы между слоями.
