#include "yandexscraper.h"
#include "yandexcollector.h"
#include "yandexparser.h"

#include <QDateTime>
#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEnginePage>

YandexScraper::YandexScraper(QString query, QString city,
                             QWebEngineProfile* profile,
                             StopWordsStore* stopWordsStore,
                             QStringList scoreKeywords,
                             int gridN,
                             QObject* p)
    : ScrapeTask(std::move(query), std::move(city), profile, stopWordsStore,
                 std::move(scoreKeywords), p)
    , m_gridN(qBound(2, gridN, 10))
{
    setupProfile(m_profile);
}

void YandexScraper::setupProfile(QWebEngineProfile* profile)
{
    if (!profile) return;
    auto* store = profile->cookieStore();

    QNetworkCookie gid("yandex_gid", "213");
    gid.setDomain(".yandex.ru"); gid.setPath("/");
    gid.setExpirationDate(QDateTime::currentDateTimeUtc().addYears(1));
    store->setCookie(gid);

    QNetworkCookie lang("yandex_language", "ru");
    lang.setDomain(".yandex.ru"); lang.setPath("/");
    lang.setExpirationDate(QDateTime::currentDateTimeUtc().addYears(1));
    store->setCookie(lang);
}

void YandexScraper::start()
{
    if (m_collector) { m_collector->abort(); m_collector->deleteLater(); m_collector = nullptr; }
    if (m_parser)    { m_parser->abort();    m_parser->deleteLater();    m_parser    = nullptr; }

    m_stats = {};
    m_stats.source = "Яндекс.Карты";
    m_stats.query  = (m_query + " " + m_city).toStdString();
    m_stats.city   = m_city.toStdString();
    m_stats.gridN  = m_gridN;
    m_collectionTimer.start();

    emit phaseChanged("Collection of stores by zone");

    m_collector = new YandexCollector(m_query, m_city, m_profile, m_gridN, m_stats, this);
    connect(m_collector, &YandexCollector::hrefsReady,   this, &YandexScraper::onHrefsReady);
    connect(m_collector, &YandexCollector::gridProgress, this, &YandexScraper::gridProgress);
    connect(m_collector, &YandexCollector::preview,      this, &YandexScraper::preview);
    connect(m_collector, &YandexCollector::error,        this, &YandexScraper::finished);
    m_collector->start();
}

void YandexScraper::onHrefsReady(QStringList hrefs)
{
    m_stats.collectionMs  = m_collectionTimer.elapsed();
    m_stats.queuedCards   = hrefs.size();

    emit queueSized(hrefs.size());
    emit phaseChanged("parsing");
    emit parseProgress(hrefs.size(), 0);

    m_parsingTimer.start();

    m_parser = new YandexParser(m_query, m_city, m_profile,
                                m_scoreKeywords, m_stopWordsStore, m_stats, this);
    connect(m_parser, &YandexParser::result,        this, &YandexScraper::result);
    connect(m_parser, &YandexParser::parseProgress, this, &YandexScraper::parseProgress);
    connect(m_parser, &YandexParser::preview,       this, &YandexScraper::preview);
    connect(m_parser, &YandexParser::error,         this, &YandexScraper::finished);
    connect(m_parser, &YandexParser::done, this, [this]{
        m_stats.parsingMs = m_parsingTimer.elapsed();
        emit phaseChanged("idle");
        emitStats();
        emit finishedAll();
        emit finished();
    });
    m_parser->start(std::move(hrefs));
}

void YandexScraper::reset()
{
    if (m_collector) { m_collector->abort(); m_collector->deleteLater(); m_collector = nullptr; }
    if (m_parser)    { m_parser->abort();    m_parser->deleteLater();    m_parser    = nullptr; }

    emit phaseChanged(QStringLiteral("Click the 'start' button to start parsing"));
    emit gridProgress(0, 0);
    emit queueSized(0);
    emit parseProgress(0, 0);
}
