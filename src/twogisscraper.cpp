#include "twogisscraper.h"
#include "cityregistry.h"
#include "captchaawarepage.h"
#include "rules.h"

#include <QTimer>
#include <QUrlQuery>
#include <QRandomGenerator>

static inline bool is2GisCaptcha(const QUrl& u) {
    const QString host = u.host();
    if (!host.contains("2gis")) return false;
    const QString p = u.path();
    return p.contains("challenge") || p.contains("blocked") || p.contains("captcha");
}

TwoGisScraper::TwoGisScraper(QString query, QString city,
                             QWebEngineProfile* profile,
                             StopWordsStore* stopWordsStore,
                             QStringList scoreKeywords,
                             QObject* p)
    : ScrapeTask(std::move(query), std::move(city), profile, stopWordsStore, std::move(scoreKeywords), p)
{}

void TwoGisScraper::start()
{
    m_aborted = false;
    m_parsingStarted = false;
    m_stats = {};
    m_stats.source = QStringLiteral("2ГИС");
    m_stats.query = m_query + " " + m_city;
    m_stats.city = m_city;
    m_collectionTimer.start();

    m_seen.clear();
    m_hrefQueue.clear();
    m_pass = 0; m_idlePass = 0; m_delay = 1200;
    m_totalCards = m_doneCards = 0;

    emit phaseChanged("Collecting from 2GIS");
    emit gridProgress(1, 0);

    const CityGeo* c = CityRegistry::find(m_city);
    const QString slug = c ? QString::fromUtf8(c->twoGisSlug) : m_city.trimmed().toLower().replace(' ', '_');

    QUrl u;
    u.setScheme("https");
    u.setHost("2gis.ru");
    u.setPath(QString("/%1/search/%2").arg(slug, m_query));
    m_searchUrl = u;

    openSearch();
}

void TwoGisScraper::reset()
{
    m_aborted = true;
    if (m_searchPage) { m_searchPage->deleteLater(); m_searchPage = nullptr; }
    m_hrefQueue.clear();
    m_seen.clear();
    m_pass = m_idlePass = 0;
    m_totalCards = m_doneCards = 0;

    emit phaseChanged(QStringLiteral("Click the 'start' button to start parsing"));
    emit gridProgress(0, 0);
    emit queueSized(0);
    emit parseProgress(0, 0);
}

void TwoGisScraper::openSearch()
{
    m_searchPage = new CaptchaAwarePage(m_profile, is2GisCaptcha, this);

    connect(m_searchPage, &QWebEnginePage::loadFinished, this, [this](bool ok){
        if (!ok) { emit finished(); return; }
        if (is2GisCaptcha(m_searchPage->url())) {
            QMetaObject::Connection* conn = new QMetaObject::Connection;
            *conn = connect(m_searchPage, &QWebEnginePage::urlChanged, this, [this, conn](const QUrl& u){
                if (!is2GisCaptcha(u)) {
                    QObject::disconnect(*conn); delete conn;
                    m_searchPage->load(m_searchUrl);
                    QTimer::singleShot(1500, this, &TwoGisScraper::collectHrefsStep);
                }
            });
            return;
        }
        QTimer::singleShot(2000, this, &TwoGisScraper::collectHrefsStep);
    });

    qDebug() << "[2GIS] url=" << m_searchUrl;
    emit preview(m_searchPage, "2GIS поиск: " + m_query + " / " + m_city);
    m_searchPage->load(m_searchUrl);
}

void TwoGisScraper::collectHrefsStep()
{
    if (m_aborted) return;

    QWebEnginePage* page = m_searchPage;
    if (!page) { emit finished(); return; }

    static const QString js = R"JS(
(function(){
  // Find the scrollable sidebar
  function isScrollable(el){
    if (!el) return false;
    const cs = getComputedStyle(el);
    return (cs.overflowY === 'auto' || cs.overflowY === 'scroll') && el.scrollHeight > el.clientHeight + 20;
  }
  function findFeed(){
    // 2GIS sidebar selectors
    const seeds = [
      document.querySelector('[class*="searchList"]'),
      document.querySelector('[class*="search-list"]'),
      document.querySelector('[class*="CitySearch"]'),
      document.querySelector('[data-module="SearchList"]'),
      document.querySelector('[class*="sidebar"]'),
      document.querySelector('[class*="Sidebar"]'),
    ].filter(Boolean);
    for (let el of seeds) {
      let cur = el;
      for (let k = 0; k < 6 && cur; k++, cur = cur.parentElement)
        if (isScrollable(cur)) return cur;
    }
    // fallback: any narrow scrollable column
    const all = Array.from(document.querySelectorAll('div'));
    for (let el of all) {
      const r = el.getBoundingClientRect();
      if (r.width > 260 && r.width < 700 && r.height > 300 && isScrollable(el)) return el;
    }
    return null;
  }
  const feed = findFeed();
  const scope = feed || document;

  // Collect firm links
  const firmLinks = Array.from(scope.querySelectorAll('a[href*="/firm/"]'));
  const out = [], seen = {};
  for (const a of firmLinks) {
    let u = a.href || '';
    if (!u || !u.includes('2gis')) continue;
    // normalize: strip query params, keep base firm URL
    const m = u.match(/(https?:\/\/[^\/]+\/[^\/]+\/(?:firm|branches)\/\d+)/);
    if (!m) continue;
    const key = m[1];
    if (seen[key]) continue;
    seen[key] = true;
    out.push(key);
  }

  // Scroll to load more
  if (feed) {
    feed.scrollTop += 3000;
  } else {
    window.scrollBy(0, 3000);
  }

  const itemsCount = scope.querySelectorAll('a[href*="/firm/"]').length;
  return { urls: out, hasFeed: !!feed, itemsCount: itemsCount };
})();
)JS";

    page->runJavaScript(js, [this, page](const QVariant& v){
        if (!page) return;

        const auto map = v.toMap();
        const auto arr = map.value("urls").toList();

        const int before = m_seen.size();
        for (const auto& it : arr) m_seen.insert(it.toString());
        const int after = m_seen.size();

        qDebug() << "[2GIS] pass" << m_pass << "hrefs" << after
                 << "hasFeed:" << map.value("hasFeed").toBool()
                 << "items:" << map.value("itemsCount").toInt();

        if (after == before) { ++m_idlePass; m_delay = qMin(m_delay + 300, 2000); }
        else                 { m_idlePass = 0; m_delay = 1000; }

        const bool stop = (after >= 60) || (m_pass >= m_maxPass) || (m_idlePass >= 6);

        if (stop) {
            m_hrefQueue = QStringList(m_seen.begin(), m_seen.end());
            std::sort(m_hrefQueue.begin(), m_hrefQueue.end());
            qDebug() << "[2GIS] collected" << m_hrefQueue.size() << "hrefs";

            if (m_searchPage) { m_searchPage->deleteLater(); m_searchPage = nullptr; }

            emit gridProgress(1, 1);
            m_totalCards = m_hrefQueue.size();
            m_doneCards = 0;
            emit queueSized(m_totalCards);
            emit phaseChanged("Parsing 2GIS cards");
            emit parseProgress(m_totalCards, 0);

            processQueue();
            return;
        }

        ++m_pass;
        QTimer::singleShot(m_delay, this, &TwoGisScraper::collectHrefsStep);
    });
}

void TwoGisScraper::processQueue()
{
    if (m_aborted) return;

    if (!m_parsingStarted) {
        m_stats.collectionMs = m_collectionTimer.elapsed();
        m_parsingTimer.start();
        m_parsingStarted = true;
    }

    if (m_hrefQueue.isEmpty()) {
        m_stats.parsingMs = m_parsingTimer.elapsed();
        emit parseProgress(m_totalCards, m_totalCards);
        emit phaseChanged("idle");
        emitStats();
        emit finishedAll();
        emit finished();
        return;
    }

    QUrl href(m_hrefQueue.takeFirst());
    openCard(href);
}

void TwoGisScraper::openCard(const QUrl& href)
{
    auto* page = new CaptchaAwarePage(m_profile, is2GisCaptcha, this);

    connect(page, &QWebEnginePage::loadFinished, this, [this, page](bool ok){
        if (!ok) {
            page->deleteLater();
            QTimer::singleShot(500, this, &TwoGisScraper::processQueue);
            return;
        }

        // Wait for card content to load
        auto probe = new std::function<void(int)>;
        *probe = [this, page, probe](int left){
            static const QString readyJS = R"JS(
              (function(){
                var hasName = !!document.querySelector('h1, [class*="orgName"], [class*="OrgName"]');
                var hasContact = !!document.querySelector('a[href^="tel:"], [class*="contact"]');
                return hasName || hasContact;
              })();
            )JS";
            page->runJavaScript(readyJS, [this, page, left, probe](const QVariant& v){
                if (v.toBool() || left <= 0) {
                    static const QString js = R"JS(
                    (function(){
                      function qsa(s){ return Array.from(document.querySelectorAll(s)); }

                      // Name
                      var nameEl = document.querySelector('h1, [class*="orgName"], [class*="OrgName"], [class*="header__name"]');
                      var name = nameEl ? nameEl.textContent.trim() : '';

                      // Address
                      var addrEl = document.querySelector(
                        '[class*="address"], [itemprop="streetAddress"], [class*="Address"], ' +
                        '[data-testid*="address"], [class*="geo"]'
                      );
                      var addr = addrEl ? addrEl.textContent.trim() : '';

                      // Phone
                      var tels = [];
                      qsa('a[href^="tel:"]').forEach(function(a){
                        var t = (a.href || '').replace(/^tel:/i, '').trim();
                        if (t) tels.push(t);
                      });

                      // Website — external links only
                      var sites = [];
                      var seen = {};
                      qsa('a[href^="http"]').forEach(function(a){
                        var u = a.href;
                        if (!u || u.indexOf('2gis') >= 0) return;
                        if (seen[u]) return; seen[u] = true;
                        sites.push(u);
                      });

                      // Description
                      var descrEl = document.querySelector(
                        'meta[property="og:description"], meta[name="description"]'
                      );
                      var descr = descrEl ? (descrEl.content || descrEl.getAttribute('content') || '') : '';
                      if (!descr) {
                        var dEl = document.querySelector('[class*="description"], [class*="Description"]');
                        if (dEl) descr = dEl.textContent.trim();
                      }

                      return { name: name, addr: addr, tels: tels, sites: sites, descr: descr };
                    })();
                    )JS";
                    page->runJavaScript(js, [this, page](const QVariant& v){
                        const auto m = v.toMap();

                        PlaceRow r;
                        r.source  = "2ГИС";
                        r.query   = m_query + " " + m_city;
                        r.name    = m.value("name").toString();
                        r.address = m.value("addr").toString();
                        r.descr   = m.value("descr").toString();

                        QStringList phones;
                        for (const auto& it : m.value("tels").toList())
                            phones << it.toString();
                        phones.removeDuplicates();
                        r.phone = phones.join(" | ");

                        QStringList sites;
                        for (const auto& it : m.value("sites").toList())
                            sites << it.toString();
                        r.site = sites.join(", ");

                        auto [sc, why] = Rules::score(r.name, r.descr, !r.site.isEmpty(), m_scoreKeywords);
                        r.score = sc; r.why = why;

                        if (isBlocked(r)) {
                            qDebug() << "[2GIS] skip" << r.name;
                            page->deleteLater();
                            QTimer::singleShot(300, this, &TwoGisScraper::processQueue);
                            return;
                        }

                        emit result(r);
                        const qint64 cardMs = m_cardTimer.elapsed();
                        m_stats.cardCount++;
                        m_stats.totalCardProcessMs += cardMs;
                        if (m_stats.minCardMs < 0 || cardMs < m_stats.minCardMs) m_stats.minCardMs = cardMs;
                        m_stats.maxCardMs = qMax(m_stats.maxCardMs, cardMs);
                        page->deleteLater();

                        m_doneCards++;
                        emit parseProgress(m_totalCards, m_doneCards);

                        QTimer::singleShot(500 + QRandomGenerator::global()->bounded(400),
                                           this, &TwoGisScraper::processQueue);
                    });
                } else {
                    m_stats.probeRetries++;
                    QTimer::singleShot(400, [probe, left]{ (*probe)(left - 1); });
                }
            });
        };
        (*probe)(5);
    });

    m_cardTimer.start();
    qDebug() << "[2GIS] openCard" << href;
    emit preview(page, "2GIS карточка: " + href.toString());
    page->load(href);
}
