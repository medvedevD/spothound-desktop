#include "scrapetask.h"
#include "stopwordsstore.h"

#include <nlohmann/json.hpp>

#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <QWebEnginePage>

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
    , m_scoreKeywords([&]{
        std::vector<std::string> v;
        v.reserve(static_cast<size_t>(scoreKeywords.size()));
        for (const auto& k : scoreKeywords) v.push_back(k.toStdString());
        return v;
    }())
{
}

bool ScrapeTask::isBlocked(const core::PlaceRow& r) const
{
    return m_stopWordsStore && m_stopWordsStore->matchesRow(r);
}

void ScrapeTask::emitStats()
{
    const qint64 minMs = m_stats.minCardMs < 0 ? 0 : m_stats.minCardMs;

    qDebug() << qPrintable(QString(
        "[PERF] source=%1 total=%2s collection=%3s parsing=%4s cards=%5 avg=%6ms min=%7ms max=%8ms inter=%9ms probes=%10")
        .arg(QString::fromStdString(m_stats.source))
        .arg((m_stats.collectionMs + m_stats.parsingMs) / 1000.0, 0, 'f', 1)
        .arg(m_stats.collectionMs / 1000.0, 0, 'f', 1)
        .arg(m_stats.parsingMs / 1000.0, 0, 'f', 1)
        .arg(m_stats.cardCount)
        .arg(m_stats.avgCardMs())
        .arg(minMs)
        .arg(m_stats.maxCardMs)
        .arg(m_stats.interCardMs())
        .arg(m_stats.probeRetries));

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QFile f(dir + "/spothound_perf_last.json");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        const nlohmann::json j = m_stats;
        const std::string dumped = j.dump(2);
        f.write(dumped.data(), static_cast<qint64>(dumped.size()));
        qDebug() << "[PERF] written to" << f.fileName();
    }

    emit statsReady(m_stats);
}
