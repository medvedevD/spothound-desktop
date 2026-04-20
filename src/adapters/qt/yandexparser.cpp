#include "yandexparser.h"
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

// -- JavaScript fragments --

static const QString kJsCardReady = R"JS(
  (function(){
    var hasTel = !!document.querySelector('a[href^="tel:"]');
    var hasName = !!document.querySelector('h1, [data-testid="business-header-title"]');
    var hasLinks = document.querySelectorAll('a[href^="http"]').length > 3;
    return hasTel || hasName || hasLinks;
  })();
)JS";

static const QString kJsExtractCard = R"JS(
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
    page->runJavaScript(kJsCardReady, [this, wp, attemptsLeft](const QVariant& v){
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
    page->runJavaScript(kJsExtractCard, [this, wp](const QVariant& v){
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
