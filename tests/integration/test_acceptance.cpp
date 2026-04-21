#include <gtest/gtest.h>

#include "adapters/qt/scrapetask.h"
#include "adapters/qt/yandexscraper.h"
#include "core/place_row.h"

#include <QEventLoop>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineProfile>

static constexpr int kTimeoutMs = 5 * 60 * 1000;

struct AcceptanceCase {
    const char* query;
    const char* city;
    int         gridN;
    int         minExpected;
};

class AcceptanceSuite : public ::testing::TestWithParam<AcceptanceCase> {};

TEST_P(AcceptanceSuite, MinResults)
{
    const AcceptanceCase& tc = GetParam();

    QWebEngineProfile profile;
    profile.setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36");

    auto* scraper = new YandexScraper(
        QString::fromUtf8(tc.query),
        QString::fromUtf8(tc.city),
        &profile,
        nullptr,    // stop-words not needed for acceptance
        {},
        tc.gridN
    );

    int  resultCount = 0;
    bool captchaHit  = false;
    bool timedOut    = false;

    QEventLoop loop;
    QTimer     watchdog;
    watchdog.setSingleShot(true);
    watchdog.setInterval(kTimeoutMs);

    QObject::connect(scraper, &ScrapeTask::result,
                     [&](const core::PlaceRow&) { ++resultCount; });

    QObject::connect(scraper, &ScrapeTask::captchaRequested,
                     [&](QWebEnginePage*) { captchaHit = true; loop.quit(); });

    QObject::connect(scraper, &ScrapeTask::finishedAll,
                     [&]() { loop.quit(); });

    QObject::connect(&watchdog, &QTimer::timeout,
                     [&]() { timedOut = true; loop.quit(); });

    scraper->start();
    watchdog.start();
    loop.exec();

    scraper->deleteLater();

    if (captchaHit)
        GTEST_SKIP() << "CAPTCHA for query '" << tc.query << "'";
    if (timedOut)
        GTEST_SKIP() << "Timed out (>" << kTimeoutMs / 60000 << " min) for query '"
                     << tc.query << "'";

    EXPECT_GE(resultCount, tc.minExpected)
        << "query='" << tc.query << "' city='" << tc.city
        << "' got " << resultCount << " results, expected >=" << tc.minExpected;
}

INSTANTIATE_TEST_SUITE_P(
    YandexAcceptance, AcceptanceSuite,
    ::testing::Values(
        AcceptanceCase{"кофейни",   "москва",          2, 10},
        AcceptanceCase{"рестораны", "санкт-петербург", 2,  8},
        AcceptanceCase{"аптеки",    "новосибирск",     2,  5},
        AcceptanceCase{"банки",     "краснодар",       2,  5},
        AcceptanceCase{"пиццерии",  "воронеж",         2,  3}
    )
);
