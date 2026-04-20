#ifndef YANDEXSCRAPER_H
#define YANDEXSCRAPER_H

#include "scrapetask.h"

class YandexCollector;
class YandexParser;

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
    void onHrefsReady(QStringList hrefs);

    const int        m_gridN;
    YandexCollector* m_collector = nullptr;
    YandexParser*    m_parser    = nullptr;
};

#endif // YANDEXSCRAPER_H
