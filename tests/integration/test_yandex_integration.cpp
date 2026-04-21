#include <gtest/gtest.h>

#include "adapters/qt/yandex_js.h"

#include <QEventLoop>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <QWebEnginePage>
#include <QWebEngineProfile>

static constexpr int kPageLoadTimeoutMs = 30000;
static constexpr int kReadyPollMs       = 500;
static constexpr int kReadyAttempts     = 20;   // 10 секунд

static bool isCaptcha(const QUrl& url)
{
    const QString s = url.toString();
    return s.contains("showcaptcha") || s.contains("captchapgrd") || s.contains("captcha");
}

static bool waitForJs(QWebEnginePage& page, const QString& js)
{
    for (int i = 0; i < kReadyAttempts; ++i) {
        bool ready = false;
        QEventLoop loop;
        page.runJavaScript(js, [&](const QVariant& v) {
            ready = v.toBool();
            loop.quit();
        });
        loop.exec();
        if (ready) return true;
        QTest::qWait(kReadyPollMs);
    }
    return false;
}

static QVariantMap runJsMap(QWebEnginePage& page, const QString& js)
{
    QVariantMap result;
    QEventLoop loop;
    page.runJavaScript(js, [&](const QVariant& v) {
        result = v.toMap();
        loop.quit();
    });
    loop.exec();
    return result;
}

static bool loadUrl(QWebEnginePage& page, const QUrl& url)
{
    QSignalSpy spy(&page, &QWebEnginePage::loadFinished);
    page.load(url);
    return spy.wait(kPageLoadTimeoutMs);
}

// Ждём пока появится адрес или телефон.
static const QString kJsContactsReady = QStringLiteral(
    "(function(){"
    "  var hasTel  = !!document.querySelector('a[href^=\"tel:\"]');"
    "  var hasAddr = !!document.querySelector('[class*=\"address\"],[data-type=\"address\"],[data-testid=\"address\"]');"
    "  return hasTel || hasAddr;"
    "})();"
);

// ---------------------------------------------------------------------------
// Фикстура: один процесс, одна загрузка страницы для всего suite.
// Шаг 1 — поисковая выдача → собираем первый org URL.
// Шаг 2 — открываем карточку этой org → парсим.
// ---------------------------------------------------------------------------

class YandexIntegration : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        s_profile = new QWebEngineProfile();
        s_profile->setHttpUserAgent(
            "Mozilla/5.0 (X11; Linux x86_64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/124.0.0.0 Safari/537.36");
        s_page = new QWebEnginePage(s_profile);

        // Шаг 1: загружаем поиск «кофейня москва»
        const QUrl searchUrl("https://yandex.ru/maps/213/moscow/?text=%D0%BA%D0%BE%D1%84%D0%B5%D0%B9%D0%BD%D1%8F");
        if (!loadUrl(*s_page, searchUrl)) {
            s_skipReason = "Search page load timed out (no network?)";
            return;
        }
        if (isCaptcha(s_page->url())) {
            s_skipReason = "Yandex showed CAPTCHA on search page";
            return;
        }

        // Ждём появления результатов поиска
        const QString kJsHasResults = QStringLiteral(
            "document.querySelectorAll('a[href*=\"/maps/org/\"]').length > 0;"
        );
        if (!waitForJs(*s_page, kJsHasResults)) {
            s_skipReason = "No search results appeared";
            return;
        }

        // Собираем первый org URL
        const auto collected = runJsMap(*s_page, kYandexJsScrollAndCollect);
        const auto urls = collected["urls"].toList();
        if (urls.isEmpty()) {
            s_skipReason = "Collector found no org URLs";
            return;
        }
        const QUrl orgUrl(urls.first().toString());

        // Шаг 2: открываем карточку
        if (!loadUrl(*s_page, orgUrl)) {
            s_skipReason = "Card page load timed out";
            return;
        }
        if (isCaptcha(s_page->url())) {
            s_skipReason = "Yandex showed CAPTCHA on card page";
            return;
        }

        waitForJs(*s_page, kYandexJsCardReady);
        waitForJs(*s_page, kJsContactsReady);
        s_result = runJsMap(*s_page, kYandexJsExtractCard);
    }

    static void TearDownTestSuite()
    {
        delete s_page;    s_page    = nullptr;
        delete s_profile; s_profile = nullptr;
    }

protected:
    void SetUp() override
    {
        if (!s_skipReason.isEmpty())
            GTEST_SKIP() << s_skipReason.toStdString();
    }

    static QWebEngineProfile* s_profile;
    static QWebEnginePage*    s_page;
    static QVariantMap        s_result;
    static QString            s_skipReason;
};

QWebEngineProfile* YandexIntegration::s_profile   = nullptr;
QWebEnginePage*    YandexIntegration::s_page       = nullptr;
QVariantMap        YandexIntegration::s_result     = {};
QString            YandexIntegration::s_skipReason = {};

// ---------------------------------------------------------------------------

TEST_F(YandexIntegration, DiagDumpResult)
{
    qDebug() << "url   =" << s_page->url().toString();
    qDebug() << "name  =" << s_result["name"].toString();
    qDebug() << "addr  =" << s_result["addr"].toString();
    qDebug() << "tels  =" << s_result["tels"].toList();
    qDebug() << "merged=" << s_result["merged"].toList();
    SUCCEED();
}

TEST_F(YandexIntegration, ExtractsNonEmptyName)
{
    EXPECT_FALSE(s_result["name"].toString().isEmpty());
}

TEST_F(YandexIntegration, ExtractsNonEmptyAddress)
{
    EXPECT_FALSE(s_result["addr"].toString().isEmpty());
}

TEST_F(YandexIntegration, ExtractsAtLeastOnePhone)
{
    EXPECT_FALSE(s_result["tels"].toList().isEmpty());
}
