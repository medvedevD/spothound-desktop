#include <gtest/gtest.h>

#include "adapters/qt/yandex_js.h"

#include <QEventLoop>
#include <QFile>
#include <QSignalSpy>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QWebEnginePage>

// Base URL for setHtml — makes a.href resolve correctly.
static const QUrl kBaseUrl("https://yandex.ru/maps/org/test/123456/");

static QString loadFixture(const char* name)
{
    const QString path = QString(FIXTURES_DIR) + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

// Loads HTML into a page synchronously (blocks until loadFinished).
static bool loadHtml(QWebEnginePage& page, const QString& html)
{
    QSignalSpy spy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(html, kBaseUrl);
    return spy.wait(10000);
}

// Runs JS synchronously, returns the QVariantMap result.
static QVariantMap extractCard(QWebEnginePage& page)
{
    QEventLoop loop;
    QVariantMap result;
    page.runJavaScript(kYandexJsExtractCard, [&](const QVariant& v) {
        result = v.toMap();
        loop.quit();
    });
    loop.exec();
    return result;
}

// ---------------------------------------------------------------------------

TEST(YandexCardParser, BasicCard_ExtractsNameAndAddress)
{
    const QString html = loadFixture("card_basic.html");
    ASSERT_FALSE(html.isEmpty());

    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, html));

    const auto r = extractCard(page);
    EXPECT_EQ(r["name"].toString(), "Кофейня Аромат");
    EXPECT_EQ(r["addr"].toString(), "ул. Ленина, 15, Воронеж");
}

TEST(YandexCardParser, BasicCard_ExtractsPhone)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_basic.html")));

    const auto tels = extractCard(page)["tels"].toList();
    ASSERT_EQ(tels.size(), 1);
    EXPECT_EQ(tels[0].toString(), "+74732345678");
}

TEST(YandexCardParser, BasicCard_ExtractsWebsite)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_basic.html")));

    const auto merged = extractCard(page)["merged"].toList();
    ASSERT_FALSE(merged.isEmpty());
    const bool found = std::any_of(merged.begin(), merged.end(), [](const QVariant& v){
        return v.toString().contains("aromat-coffee.ru");
    });
    EXPECT_TRUE(found);
}

TEST(YandexCardParser, BasicCard_ExtractsOgDescription)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_basic.html")));

    EXPECT_EQ(extractCard(page)["descr"].toString(),
              "Уютная кофейня в центре города с авторскими напитками");
}

TEST(YandexCardParser, DataTestidSelectors_ExtractsNameAndAddress)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_data_testid.html")));

    const auto r = extractCard(page);
    EXPECT_EQ(r["name"].toString(), "Автосервис Мотор");
    EXPECT_EQ(r["addr"].toString(), "пр. Победы, 42, Краснодар");
}

TEST(YandexCardParser, DataTestidSelectors_ExtractsPhone)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_data_testid.html")));

    const auto tels = extractCard(page)["tels"].toList();
    ASSERT_EQ(tels.size(), 1);
    EXPECT_EQ(tels[0].toString(), "+78612001122");
}

TEST(YandexCardParser, NoContact_EmptyPhoneAndSites)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_no_contact.html")));

    const auto r = extractCard(page);
    EXPECT_EQ(r["name"].toString(), "Городской парк культуры и отдыха");
    EXPECT_TRUE(r["tels"].toList().isEmpty());
    EXPECT_TRUE(r["merged"].toList().isEmpty());
}

TEST(YandexCardParser, SocialLinks_MergedContainsInstagramAndVk)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_social.html")));

    const auto merged = extractCard(page)["merged"].toList();
    ASSERT_FALSE(merged.isEmpty());

    auto contains = [&](const QString& sub) {
        return std::any_of(merged.begin(), merged.end(), [&](const QVariant& v){
            return v.toString().contains(sub);
        });
    };
    EXPECT_TRUE(contains("instagram.com"));
    EXPECT_TRUE(contains("vk.com"));
    EXPECT_TRUE(contains("lady-salon.ru"));
}

TEST(YandexCardParser, SocialLinks_InstFieldSet)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_social.html")));

    EXPECT_TRUE(extractCard(page)["inst"].toString().contains("instagram.com"));
}

TEST(YandexCardParser, PhoneInBody_ExtractedByRegex)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_phone_in_body.html")));

    const auto tels = extractCard(page)["tels"].toList();
    ASSERT_FALSE(tels.isEmpty());
    EXPECT_EQ(tels[0].toString(), "+78463210055");
}

TEST(YandexCardParser, PhoneInBody_ExtractsDataTypeAddress)
{
    QWebEnginePage page;
    ASSERT_TRUE(loadHtml(page, loadFixture("card_phone_in_body.html")));

    EXPECT_EQ(extractCard(page)["addr"].toString(), "ул. Советская, 77, Самара");
}
