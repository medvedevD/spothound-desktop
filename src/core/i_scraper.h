#pragma once
#include "scraper_events.h"
#include <functional>

namespace core {

class IScraper {
public:
    virtual ~IScraper() = default;
    virtual void start() = 0;
    virtual void stop() {}

    void setEventCallback(std::function<void(const ScraperEvent&)> cb) {
        m_eventCallback = std::move(cb);
    }

protected:
    std::function<void(const ScraperEvent&)> m_eventCallback;
};

} // namespace core
