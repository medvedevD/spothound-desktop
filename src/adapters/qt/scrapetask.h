#ifndef SCRAPETASK_H
#define SCRAPETASK_H

#include "core/place_row.h"
#include "core/scrape_stats.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

#include <string>
#include <vector>

class QWebEnginePage;
class QWebEngineProfile;
class StopWordsStore;

class ScrapeTask : public QObject {
    Q_OBJECT
public:
    explicit ScrapeTask(QString query, QString city,
                        QWebEngineProfile* profile,
                        StopWordsStore* stopWordsStore,
                        QStringList scoreKeywords = {},
                        QObject* p = nullptr);
    virtual ~ScrapeTask() = default;

    virtual void start() = 0;
    virtual void reset() = 0;

signals:
    void result(core::PlaceRow row);
    void finished();
    void captchaRequested(QWebEnginePage* page);
    void preview(QWebEnginePage* page, const QString& title);
    void phaseChanged(const QString& phase);
    void gridProgress(int total, int done);
    void queueSized(int total);
    void parseProgress(int total, int done);
    void statsReady(core::ScrapeStats stats);
    void finishedAll();

protected:
    bool isBlocked(const core::PlaceRow& r) const;
    void emitStats();

    QString m_query;
    QString m_city;
    QWebEngineProfile* m_profile = nullptr;
    StopWordsStore* m_stopWordsStore = nullptr;
    std::vector<std::string> m_scoreKeywords;

    core::ScrapeStats m_stats;
    QElapsedTimer m_collectionTimer;
    QElapsedTimer m_parsingTimer;
    QElapsedTimer m_cardTimer;
    QElapsedTimer m_scrollTimer;
    bool m_parsingStarted = false;
};

#endif // SCRAPETASK_H
