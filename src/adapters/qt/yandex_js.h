#pragma once
#include <QString>

// JavaScript fragments injected into QWebEnginePage for Yandex Maps scraping.
// Shared between YandexParser (production) and fixture-based unit tests.

inline const QString kYandexJsListCount = R"JS(
  (function(){
    return (document.querySelectorAll('[role="listitem"], [data-testid="search-snippet-view"]')||[]).length;
  })();
)JS";

inline const QString kYandexJsScrollAndCollect = R"JS(
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

inline const QString kYandexJsOrgCount = QStringLiteral(
    "document.querySelectorAll('a[href*=\"/maps/org/\"]').length;"
);

inline const QString kYandexJsCardReady = R"JS(
  (function(){
    var hasTel = !!document.querySelector('a[href^="tel:"]');
    var hasName = !!document.querySelector('h1, [data-testid="business-header-title"]');
    var hasLinks = document.querySelectorAll('a[href^="http"]').length > 3;
    return hasTel || hasName || hasLinks;
  })();
)JS";

inline const QString kYandexJsExtractCard = R"JS(
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
