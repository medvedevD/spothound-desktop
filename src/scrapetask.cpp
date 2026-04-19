#include "scrapetask.h"
#include "stopwordsstore.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>

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

void ScrapeTask::emitStats()
{
    const qint64 minMs = m_stats.minCardMs < 0 ? 0 : m_stats.minCardMs;

    const qint64 interCardMs = m_stats.cardCount > 0
        ? (m_stats.parsingMs - m_stats.totalCardProcessMs) / m_stats.cardCount : 0;
    qDebug() << qPrintable(QString(
        "[PERF] source=%1 total=%2s collection=%3s parsing=%4s cards=%5 avg=%6ms min=%7ms max=%8ms inter=%9ms probes=%10")
        .arg(m_stats.source)
        .arg((m_stats.collectionMs + m_stats.parsingMs) / 1000.0, 0, 'f', 1)
        .arg(m_stats.collectionMs / 1000.0, 0, 'f', 1)
        .arg(m_stats.parsingMs / 1000.0, 0, 'f', 1)
        .arg(m_stats.cardCount)
        .arg(m_stats.avgCardMs())
        .arg(minMs)
        .arg(m_stats.maxCardMs)
        .arg(interCardMs)
        .arg(m_stats.probeRetries));

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString ts  = QString::number(QDateTime::currentSecsSinceEpoch());
    QFile f(dir + "/spothound_perf_" + ts + ".json");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        const QString json = QString(
            "{\n"
            "  \"source\": \"%1\",\n"
            "  \"query\": \"%2\",\n"
            "  \"city\": \"%3\",\n"
            "  \"total_ms\": %4,\n"
            "  \"collection_ms\": %5,\n"
            "  \"parsing_ms\": %6,\n"
            "  \"grid_n\": %7,\n"
            "  \"grid_cells\": %8,\n"
            "  \"queued_cards\": %9,\n"
            "  \"card_count\": %10,\n"
            "  \"failed_cards\": %11,\n"
            "  \"blocked_cards\": %12,\n"
            "  \"probe_retries\": %13,\n"
            "  \"min_card_ms\": %14,\n"
            "  \"max_card_ms\": %15,\n"
            "  \"avg_card_ms\": %16,\n"
            "  \"inter_card_delay_ms\": %17\n"
            "}\n"
        ).arg(m_stats.source, m_stats.query, m_stats.city)
         .arg(m_stats.collectionMs + m_stats.parsingMs)
         .arg(m_stats.collectionMs).arg(m_stats.parsingMs)
         .arg(m_stats.gridN).arg(m_stats.gridCells)
         .arg(m_stats.queuedCards).arg(m_stats.cardCount)
         .arg(m_stats.failedCards).arg(m_stats.blockedCards)
         .arg(m_stats.probeRetries)
         .arg(minMs).arg(m_stats.maxCardMs).arg(m_stats.avgCardMs())
         .arg(interCardMs);
        f.write(json.toUtf8());
        qDebug() << "[PERF] written to" << f.fileName();
    }

    emit statsReady(m_stats);
}
