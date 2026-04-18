#include "scrapetask.h"
#include "stopwordsstore.h"

ScrapeTask::ScrapeTask(QString query, QString city,
                       QWebEngineProfile* profile,
                       StopWordsStore* stopWordsStore,
                       QStringList scoreKeywords,
                       QObject* p)
    : QObject(p)
    , m_query(std::move(query))
    , m_city(std::move(city))
    , m_profile(profile)
    , m_stopWordsStore(stopWordsStore)
    , m_scoreKeywords(std::move(scoreKeywords))
{}

bool ScrapeTask::isBlocked(const PlaceRow& r) const
{
    return m_stopWordsStore && m_stopWordsStore->matchesRow(r);
}
