#ifndef SCRAPETASK_H
#define SCRAPETASK_H

#include "placerow.h"
#include <QObject>
#include <QStringList>

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
    void result(PlaceRow row);
    void finished();
    void captchaRequested(QWebEnginePage* page);
    void preview(QWebEnginePage* page, const QString& title);
    void phaseChanged(const QString& phase);
    void gridProgress(int total, int done);
    void queueSized(int total);
    void parseProgress(int total, int done);
    void finishedAll();

protected:
    bool isBlocked(const PlaceRow& r) const;

    QString m_query;
    QString m_city;
    QWebEngineProfile* m_profile = nullptr;
    StopWordsStore* m_stopWordsStore = nullptr;
    QStringList m_scoreKeywords;
};

#endif // SCRAPETASK_H
