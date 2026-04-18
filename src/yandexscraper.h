#ifndef YANDEXSCRAPER_H
#define YANDEXSCRAPER_H

#include "scrapetask.h"
#include <QWebEnginePage>

class CaptchaAwarePage;

class YandexScraper : public ScrapeTask {
    Q_OBJECT
public:
    explicit YandexScraper(QString query, QString city,
                           QWebEngineProfile* profile,
                           StopWordsStore* stopWordsStore,
                           QStringList scoreKeywords = {},
                           int gridN = 5,
                           QObject* parent = nullptr);

    void start() override;
    void reset() override;

    static void setupProfile(QWebEngineProfile* profile);

private:
    CaptchaAwarePage* m_searchPage = nullptr;
    QStringList m_hrefQueue;

    void openSearch();
    void collectHrefs();
    void collectHrefsStep();
    void processQueue();
    void openCard(const QUrl& href);
    void waitListStable(QWebEnginePage* page, std::function<void()> cont);

    QUrl m_searchUrl;

    bool m_aborted = false;
    QSet<QString> m_seen;
    int m_pass = 0;
    const int m_maxPass = 12;
    int m_idlePass = 0;
    bool m_forcedRu = false;
    int m_delay = 1100;

    QSet<QString> m_seenGlobal;
    QVector<QPointF> m_cells;
    int m_cellIdx = 0;
    int m_cellZoom = 14;
    void buildCells();
    void nextCell();

    int m_gridN = 5;
    int m_totalCells = 0, m_doneCells = 0;
    int m_totalCards = 0, m_doneCards = 0;
};

#endif // YANDEXSCRAPER_H
