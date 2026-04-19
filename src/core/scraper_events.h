#pragma once
#include "place_row.h"
#include <string>
#include <variant>

namespace core {

struct PhaseChangedEvent    { std::string phase; };
struct GridProgressEvent    { int total = 0; int done = 0; };
struct QueueSizedEvent      { int total = 0; };
struct ParseProgressEvent   { int total = 0; int done = 0; };
struct ResultEvent          { PlaceRow row; };
struct FinishedAllEvent     {};
struct CaptchaRequestedEvent {};
struct PreviewEvent         { std::string title; };

using ScraperEvent = std::variant<
    PhaseChangedEvent,
    GridProgressEvent,
    QueueSizedEvent,
    ParseProgressEvent,
    ResultEvent,
    FinishedAllEvent,
    CaptchaRequestedEvent,
    PreviewEvent
>;

} // namespace core
