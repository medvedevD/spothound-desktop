#pragma once

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

class YandexParser : public QObject {
    Q_OBJECT
public:
    YandexParser(QString query, QString city,
                 QWebEngineProfile* profile,
                 std::vector<std::string> scoreKeywords,
                 StopWordsStore* stopWordsStore,
                 core::ScrapeStats& stats,
                 QObject* parent = nullptr);

    void start(QStringList hrefs);
    void abort();

signals:
    void result(core::PlaceRow row);
    void parseProgress(int total, int done);
    void preview(QWebEnginePage* page, const QString& label);
    void done();
    void error();

private:
    void processQueue();
    void openCard(const QUrl& href);
    void waitForCardReady(QWebEnginePage* page, int attemptsLeft);
    void extractCardData(QWebEnginePage* page);
    void handleCardResult(QWebEnginePage* page, const QVariantMap& data);

    struct ParseState {
        QStringList hrefQueue;
        int totalCards = 0, doneCards = 0;
    };

    const QString              m_query;
    const QString              m_city;
    QWebEngineProfile*         m_profile;
    std::vector<std::string>   m_scoreKeywords;
    StopWordsStore*            m_stopWordsStore;
    core::ScrapeStats&         m_stats;

    bool          m_aborted = false;
    QElapsedTimer m_cardTimer;
    ParseState    m_parse;
};
