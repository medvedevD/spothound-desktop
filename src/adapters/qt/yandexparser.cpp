#include "yandexparser.h"
#include "yandex_js.h"
#include "captchaawarepage.h"
#include "stopwordsstore.h"
#include "core/rules.h"

#include <QDebug>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>

// -- Timing constants (ms) --
static constexpr int kCardLoadFailDelayMs = 500;
static constexpr int kCardReadyPollMs     = 400;
static constexpr int kCardBlockedDelayMs  = 300;
static constexpr int kCardBaseDelayMs     = 600;
static constexpr int kCardJitterMs        = 500;

// -- Parse strategy limits --
static constexpr int kCardReadyAttempts = 5;

static inline bool isCaptchaUrl(const QUrl& u) {
    if (!u.host().contains("yandex")) return false;
    const QString p = u.path();
    return p.contains("showcaptcha") || p.contains("captchapgrd") || p.contains("captcha");
}

static QString formatPhone(const QString& p) {
    static const QRegularExpression kNonDigit("[^0-9+]");
    QString d = p;
    d.remove(kNonDigit);
    if (d.startsWith("+7") && d.size() == 12) {
        return QString("+7 (%1) %2-%3-%4")
            .arg(d.mid(2,3)).arg(d.mid(5,3))
            .arg(d.mid(8,2)).arg(d.mid(10,2));
    }
    return p;
}

YandexParser::YandexParser(QString query, QString city,
                           QWebEngineProfile* profile,
                           std::vector<std::string> scoreKeywords,
                           StopWordsStore* stopWordsStore,
                           core::ScrapeStats& stats,
                           QObject* parent)
    : QObject(parent)
    , m_query(std::move(query))
    , m_city(std::move(city))
    , m_profile(profile)
    , m_scoreKeywords(std::move(scoreKeywords))
    , m_stopWordsStore(stopWordsStore)
    , m_stats(stats)
{}

void YandexParser::start(QStringList hrefs)
{
    m_aborted = false;
    m_parse   = {};
    m_parse.hrefQueue  = std::move(hrefs);
    m_parse.totalCards = m_parse.hrefQueue.size();
    emit parseProgress(m_parse.totalCards, 0);
    processQueue();
}

void YandexParser::abort()
{
    m_aborted = true;
    m_parse   = {};
}

void YandexParser::processQueue()
{
    if (m_aborted) return;

    qDebug() << "[YP] queue size:" << m_parse.hrefQueue.size();

    if (m_parse.hrefQueue.isEmpty()) {
        emit parseProgress(m_parse.totalCards, m_parse.totalCards);
        emit done();
        return;
    }

    QUrl href(m_parse.hrefQueue.takeFirst());
    if (href.host() == "yandex.com") href.setHost("yandex.ru");
    openCard(href);
}

void YandexParser::openCard(const QUrl& href)
{
    auto* page = new CaptchaAwarePage(m_profile, isCaptchaUrl, this);

    connect(page, &QWebEnginePage::loadProgress, this, [page](int p){
        qDebug() << "[VIEW]" << p << page->url();
    });
    connect(page, &QWebEnginePage::urlChanged, this, [](const QUrl& u){
        qDebug() << "[URL]" << u;
    });
    connect(page, &QWebEnginePage::loadFinished, this, [this, page, href](bool ok){
        if (!ok) {
            qDebug() << "[YP] card load failed:" << href;
            m_stats.failedCards++;
            page->deleteLater();
            QTimer::singleShot(kCardLoadFailDelayMs, this, &YandexParser::processQueue);
            return;
        }
        waitForCardReady(page, kCardReadyAttempts);
    });

    m_cardTimer.start();
    qDebug() << "[YP] openCard" << href;
    emit preview(page, "Карточка: " + href.toString());
    page->load(href);
}

void YandexParser::waitForCardReady(QWebEnginePage* page, int attemptsLeft)
{
    QPointer<QWebEnginePage> wp = page;
    page->runJavaScript(kYandexJsCardReady, [this, wp, attemptsLeft](const QVariant& v){
        if (!wp) return;
        if (v.toBool() || attemptsLeft <= 0) {
            extractCardData(wp);
        } else {
            m_stats.probeRetries++;
            QTimer::singleShot(kCardReadyPollMs, this, [this, wp, attemptsLeft]{
                if (wp) waitForCardReady(wp, attemptsLeft - 1);
            });
        }
    });
}

void YandexParser::extractCardData(QWebEnginePage* page)
{
    QPointer<QWebEnginePage> wp = page;
    page->runJavaScript(kYandexJsExtractCard, [this, wp](const QVariant& v){
        if (wp) handleCardResult(wp, v.toMap());
    });
}

void YandexParser::handleCardResult(QWebEnginePage* page, const QVariantMap& m)
{
    QStringList sites;
    for (const auto& it : m.value("merged").toList()) sites << it.toString();

    QStringList phones;
    for (const auto& it : m.value("tels").toList()) phones << it.toString();
    for (auto& ph : phones) ph = formatPhone(ph);
    phones.removeDuplicates();

    core::PlaceRow r;
    r.source  = "Яндекс.Карты";
    r.query   = (m_query + " " + m_city).toStdString();
    r.name    = m.value("name").toString().toStdString();
    r.address = m.value("addr").toString().toStdString();
    r.phone   = phones.join(" | ").toStdString();
    r.site    = sites.join(", ").toStdString();
    r.descr   = m.value("descr").toString().toStdString();
    auto [sc, why] = core::Rules::score(r.name, r.descr, !r.site.empty(), m_scoreKeywords);
    r.score = sc; r.why = std::move(why);

    const bool blocked = m_stopWordsStore && m_stopWordsStore->matchesRow(r);
    if (blocked) {
        qDebug() << "[YP] blocked:" << QString::fromStdString(r.name);
        m_stats.blockedCards++;
        page->deleteLater();
        QTimer::singleShot(kCardBlockedDelayMs, this, &YandexParser::processQueue);
        return;
    }

    emit result(r);
    const qint64 cardMs = m_cardTimer.elapsed();
    m_stats.cardCount++;
    m_stats.totalCardProcessMs += cardMs;
    if (m_stats.minCardMs < 0 || cardMs < m_stats.minCardMs) m_stats.minCardMs = cardMs;
    if (cardMs > m_stats.maxCardMs) m_stats.maxCardMs = cardMs;
    page->deleteLater();

    m_parse.doneCards++;
    emit parseProgress(m_parse.totalCards, m_parse.doneCards);
    QTimer::singleShot(kCardBaseDelayMs + QRandomGenerator::global()->bounded(kCardJitterMs),
                       this, &YandexParser::processQueue);
}
