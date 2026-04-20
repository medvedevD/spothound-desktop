#pragma once

#include "core/scrape_stats.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QUrl>
#include <QVector>

#include <functional>

class CaptchaAwarePage;
class QWebEnginePage;
class QWebEngineProfile;

class YandexCollector : public QObject {
    Q_OBJECT
public:
    YandexCollector(QString query, QString city,
                    QWebEngineProfile* profile,
                    int gridN,
                    core::ScrapeStats& stats,
                    QObject* parent = nullptr);

    void start();
    void abort();

signals:
    void hrefsReady(QStringList hrefs);
    void gridProgress(int total, int done);
    void preview(QWebEnginePage* page, const QString& label);
    void error();

private:
    void buildCells();
    void nextCell();
    void openSearch();
    void collectHrefs();
    void collectHrefsStep();
    void waitListStable(QWebEnginePage* page, std::function<void()> cont);
    void waitForNewOrgs(QWebEnginePage* page, int prevCount);

    struct GridState {
        QVector<QPointF> cells;
        int     cellIdx    = 0;
        int     cellZoom   = 0;
        int     totalCells = 0, doneCells = 0;
        int     yandexRid  = 213;
        QString yandexSlug = "moscow";
    };

    struct ScrollState {
        CaptchaAwarePage* page = nullptr;
        QUrl    searchUrl;
        QSet<QString> seen;
        QSet<QString> seenGlobal;
        int  pass     = 0;
        int  idlePass = 0;
        bool forcedRu = false;
    };

    const QString      m_query;
    const QString      m_city;
    QWebEngineProfile* m_profile;
    const int          m_gridN;
    core::ScrapeStats& m_stats;

    bool          m_aborted = false;
    QElapsedTimer m_scrollTimer;
    GridState     m_grid;
    ScrollState   m_scroll;
};
