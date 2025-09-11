#ifndef GOOGLEMAPSSCRAPER_H
#define GOOGLEMAPSSCRAPER_H

#include "scrapetask.h"
#include <QWebEnginePage>

class CaptchaAwarePage;

class GoogleMapsScraper : public ScrapeTask {
    Q_OBJECT
public:
    explicit GoogleMapsScraper(QString query, QString city,
                               QWebEngineProfile* profile,
                               StopWordsStore* stopWordsStore,
                               QStringList scoreKeywords = {},
                               QObject* parent = nullptr);

    void start() override;
    void reset() override;

private:
    CaptchaAwarePage* m_searchPage = nullptr;
    QStringList m_hrefQueue;

    void openSearch();
    void collectHrefsStep();
    void processQueue();
    void openCard(const QUrl& href);

    QUrl m_searchUrl;
    bool m_aborted = false;

    QSet<QString> m_seen;
    int m_pass = 0;
    const int m_maxPass = 15;
    int m_idlePass = 0;
    int m_delay = 1200;

    int m_totalCards = 0, m_doneCards = 0;
};

#endif // GOOGLEMAPSSCRAPER_H
