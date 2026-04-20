#include "yandexcollector.h"
#include "captchaawarepage.h"
#include "core/geo_grid.h"

#include <QPointer>
#include <QTimer>
#include <QUrlQuery>

// -- JavaScript fragments --

static const QString kJsListCount = R"JS(
  (function(){
    return (document.querySelectorAll('[role="listitem"], [data-testid="search-snippet-view"]')||[]).length;
  })();
)JS";

static const QString kJsScrollAndCollect = R"JS(
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

static const QString kJsOrgCount = QStringLiteral(
    "document.querySelectorAll('a[href*=\"/maps/org/\"]').length;"
);

// -- Timing constants (ms) --
static constexpr int kAfterSearchLoadDelayMs = 500;
static constexpr int kAfterCaptchaDelayMs    = 1200;
static constexpr int kCellStartDelayMs       = 700;
static constexpr int kCellDoneDelayMs        = 200;
static constexpr int kScrollPollIntervalMs   = 100;
static constexpr int kScrollTimeoutMs        = 1000;
static constexpr int kListStablePollMs       = 200;

// -- Scroll strategy limits --
static constexpr int kMaxScrollPasses    = 12;
static constexpr int kMaxIdlePasses      = 3;
static constexpr int kHrefTargetPerCell  = 60;
static constexpr int kListStableAttempts = 4;

static inline bool isCaptchaUrl(const QUrl& u) {
    if (!u.host().contains("yandex")) return false;
    const QString p = u.path();
    return p.contains("showcaptcha") || p.contains("captchapgrd") || p.contains("captcha");
}

YandexCollector::YandexCollector(QString query, QString city,
                                 QWebEngineProfile* profile,
                                 int gridN,
                                 core::ScrapeStats& stats,
                                 QObject* parent)
    : QObject(parent)
    , m_query(std::move(query))
    , m_city(std::move(city))
    , m_profile(profile)
    , m_gridN(qBound(2, gridN, 10))
    , m_stats(stats)
{}

void YandexCollector::start()
{
    m_aborted = false;
    m_grid    = {};
    m_scroll  = {};
    buildCells();
    m_grid.totalCells   = m_grid.cells.size();
    m_stats.gridCells   = m_grid.totalCells;
    emit gridProgress(m_grid.totalCells, 0);
    nextCell();
}

void YandexCollector::abort()
{
    m_aborted = true;
    if (m_scroll.page) { m_scroll.page->deleteLater(); m_scroll.page = nullptr; }
    m_grid   = {};
    m_scroll = {};
}

void YandexCollector::buildCells()
{
    const core::CityGeo* c = core::CityRegistry::find(m_city.toStdString());
    const core::GeoGrid  g = core::buildGrid(c, m_gridN);

    m_grid.cellZoom   = g.zoom;
    m_grid.yandexRid  = g.yandexRid;
    m_grid.yandexSlug = QString::fromStdString(g.yandexSlug);
    m_grid.cells.reserve(g.cells.size());
    for (const auto& p : g.cells)
        m_grid.cells.push_back(QPointF(p.lon, p.lat));
}

void YandexCollector::nextCell()
{
    if (m_grid.cellIdx >= m_grid.cells.size()) {
        QStringList all(m_scroll.seenGlobal.begin(), m_scroll.seenGlobal.end());
        std::sort(all.begin(), all.end());
        qDebug() << "[YC] total hrefs:" << all.size();
        emit hrefsReady(std::move(all));
        return;
    }

    qDebug() << QString("[YC] cell %1/%2").arg(m_grid.cellIdx).arg(m_grid.cells.size());

    const auto ll = m_grid.cells[m_grid.cellIdx++];
    QUrl u; u.setScheme("https"); u.setHost("yandex.ru");
    u.setPath(QString("/maps/%1/%2/search/%3/")
              .arg(m_grid.yandexRid).arg(m_grid.yandexSlug).arg(m_query));
    QUrlQuery q;
    q.addQueryItem("ll", QString("%1,%2").arg(ll.x(), 0, 'f', 6).arg(ll.y(), 0, 'f', 6));
    q.addQueryItem("z",  QString::number(m_grid.cellZoom));
    q.addQueryItem("mode", "search");
    u.setQuery(q);

    m_scroll.searchUrl = u;
    m_scroll.seen.clear(); m_scroll.pass = 0; m_scroll.idlePass = 0; m_scroll.forcedRu = false;

    openSearch();
}

void YandexCollector::openSearch()
{
    m_scroll.page = new CaptchaAwarePage(m_profile, isCaptchaUrl, this);

    connect(m_scroll.page, &QWebEnginePage::loadFinished, this, [this](bool ok){
        qDebug() << "[YC] search loadFinished:" << ok << m_scroll.page->url();
        if (!ok) { emit error(); return; }

        if (!m_scroll.forcedRu && m_scroll.page->url().host() != "yandex.ru") {
            m_scroll.forcedRu = true;
            m_scroll.page->load(m_scroll.searchUrl);
            return;
        }

        if (isCaptchaUrl(m_scroll.page->url())) {
            QMetaObject::Connection* conn = new QMetaObject::Connection;
            *conn = connect(m_scroll.page, &QWebEnginePage::urlChanged, this, [this, conn](const QUrl& u){
                if (!isCaptchaUrl(u)) {
                    QObject::disconnect(*conn); delete conn;
                    m_scroll.page->load(m_scroll.searchUrl);
                    QTimer::singleShot(kAfterCaptchaDelayMs, this, &YandexCollector::collectHrefs);
                }
            });
            return;
        }

        m_scroll.page->runJavaScript("window.scrollBy(0,1200);", [](const QVariant&){});
        QTimer::singleShot(kAfterSearchLoadDelayMs, this, &YandexCollector::collectHrefs);
    });

    qDebug() << "[YC] url=" << m_scroll.searchUrl;
    emit preview(m_scroll.page, "Поиск: " + m_query + " / " + m_city);
    m_scroll.page->load(m_scroll.searchUrl);
}

void YandexCollector::collectHrefs()
{
    QTimer::singleShot(kCellStartDelayMs, this, &YandexCollector::collectHrefsStep);
}

void YandexCollector::waitListStable(QWebEnginePage* page, std::function<void()> cont)
{
    QPointer<QWebEnginePage> p = page;
    auto attempts = QSharedPointer<int>::create(kListStableAttempts);
    auto last     = QSharedPointer<int>::create(-1);

    auto poll = QSharedPointer<std::function<void()>>::create();
    *poll = [this, p, attempts, last, cont, poll]() mutable {
        if (!p) return;
        p->runJavaScript(kJsListCount, [this, p, attempts, last, cont, poll](const QVariant& v){
            if (!p) return;
            const int cur = v.toInt();
            if (cur == *last || *attempts <= 0) { cont(); return; }
            *last = cur; --(*attempts);
            QTimer::singleShot(kListStablePollMs, this, [poll]{ (*poll)(); });
        });
    };
    (*poll)();
}

void YandexCollector::waitForNewOrgs(QWebEnginePage* page, int prevCount)
{
    QPointer<QWebEnginePage> wp = page;
    wp->runJavaScript(kJsOrgCount, [this, wp, prevCount](const QVariant& v) {
        const qint64 ms = m_scrollTimer.elapsed();
        if (!wp || v.toInt() > prevCount || ms >= kScrollTimeoutMs) {
            qDebug() << "[YC] scroll→items:" << ms << "ms (prev" << prevCount << "→" << v.toInt() << ")";
            if (!m_aborted) collectHrefsStep();
        } else {
            QTimer::singleShot(kScrollPollIntervalMs, this, [this, wp, prevCount]{
                if (!m_aborted && wp) waitForNewOrgs(wp, prevCount);
            });
        }
    });
}

void YandexCollector::collectHrefsStep()
{
    if (m_aborted) return;

    QWebEnginePage* page = m_scroll.page;
    if (!page) { emit error(); return; }

    waitListStable(page, [this, page]{
        if (!page) return;

        page->runJavaScript(kJsScrollAndCollect, [this, page](const QVariant& v){
            if (!page) return;

            const auto m   = v.toMap();
            const auto arr = m.value("urls").toList();

            const int before = m_scroll.seen.size();
            for (const auto& it : arr) m_scroll.seen.insert(it.toString());
            const int after  = m_scroll.seen.size();

            if (after == before) ++m_scroll.idlePass;
            else                  m_scroll.idlePass = 0;

            const bool stop = (after >= kHrefTargetPerCell)
                           || (m_scroll.pass >= kMaxScrollPasses)
                           || (m_scroll.idlePass >= kMaxIdlePasses);

            if (stop) {
                qDebug() << "[YC] hrefs cell:" << m_scroll.seen.size()
                         << "hasRoot:" << m.value("hasRoot").toBool()
                         << "items:"   << m.value("itemsCount").toInt()
                         << "idle:"    << m_scroll.idlePass
                         << "pass:"    << m_scroll.pass;
                page->deleteLater();
                if (page == m_scroll.page) m_scroll.page = nullptr;

                for (const auto& u : m_scroll.seen) m_scroll.seenGlobal.insert(u);
                m_grid.doneCells++;
                emit gridProgress(m_grid.totalCells, m_grid.doneCells);
                qDebug() << "[YC] cell done, uniq now:" << m_scroll.seenGlobal.size();
                QTimer::singleShot(kCellDoneDelayMs, this, &YandexCollector::nextCell);
                return;
            }

            ++m_scroll.pass;
            m_scrollTimer.start();
            waitForNewOrgs(page, m.value("orgCount").toInt());
        });
    });
}
