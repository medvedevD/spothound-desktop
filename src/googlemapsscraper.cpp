#include "googlemapsscraper.h"
#include "captchaawarepage.h"
#include "rules.h"

#include <QTimer>
#include <QRandomGenerator>

static inline bool isGoogleCaptcha(const QUrl& u) {
    const QString host = u.host();
    return (host.contains("google.com") && u.path().contains("/sorry/"))
        || host.contains("accounts.google.com");
}

GoogleMapsScraper::GoogleMapsScraper(QString query, QString city,
                                     QWebEngineProfile* profile,
                                     StopWordsStore* stopWordsStore,
                                     QStringList scoreKeywords,
                                     QObject* p)
    : ScrapeTask(std::move(query), std::move(city), profile, stopWordsStore, std::move(scoreKeywords), p)
{}

void GoogleMapsScraper::start()
{
    m_aborted = false;
    m_seen.clear();
    m_hrefQueue.clear();
    m_pass = 0; m_idlePass = 0; m_delay = 1200;
    m_totalCards = m_doneCards = 0;

    emit phaseChanged("Collecting from Google Maps");
    emit gridProgress(1, 0);

    const QString searchTerm = m_query + " " + m_city;
    QUrl u;
    u.setScheme("https");
    u.setHost("www.google.com");
    u.setPath("/maps/search/" + QUrl::toPercentEncoding(searchTerm));
    m_searchUrl = u;

    openSearch();
}

void GoogleMapsScraper::reset()
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

void GoogleMapsScraper::openSearch()
{
    m_searchPage = new CaptchaAwarePage(m_profile, isGoogleCaptcha, this);

    connect(m_searchPage, &QWebEnginePage::loadFinished, this, [this](bool ok){
        if (!ok) { emit finished(); return; }
        if (isGoogleCaptcha(m_searchPage->url())) {
            QMetaObject::Connection* conn = new QMetaObject::Connection;
            *conn = connect(m_searchPage, &QWebEnginePage::urlChanged, this, [this, conn](const QUrl& u){
                if (!isGoogleCaptcha(u)) {
                    QObject::disconnect(*conn); delete conn;
                    m_searchPage->load(m_searchUrl);
                    QTimer::singleShot(2000, this, &GoogleMapsScraper::collectHrefsStep);
                }
            });
            return;
        }
        QTimer::singleShot(2500, this, &GoogleMapsScraper::collectHrefsStep);
    });

    qDebug() << "[GMaps] url=" << m_searchUrl;
    emit preview(m_searchPage, "Google Maps: " + m_query + " / " + m_city);
    m_searchPage->load(m_searchUrl);
}

void GoogleMapsScraper::collectHrefsStep()
{
    if (m_aborted) return;

    QWebEnginePage* page = m_searchPage;
    if (!page) { emit finished(); return; }

    static const QString js = R"JS(
(function(){
  // Find the scrollable results feed
  var feed = document.querySelector('[role="feed"]');
  if (!feed) {
    // fallback: look for the sidebar results panel
    var all = Array.from(document.querySelectorAll('div'));
    for (var el of all) {
      var r = el.getBoundingClientRect();
      if (r.width > 200 && r.width < 600 && r.height > 400) {
        var cs = getComputedStyle(el);
        if ((cs.overflowY === 'auto' || cs.overflowY === 'scroll') && el.scrollHeight > el.clientHeight + 20) {
          feed = el;
          break;
        }
      }
    }
  }
  var scope = feed || document;

  // Collect place links
  var placeLinks = Array.from(scope.querySelectorAll('a[href*="/maps/place/"]'));
  var out = [], seen = {};
  for (var a of placeLinks) {
    var u = a.href || '';
    if (!u) continue;
    // normalize: strip after place name
    var m = u.match(/(https?:\/\/[^\/]+\/maps\/place\/[^\/]+\/[^?#]+)/);
    var key = m ? m[1] : u.split('?')[0];
    if (seen[key]) continue;
    seen[key] = true;
    out.push(u.split('?')[0]); // strip query params
  }

  // Scroll to load more
  if (feed) {
    feed.scrollTop += 3000;
  } else {
    window.scrollBy(0, 3000);
  }

  return { urls: out, hasFeed: !!feed, itemsCount: placeLinks.length };
})();
)JS";

    page->runJavaScript(js, [this, page](const QVariant& v){
        if (!page) return;

        const auto map = v.toMap();
        const auto arr = map.value("urls").toList();

        const int before = m_seen.size();
        for (const auto& it : arr) m_seen.insert(it.toString());
        const int after = m_seen.size();

        qDebug() << "[GMaps] pass" << m_pass << "hrefs" << after
                 << "hasFeed:" << map.value("hasFeed").toBool();

        if (after == before) { ++m_idlePass; m_delay = qMin(m_delay + 300, 2000); }
        else                 { m_idlePass = 0; m_delay = 1200; }

        const bool stop = (after >= 60) || (m_pass >= m_maxPass) || (m_idlePass >= 5);

        if (stop) {
            m_hrefQueue = QStringList(m_seen.begin(), m_seen.end());
            std::sort(m_hrefQueue.begin(), m_hrefQueue.end());
            qDebug() << "[GMaps] collected" << m_hrefQueue.size() << "hrefs";

            if (m_searchPage) { m_searchPage->deleteLater(); m_searchPage = nullptr; }

            emit gridProgress(1, 1);
            m_totalCards = m_hrefQueue.size();
            m_doneCards = 0;
            emit queueSized(m_totalCards);
            emit phaseChanged("Parsing Google Maps cards");
            emit parseProgress(m_totalCards, 0);

            processQueue();
            return;
        }

        ++m_pass;
        QTimer::singleShot(m_delay, this, &GoogleMapsScraper::collectHrefsStep);
    });
}

void GoogleMapsScraper::processQueue()
{
    if (m_aborted) return;

    if (m_hrefQueue.isEmpty()) {
        emit parseProgress(m_totalCards, m_totalCards);
        emit phaseChanged("idle");
        emit finishedAll();
        emit finished();
        return;
    }

    QUrl href(m_hrefQueue.takeFirst());
    openCard(href);
}

void GoogleMapsScraper::openCard(const QUrl& href)
{
    auto* page = new CaptchaAwarePage(m_profile, isGoogleCaptcha, this);

    connect(page, &QWebEnginePage::loadFinished, this, [this, page](bool ok){
        if (!ok) {
            page->deleteLater();
            QTimer::singleShot(500, this, &GoogleMapsScraper::processQueue);
            return;
        }

        auto probe = new std::function<void(int)>;
        *probe = [this, page, probe](int left){
            static const QString readyJS = R"JS(
              (function(){
                var h1 = document.querySelector('h1');
                var hasData = document.querySelectorAll('[data-item-id]').length > 0;
                return !!(h1 || hasData);
              })();
            )JS";
            page->runJavaScript(readyJS, [this, page, left, probe](const QVariant& v){
                if (v.toBool() || left <= 0) {
                    static const QString js = R"JS(
                    (function(){
                      // Name
                      var h1 = document.querySelector('h1');
                      var name = h1 ? h1.textContent.trim() : '';

                      // Data items (address, phone, website)
                      var addr = '', phone = '', website = '';
                      Array.from(document.querySelectorAll('[data-item-id]')).forEach(function(el){
                        var id = el.getAttribute('data-item-id') || '';
                        if (id.indexOf('address') >= 0) {
                          addr = el.textContent.trim();
                        } else if (id.indexOf('phone') >= 0) {
                          // prefer the tel: link text
                          var a = el.querySelector('a[href^="tel:"]');
                          phone = a ? a.href.replace(/^tel:/i,'').trim() : el.textContent.trim();
                        } else if (id.indexOf('authority') >= 0) {
                          var a = el.querySelector('a[href^="http"]');
                          if (a) website = a.href;
                        }
                      });

                      // Fallback: aria-label based buttons
                      if (!addr) {
                        var addrEl = document.querySelector('[aria-label*="Address"], [data-tooltip*="address"]');
                        if (addrEl) addr = addrEl.textContent.trim();
                      }
                      if (!phone) {
                        var phoneEl = document.querySelector('[aria-label*="Phone"], [data-tooltip*="phone"], a[href^="tel:"]');
                        if (phoneEl) phone = (phoneEl.href || phoneEl.textContent || '').replace(/^tel:/i,'').trim();
                      }
                      if (!website) {
                        var webEl = document.querySelector('[aria-label*="Website"], [data-tooltip*="website"]');
                        if (webEl) {
                          var a = webEl.querySelector('a') || webEl;
                          website = (a.href || '').startsWith('http') ? a.href : '';
                        }
                      }

                      // Description from meta
                      var meta = document.querySelector('meta[property="og:description"]');
                      var descr = meta ? (meta.content || '') : '';

                      return { name: name, addr: addr, phone: phone, website: website, descr: descr };
                    })();
                    )JS";
                    page->runJavaScript(js, [this, page](const QVariant& v){
                        const auto m = v.toMap();

                        PlaceRow r;
                        r.source  = "Google Maps";
                        r.query   = m_query + " " + m_city;
                        r.name    = m.value("name").toString();
                        r.address = m.value("addr").toString();
                        r.phone   = m.value("phone").toString();
                        r.site    = m.value("website").toString();
                        r.descr   = m.value("descr").toString();
                        auto [sc, why] = Rules::score(r.name, r.descr, !r.site.isEmpty(), m_scoreKeywords);
                        r.score = sc; r.why = why;

                        if (isBlocked(r)) {
                            qDebug() << "[GMaps] skip" << r.name;
                            page->deleteLater();
                            QTimer::singleShot(300, this, &GoogleMapsScraper::processQueue);
                            return;
                        }

                        emit result(r);
                        page->deleteLater();

                        m_doneCards++;
                        emit parseProgress(m_totalCards, m_doneCards);

                        QTimer::singleShot(700 + QRandomGenerator::global()->bounded(500),
                                           this, &GoogleMapsScraper::processQueue);
                    });
                } else {
                    QTimer::singleShot(400, [probe, left]{ (*probe)(left - 1); });
                }
            });
        };
        (*probe)(5);
    });

    qDebug() << "[GMaps] openCard" << href;
    emit preview(page, "Google Maps карточка: " + href.toString());
    page->load(href);
}
