#ifndef SCRAPETASK_H
#define SCRAPETASK_H

#include "placerow.h"
#include "core/i_scraper.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

class QWebEnginePage;
class QWebEngineProfile;
class StopWordsStore;

struct ScrapeStats {
    QString source;
    QString query;
    QString city;
    int gridN = 0;
    int gridCells = 0;
    qint64 collectionMs = 0;
    qint64 parsingMs = 0;
    qint64 totalCardProcessMs = 0;
    int queuedCards = 0;
    int cardCount = 0;
    int failedCards = 0;
    int blockedCards = 0;
    int probeRetries = 0;
    qint64 minCardMs = -1;
    qint64 maxCardMs = 0;
    qint64 avgCardMs() const { return cardCount > 0 ? totalCardProcessMs / cardCount : 0; }
};

// QObject must be listed first in multiple inheritance with Qt.
class ScrapeTask : public QObject, public core::IScraper {
    Q_OBJECT
public:
    explicit ScrapeTask(QString query, QString city,
                        QWebEngineProfile* profile,
                        StopWordsStore* stopWordsStore,
                        QStringList scoreKeywords = {},
                        QObject* p = nullptr);
    virtual ~ScrapeTask() = default;

    void start() override = 0;
    virtual void reset() = 0;
    void stop() override {}

    // setEventCallback() and m_eventCallback inherited from core::IScraper

signals:
    void result(PlaceRow row);
    void finished();
    void captchaRequested(QWebEnginePage* page);
    void preview(QWebEnginePage* page, const QString& title);
    void phaseChanged(const QString& phase);
    void gridProgress(int total, int done);
    void queueSized(int total);
    void parseProgress(int total, int done);
    void statsReady(ScrapeStats stats);
    void finishedAll();

protected:
    bool isBlocked(const PlaceRow& r) const;
    void emitStats();

    QString m_query;
    QString m_city;
    QWebEngineProfile* m_profile = nullptr;
    StopWordsStore* m_stopWordsStore = nullptr;
    QStringList m_scoreKeywords;

    ScrapeStats m_stats;
    QElapsedTimer m_collectionTimer;
    QElapsedTimer m_parsingTimer;
    QElapsedTimer m_cardTimer;
    QElapsedTimer m_scrollTimer;
    bool m_parsingStarted = false;
};

#endif // SCRAPETASK_H
