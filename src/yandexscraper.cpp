#include "yandexscraper.h"
#include "cityregistry.h"
#include "loggingpage.h"
#include "captchaawarepage.h"
#include "rules.h"

#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QDateTime>

static inline bool isCaptchaUrl(const QUrl& u) {
    if (!u.host().contains("yandex")) return false;
    const QString p = u.path();
    return p.contains("showcaptcha") || p.contains("captchapgrd") || p.contains("captcha");
}

static inline bool isCom(const QUrl& u){ return u.host().endsWith("yandex.com"); }

YandexScraper::YandexScraper(QString query, QString city,
                             QWebEngineProfile* profile,
                             StopWordsStore* stopWordsStore,
                             QStringList scoreKeywords,
                             int gridN,
                             QObject* p)
    : ScrapeTask(std::move(query), std::move(city), profile, stopWordsStore, std::move(scoreKeywords), p)
    , m_gridN(qBound(2, gridN, 10))
{
    setupProfile(m_profile);
}

void YandexScraper::setupProfile(QWebEngineProfile* profile)
{
    if (!profile) return;
    auto* store = profile->cookieStore();

    QNetworkCookie gid("yandex_gid", "213");
    gid.setDomain(".yandex.ru"); gid.setPath("/");
    gid.setExpirationDate(QDateTime::currentDateTimeUtc().addYears(1));
    store->setCookie(gid);

    QNetworkCookie lang("yandex_language", "ru");
    lang.setDomain(".yandex.ru"); lang.setPath("/");
    lang.setExpirationDate(QDateTime::currentDateTimeUtc().addYears(1));
    store->setCookie(lang);
}

void YandexScraper::start()
{
    m_aborted = false;
    m_forcedRu = false;
    m_parsingStarted = false;
    m_stats = {};
    m_stats.source = QStringLiteral("Яндекс.Карты");
    m_stats.query = m_query + " " + m_city;
    m_stats.city = m_city;
    m_collectionTimer.start();

    m_seen.clear();

    m_seenGlobal.clear();
    m_cells.clear();
    m_cellIdx = 0;
    m_cellZoom = 14;
    buildCells();

    m_totalCells = m_cells.size();
    m_doneCells  = 0;
    m_stats.gridN = m_gridN;
    m_stats.gridCells = m_totalCells;
    emit phaseChanged("Collection of stores by zone");
    emit gridProgress(m_totalCells, m_doneCells);

    nextCell();
}

void YandexScraper::reset()
{
    m_aborted = true;
    m_forcedRu = false;

    if (m_searchPage) { m_searchPage->deleteLater(); m_searchPage = nullptr; }

    m_hrefQueue.clear();
    m_seen.clear();
    m_seenGlobal.clear();
    m_cells.clear();
    m_cellIdx = 0;
    m_totalCells = m_doneCells = 0;
    m_totalCards = m_doneCards = 0;
    m_idlePass = m_pass = 0;

    emit phaseChanged(QStringLiteral("Click the 'start' button to start parsing"));
    emit gridProgress(0, 0);
    emit queueSized(0);
    emit parseProgress(0, 0);
}

void YandexScraper::buildCells() {
    const CityGeo* c = CityRegistry::find(m_city);
    double lon0, lon1, lat0, lat1;
    if (c) {
        lon0 = c->bboxLon0; lon1 = c->bboxLon1;
        lat0 = c->bboxLat0; lat1 = c->bboxLat1;
        m_cellZoom = c->zoom;
    } else {
        lon0 = 37.30; lon1 = 37.90;
        lat0 = 55.55; lat1 = 55.95;
        m_cellZoom = 14;
    }
    const int nx = m_gridN, ny = m_gridN;
    for (int iy = 0; iy < ny; ++iy) {
        for (int ix = 0; ix < nx; ++ix) {
            double lon = lon0 + (lon1 - lon0) * (ix + 0.5) / nx;
            double lat = lat0 + (lat1 - lat0) * (iy + 0.5) / ny;
            m_cells.push_back(QPointF(lon, lat));
        }
    }
}

void YandexScraper::nextCell()
{
    if (m_cellIdx >= m_cells.size()) {
        QStringList all(m_seenGlobal.begin(), m_seenGlobal.end());
        std::sort(all.begin(), all.end());
        m_hrefQueue = all;
        qDebug() << "[YS] total hrefs:" << m_hrefQueue.size();

        m_totalCards = m_hrefQueue.size();
        m_doneCards  = 0;
        m_stats.queuedCards = m_totalCards;
        emit queueSized(m_totalCards);
        emit phaseChanged("parsing");
        emit parseProgress(m_totalCards, m_doneCards);

        processQueue();
        return;
    }

    qDebug() << QString("%1/%2").arg(m_cellIdx).arg(m_cells.size());

    const CityGeo* c = CityRegistry::find(m_city);
    const QString slug = c ? QString::fromUtf8(c->yandexSlug) : QStringLiteral("moscow");
    const int rid = c ? c->yandexRid : 213;

    const auto ll = m_cells[m_cellIdx++];
    QUrl u; u.setScheme("https"); u.setHost("yandex.ru");
    u.setPath(QString("/maps/%1/%2/search/%3/").arg(rid).arg(slug).arg(m_query));
    QUrlQuery q;
    q.addQueryItem("ll", QString("%1,%2").arg(ll.x(),0,'f',6).arg(ll.y(),0,'f',6));
    q.addQueryItem("z", QString::number(m_cellZoom));
    q.addQueryItem("mode", "search");
    u.setQuery(q);

    m_searchUrl = u;
    m_seen.clear(); m_pass = 0; m_idlePass = 0; m_delay = 1100;

    openSearch();
}

void YandexScraper::openSearch() {

    m_searchPage = new CaptchaAwarePage(m_profile, isCaptchaUrl, this);

    connect(m_searchPage, &QWebEnginePage::loadFinished, this, [this](bool ok){
        qDebug() << "[YS] search loadFinished:" << ok << m_searchPage->url();
        if (!ok) { emit finished(); return; }

        if (!m_forcedRu && m_searchPage->url().host() != "yandex.ru") {
            QUrl back = m_searchUrl;
            m_forcedRu = true;
            m_searchPage->load(back);
            return;
        }

        if (isCaptchaUrl(m_searchPage->url())) {
            QMetaObject::Connection *conn = new QMetaObject::Connection;
            *conn = connect(m_searchPage, &QWebEnginePage::urlChanged, this, [this, conn](const QUrl& u){
                if (!isCaptchaUrl(u)) {
                    QObject::disconnect(*conn); delete conn;
                    m_searchPage->load(m_searchUrl);
                    QTimer::singleShot(1200, this, &YandexScraper::collectHrefs);
                }
            });
            return;
        }

        m_searchPage->runJavaScript("window.scrollBy(0,1200);", [](const QVariant&){});
        QTimer::singleShot(500, this, &YandexScraper::collectHrefs);
    });

    qDebug() << "[YS] url=" << m_searchUrl;
    emit preview(m_searchPage, "Поиск: " + m_query + " / " + m_city);
    m_searchPage->load(m_searchUrl);
}

void YandexScraper::collectHrefs() {
    m_seen.clear(); m_pass = 0; m_idlePass = 0; m_delay = 800;
    QTimer::singleShot(700, this, &YandexScraper::collectHrefsStep);
}

void YandexScraper::waitListStable(QWebEnginePage* page, std::function<void()> cont) {
    QPointer<QWebEnginePage> p = page;
    auto attempts = QSharedPointer<int>::create(4);
    auto last     = QSharedPointer<int>::create(-1);

    static const QString jsCount = R"JS(
      (function(){
        return (document.querySelectorAll('[role="listitem"], [data-testid="search-snippet-view"]')||[]).length;
      })();
    )JS";

    auto poll = QSharedPointer<std::function<void()>>::create();
    *poll = [this, p, attempts, last, cont, poll]() mutable {
        if (!p) return;
        p->runJavaScript(jsCount, [this, p, attempts, last, cont, poll](const QVariant& v){
            if (!p) return;
            const int cur = v.toInt();
            if (cur == *last || *attempts <= 0) { cont(); return; }
            *last = cur; --(*attempts);
            QTimer::singleShot(200, this, [poll]{ (*poll)(); });
        });
    };
    (*poll)();
}

void YandexScraper::collectHrefsStep()
{
    if (m_aborted) return;

    QWebEnginePage* page = m_searchPage;
    if (!page) { emit finished(); return; }

    waitListStable(page, [this, page]{
        if (!page) return;

        static const QString js = R"JS(
(function(){
  function clickByText(rx){
    const nodes=[].slice.call(document.querySelectorAll('button, [role="button"], a[role="button"], div[role="button"]'));
    for (let i=0;i<nodes.length;i++){
      const t=((nodes[i].innerText||nodes[i].textContent||"")+"").toLowerCase().replace(/ё/g,'е').trim();
      if (!t) continue;
      if (rx.test(t)) { const r=nodes[i].getBoundingClientRect(); if (r.width>5&&r.height>5){ nodes[i].click(); return true; } }
    }
    return false;
  }
  clickByText(/список|списком/);
  clickByText(/искать здесь/);

  function isScrollable(el){
    if (!el) return false;
    const cs=getComputedStyle(el);
    return ((cs.overflowY==='auto'||cs.overflowY==='scroll') && el.scrollHeight>el.clientHeight+20);
  }
  function listRoot(){
    const seeds=[
      document.querySelector('[data-testid="search-snippet-view"]'),
      document.querySelector('div[role="list"]'),
      document.querySelector('[class*="search-list-view"]'),
      document.querySelector('[class*="search-panel-view"]')
    ].filter(Boolean);
    for (let i=0;i<seeds.length;i++){
      let el=seeds[i];
      for (let k=0;k<6 && el; k++, el=el.parentElement) if (isScrollable(el)) return el;
    }
    const all=[].slice.call(document.querySelectorAll('div'));
    for (let i=0;i<all.length;i++){
      const el=all[i], r=el.getBoundingClientRect();
      if (r.width>260 && r.width<620 && r.height>200 && isScrollable(el)) return el;
    }
    return null;
  }
  const root = listRoot();
  const scope = root || document;

  const as=[].slice.call(scope.querySelectorAll('a[href*="/maps/org/"]'));
  const out=[], seen={};
  for (let i=0;i<as.length;i++){
    let u = as[i].href||""; if(!u) continue;
    u = u.replace(/^https:\/\/yandex\.com/,'https://yandex.ru');
    u = u.replace(/\/(gallery|reviews|related|panorama)(\/.*)?$/,'/');
    u = u.replace(/\?.*$/,'');
    const m = u.match(/^(https:\/\/yandex\.ru\/maps\/org\/[^/]+\/\d+)\/$/);
    if (!m) continue;
    u = m[1] + "/";
    if (seen[u]) continue; seen[u]=true;
    out.push(u);
  }

  function clickMore(){
    const btns=[].slice.call((root||document).querySelectorAll('button, [role="button"]'));
    for (let i=0;i<btns.length;i++){
      const t=((btns[i].innerText||btns[i].textContent||"")+"").toLowerCase().replace(/ё/g,'е').trim();
      if (!t) continue;
      if (t.includes('показать еще') || t==='еще' || t.includes('показать больше') || t.includes('дальше')) {
        const r=btns[i].getBoundingClientRect(); if (r.width>5&&r.height>5){ btns[i].click(); return true; }
      }
    }
    return false;
  }
  clickMore();

  if (root){
    root.scrollTop = root.scrollTop + 2000;
    const ev = new WheelEvent('wheel', {deltaY:2000, bubbles:true, cancelable:true});
    root.dispatchEvent(ev);
  } else {
    window.scrollBy(0, 2000);
  }

  const itemsCount = (scope.querySelectorAll('[role="listitem"], [data-testid="search-snippet-view"]')||[]).length;
  const orgCount = (document.querySelectorAll('a[href*="/maps/org/"]')||[]).length;

  return { urls: out, hasRoot: !!root, itemsCount: itemsCount, orgCount: orgCount };
})();
)JS";

        page->runJavaScript(js, [this, page](const QVariant& v){
            if (!page) return;

            const auto m   = v.toMap();
            const auto arr = m.value("urls").toList();

            const int before = m_seen.size();
            for (const auto& it : arr) m_seen.insert(it.toString());
            const int after  = m_seen.size();

            if (after == before) { ++m_idlePass; m_delay = qMin(m_delay + 150, 1200); }
            else                 { m_idlePass = 0;      m_delay = 800; }

            const int target = 60;
            const bool stop = (after >= target) || (m_pass >= m_maxPass) || (m_idlePass >= 3);

            if (stop) {
                m_hrefQueue = QStringList(m_seen.begin(), m_seen.end());
                std::sort(m_hrefQueue.begin(), m_hrefQueue.end());
                qDebug() << "[YS] hrefs total:" << m_hrefQueue.size()
                         << "hasRoot:" << m.value("hasRoot").toBool()
                         << "items:"   << m.value("itemsCount").toInt()
                         << "idle:"    << m_idlePass
                         << "pass:"    << m_pass;
                page->deleteLater();
                if (page == m_searchPage) m_searchPage = nullptr;

                for (const auto& u : m_seen) m_seenGlobal.insert(u);
                m_doneCells++;
                emit gridProgress(m_totalCells, m_doneCells);
                qDebug() << "[YS] cell done, uniq now:" << m_seenGlobal.size();
                QTimer::singleShot(200, this, &YandexScraper::nextCell);
                return;
            }

            ++m_pass;
            const int prevCount = m.value("orgCount").toInt();
            m_scrollTimer.start();
            QPointer<QWebEnginePage> wp = page;
            auto poll = QSharedPointer<std::function<void()>>::create();
            *poll = [this, wp, prevCount, poll]() mutable {
                if (m_aborted || !wp) return;
                static const QString cntJs = "document.querySelectorAll('a[href*=\"/maps/org/\"]').length;";
                wp->runJavaScript(cntJs, [this, wp, prevCount, poll](const QVariant& v) {
                    const qint64 ms = m_scrollTimer.elapsed();
                    if (!wp || v.toInt() > prevCount || ms >= 1000) {
                        qDebug() << "[YS] scroll→items:" << ms << "ms (prev" << prevCount << "→" << v.toInt() << ")";
                        if (!m_aborted) collectHrefsStep();
                    } else {
                        QTimer::singleShot(100, this, [poll]{ (*poll)(); });
                    }
                });
            };
            (*poll)();
        });
    });
}

void YandexScraper::processQueue()
{
    if (m_aborted) return;

    if (!m_parsingStarted) {
        m_stats.collectionMs = m_collectionTimer.elapsed();
        m_parsingTimer.start();
        m_parsingStarted = true;
    }

    qDebug() << "[YS] queue size:" << m_hrefQueue.size();

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
    if (href.host() == "yandex.com") href.setHost("yandex.ru");
    openCard(href);
}

void YandexScraper::openCard(const QUrl& href) {
    auto* page = new CaptchaAwarePage(m_profile, isCaptchaUrl, this);

    connect(page, &QWebEnginePage::loadProgress, this, [this,page](int p){
        qDebug() << "[VIEW]" << p << page->url();
    });
    connect(page, &QWebEnginePage::urlChanged, this, [](const QUrl& u){
        qDebug() << "[URL]" << u;
    });

    connect(page, &QWebEnginePage::loadFinished, this, [this, page, href](bool ok){
        if (!ok) {
            qDebug() << "[YS] card load failed:" << href;
            m_stats.failedCards++;
            page->deleteLater();
            QTimer::singleShot(500, this, &YandexScraper::processQueue);
            return;
        }

        auto probe = QSharedPointer<std::function<void(int)>>::create();
        *probe = [this, page, href, probe](int left){
            static const QString readyJS = R"JS(
              (function(){
                var hasTel = !!document.querySelector('a[href^="tel:"]');
                var hasName = !!document.querySelector('h1, [data-testid="business-header-title"]');
                var hasLinks = document.querySelectorAll('a[href^="http"]').length > 3;
                return hasTel || hasName || hasLinks;
              })();
            )JS";
            page->runJavaScript(readyJS, [this, page, left, probe](const QVariant& v){
                bool ready = v.toBool();
                if (ready || left<=0) {
                    static const QString js = R"JS(
                    (function(){
                      function qsa(s){ return Array.prototype.slice.call(document.querySelectorAll(s)); }

                      try {
                        qsa('button, [role="button"], a[role="button"], div[role="button"]').forEach(function(b){
                          var t=((b.innerText||b.textContent||"")+"").toLowerCase().replace(/ё/g,'е').trim();
                          if (!t) return;
                          if (t.includes("accept all")||t.includes("accept")||t.includes("agree")||
                              t.includes("принять все")||t==="принять"||t.includes("соглас")||t==="ок") {
                            try{ b.click(); }catch(e){}
                          }
                        });
                        qsa('[id*="cookie"],[class*="cookie"],[data-testid*="cookie"]').forEach(function(el){ el.style.display="none"; });
                      } catch(e){}

                      var links = qsa('a[href^="http"]').map(function(a){ return a.href; })
                        .filter(function(u){ return u && u.indexOf('yandex')===-1; });

                      function decodeTel(v){ try{ return decodeURIComponent(v); }catch(e){ return v; } }
                      function normTel(s){
                        if(!s) return "";
                        s = s.replace(/^tel:/i,"").trim();
                        s = decodeTel(s);
                        var d = s.replace(/\D+/g,"");
                        if(!d) return "";
                        if(d[0]==="8" && d.length===11) d = "7"+d.slice(1);
                        if(d[0]==="7" && d.length===11) return "+7"+d.slice(1);
                        if(s[0]==="+") return s.replace(/\s+/g," ");
                        return "";
                      }
                      var seenTel={}, tels=[];
                      qsa('a[href^="tel:"], [data-url^="tel:"], button[data-url^="tel:"], a[data-href^="tel:"], [aria-label*="Телефон"], [aria-label*="phone"]').forEach(function(el){
                        var v = el.getAttribute("href")||el.getAttribute("data-url")||el.getAttribute("data-href")||el.getAttribute("aria-label")||"";
                        var t = normTel(v); if(t && !seenTel[t]){ seenTel[t]=1; tels.push(t); }
                      });
                      var text = (document.body.innerText||document.body.textContent||"");
                      var re = /(\+7|8)\s*[\( ]?\d{3}[\) ]?\s*\d{3}[-\s]?\d{2}[-\s]?\d{2}/g, m;
                      while((m=re.exec(text))!==null){ var t=normTel(m[0]); if(t && !seenTel[t]){ seenTel[t]=1; tels.push(t); } }

                      var socialHosts=["instagram.com","t.me","telegram.me","vk.com","whatsapp.com","youtube.com","facebook.com","ok.ru","pinterest.com"];
                      var siteTld=new RegExp("\\.(ru|com|net|org|shop|store|moscow)(/|$)","i");
                      var merged=[], inst="", seen={};
                      for (var i=0;i<links.length;i++){
                        var h=links[i], key=(h||"").toLowerCase(); if(!key||seen[key]) continue; seen[key]=true;
                        var host=""; try{ host=new URL(h).hostname.toLowerCase(); }catch(e){ continue; }
                        var isSocial=false; for (var j=0;j<socialHosts.length;j++){ var s=socialHosts[j];
                          if (host.length>=s.length && host.substr(host.length-s.length)===s){ isSocial=true; break; } }
                        if (isSocial){ if(!inst && host.indexOf("instagram.com")>=0) inst=h; merged.push(h); }
                        else if (siteTld.test(h)) merged.push(h);
                      }

                      var name=""; var h1=document.querySelector('h1, [data-testid="business-header-title"]');
                      if(h1&&h1.textContent) name=h1.textContent.trim();

                      var addr=""; var a1=document.querySelector('[class*="address"], [data-type="address"], [data-testid="address"]');
                      if(a1&&a1.textContent) addr=a1.textContent.trim();

                      function getDescr(){
                        var meta=document.querySelector('meta[property="og:description"]');
                        if(meta && meta.content) return meta.content.trim();
                        var scripts=qsa('script[type="application/ld+json"]');
                        for(var i=0;i<scripts.length;i++){
                          try{
                            var o=JSON.parse(scripts[i].textContent);
                            if (o && typeof o==="object") {
                              if (o.description) return String(o.description).trim();
                              if (Array.isArray(o["@graph"])) {
                                for (var k=0;k<o["@graph"].length;k++){
                                  var g=o["@graph"][k]; if (g && g.description) return String(g.description).trim();
                                }
                              }
                            }
                          }catch(e){}
                        }
                        var cand=document.querySelector('[data-testid*="description"], [class*="business-description"], [data-action="business-description"]');
                        var t=cand&&cand.textContent?cand.textContent.trim():"";
                        return t;
                      }
                      var descr=getDescr();
                      if (/(cookie|cookies|куки|cookie policy|политик[аи] куки)/i.test(descr)) descr="";

                      return {tels:tels, merged:merged, inst:inst, name:name, addr:addr, descr:descr};
                    })();
                    )JS";
                    page->runJavaScript(js, [this, page](const QVariant& v){
                        const auto m = v.toMap();
                        QStringList sites; for (const auto& it : m.value("merged").toList()) sites << it.toString();

                        QStringList phones;
                        for (const auto& it : m.value("tels").toList())
                            phones << it.toString();

                        auto fmt = [](const QString& p)->QString{
                            QString d = p; d.remove(QRegularExpression("[^0-9+]"));
                            if (d.startsWith("+7") && d.size()==12) {
                                return QString("+7 (%1) %2-%3-%4")
                                    .arg(d.mid(2,3))
                                    .arg(d.mid(5,3))
                                    .arg(d.mid(8,2))
                                    .arg(d.mid(10,2));
                            }
                            return p;
                        };
                        for (int i=0;i<phones.size();++i) phones[i] = fmt(phones[i]);
                        phones.removeDuplicates();

                        PlaceRow r;
                        r.source  = "Яндекс.Карты";
                        r.query   = m_query + " " + m_city;
                        r.name    = m.value("name").toString();
                        r.address = m.value("addr").toString();
                        r.phone   = phones.join(" | ");
                        r.site    = sites.join(", ");
                        r.descr   = m.value("descr").toString();
                        auto [sc, why] = Rules::score(r.name, r.descr, !r.site.isEmpty(), m_scoreKeywords);
                        r.score = sc; r.why = why;

                        if (isBlocked(r)) {
                            qDebug() << "[YS] blocked:" << r.name;
                            m_stats.blockedCards++;
                            page->deleteLater();
                            QTimer::singleShot(300, this, &YandexScraper::processQueue);
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

                        QTimer::singleShot(600 + QRandomGenerator::global()->bounded(500),
                                           this, &YandexScraper::processQueue);
                    });
                } else {
                    m_stats.probeRetries++;
                    QTimer::singleShot(400, [probe, left]{ (*probe)(left-1); });
                }
            });
        };
        (*probe)(5);
    });

    m_cardTimer.start();
    qDebug() << "[YS] openCard" << href;
    emit preview(page, "Карточка: " + href.toString());
    page->load(href);
}
