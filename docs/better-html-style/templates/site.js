/* better-html-style - search (the site index, a server endpoint, or the sections of the page itself), its keys, the phone layout
   of the search box, embed mode (?embed=1), ?theme= / ?lang= / ?q= in the address */
(function () {
  'use strict';
  var Q = new URLSearchParams(location.search);
  var EMBED = Q.get('embed') === '1';
  var root = document.documentElement;
  if (window.DOC_FR) { // the words of the site in the French table of base.js (the same object), applied again
    var words = { 'Search': 'Rechercher', 'Home': 'Accueil', 'Edit this page': 'Modifier cette page', 'Updated': 'Mis à jour', 'pages': 'pages',
      'Changelog': 'Historique', 'Full changelog': 'Historique complet', 'Sections': 'Sections', 'On this page': 'Sur cette page' };
    for (var w in words) if (!Object.prototype.hasOwnProperty.call(window.DOC_FR, w)) window.DOC_FR[w] = words[w];
    if (window.docI18n && root.lang === 'fr') window.docI18n.apply();
  }
  if (EMBED) {
    root.classList.add('embed');
    document.querySelectorAll('a[href]').forEach(function (a) {
      var h = a.getAttribute('href');
      if (h && !/^(#|https?:|mailto:|[\w.-]+\.(pdf|rpm|deb|apk|zip|gz|sh|json)(\?|#|$))/.test(h) && h.indexOf('embed=') < 0) a.setAttribute('href', h + (h.indexOf('?') < 0 ? '?' : '&') + 'embed=1');
    });
  }
  if (Q.get('theme') === 'dark' || Q.get('theme') === 'light') root.setAttribute('data-theme', Q.get('theme'));
  if (Q.get('lang') === 'fr' || Q.get('lang') === 'en') { try { window.docI18n.set(Q.get('lang')); } catch (e) { root.lang = Q.get('lang'); } }
  var meta = document.querySelector('meta[name="docs-search"]');
  var ENDPOINT = meta ? meta.content : '';
  function lang() { return root.lang === 'fr' ? 'fr' : 'en'; }

  // the index: the site's (search-index.js, deferred: read when searching), else the sections of this page, by language
  var own = {};
  function pageIndex() {
    var l = lang();
    if (own[l]) return own[l];
    var out = [], title = document.querySelector('main h1');
    function text(el) {
      var c = el.cloneNode(true);
      c.querySelectorAll('[data-lang]:not([data-lang="' + l + '"]), script, style, svg, .copy-btn').forEach(function (x) { x.remove(); });
      return c.textContent.replace(/\s+/g, ' ').trim();
    }
    document.querySelectorAll('main section[id]').forEach(function (s) {
      var h = s.querySelector('h2'), body = text(s), head = h ? text(h) : s.id;
      out.push({ k: 'page', t: (title ? text(title) + ' › ' : '') + head, d: body.slice(head.length, head.length + 110), u: '#' + s.id, x: body.slice(0, 600) });
      s.querySelectorAll('h3[id]').forEach(function (h3) { out.push({ k: 'page', t: head + ' › ' + text(h3), d: '', u: '#' + h3.id, x: '' }); });
    });
    own[l] = out;
    return out;
  }
  function index() { return (window.DOCS_INDEX && window.DOCS_INDEX.entries) || pageIndex(); }

  // a query is words: each one must be found (prefix 3000 > start of a word 2600 > inside 2000 > a typo 900 / 800 > letters in order);
  // the title counts fully, the description a little less, the body text least
  function dist(a, b, max) {
    if (Math.abs(a.length - b.length) > max) return max + 1;
    var prev2 = null, prev = [], cur, i, j;
    for (j = 0; j <= b.length; j++) prev[j] = j;
    for (i = 1; i <= a.length; i++) {
      cur = [i];
      for (j = 1; j <= b.length; j++) {
        cur[j] = Math.min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
        if (prev2 && i > 1 && j > 1 && a[i - 1] === b[j - 2] && a[i - 2] === b[j - 1]) cur[j] = Math.min(cur[j], prev2[j - 2] + 1);
      }
      prev2 = prev; prev = cur;
    }
    return prev[b.length];
  }
  // typo tolerance: a missing, doubled, wrong or swapped letter still finds it (a swap counts as one)
  function typo(q, s) {
    if (q.length < 4) return -1;
    var max = q.length >= 8 ? 2 : 1, words = s.split(/[^a-z0-9à-ÿ]+/), best = max + 1;
    for (var k = 0; k < words.length; k++) {
      var w = words[k];
      if (w.length < 3) continue;
      var d = Math.min(dist(q, w, max), dist(q, w.slice(0, q.length + 1), max) + 1);
      if (d < best) best = d;
    }
    return best <= max ? 900 - best * 100 : -1;
  }
  function wscore(w, s) {
    s = (s || '').toLowerCase();
    var i = s.indexOf(w);
    if (i === 0) return 3000 - s.length / 10;
    if (i > 0) return (/[^a-z0-9à-ÿ]/.test(s[i - 1]) ? 2600 : 2000) - i / 10;
    var t = typo(w, s);
    if (t >= 0) return t;
    if (w.length >= 4) { // letters in order, tight (sesend -> session_send)
      var p = 0, first = -1, last = 0;
      for (var j = 0; j < s.length && p < w.length; j++) if (s[j] === w[p]) { if (first < 0) first = j; last = j; p++; }
      if (p === w.length && last - first <= w.length * 2) return 600 - (last - first);
    }
    return -1;
  }
  function entryScore(words, e) {
    var total = 0;
    for (var n = 0; n < words.length; n++) {
      var w = words[n], best = Math.max(wscore(w, e.t), wscore(w, e.d) - 400, wscore(w, e.x) - 800);
      if (best < 0) return -1;
      total += best;
    }
    return total / words.length;
  }
  function queryWords(q) { return q.toLowerCase().split(/\s+/).filter(Boolean); }
  function esc(x) { return String(x).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function mark(q, s) {
    var out = esc(s);
    queryWords(q).forEach(function (w) {
      var re = new RegExp('(' + esc(w).replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'ig');
      out = out.replace(re, '<mark>$1</mark>');
    });
    return out;
  }
  function localSearch(q) {
    var l = lang(), words = queryWords(q);
    return index().filter(function (e) { return !e.l || e.l === l; }).map(function (e) {
      return { e: e, s: entryScore(words, e) };
    }).filter(function (x) { return x.s >= 0; }).sort(function (a, b) { return b.s - a.s; }).slice(0, 8).map(function (x) { return x.e; });
  }
  function remoteSearch(q, done) {
    fetch(ENDPOINT + '?q=' + encodeURIComponent(q) + '&lang=' + lang()).then(function (r) { return r.json(); })
      .then(function (j) { done(j.results || []); }, function () { done(localSearch(q)); });
  }
  window.docsSearch = function (q, done) { if (ENDPOINT) remoteSearch(q, done); else done(localSearch(q)); };

  function bind(input) {
    var box = input.parentNode, ul = box.querySelector('.results'), sel = 0, cur = [], seq = 0;
    function show(q, list) {
      cur = list; sel = 0;
      ul.innerHTML = cur.length ? cur.map(function (e, n) {
        var u = e.u;
        if (EMBED && u.charAt(0) !== '#') u = u.indexOf('#') >= 0 ? u.replace('#', (u.indexOf('?') < 0 ? '?' : '&') + 'embed=1#') : u + (u.indexOf('?') < 0 ? '?' : '&') + 'embed=1';
        return '<li class="' + (n ? '' : 'sel') + '"><a href="' + esc(u) + '"><span class="t">' + mark(q, e.t) + '<span class="d">' + esc(e.d || '') + '</span></span><span class="kind ' + esc(e.k) + '">' + esc(e.k) + '</span></a></li>';
      }).join('') : '<li class="none">' + (lang() === 'fr' ? 'Aucun résultat' : 'No result') + '</li>';
      ul.hidden = false;
    }
    function render() {
      var q = input.value.trim(), n = ++seq;
      if (!q) { ul.hidden = true; return; }
      window.docsSearch(q, function (list) { if (n === seq) show(q, list); });
    }
    function move(d) {
      var li = ul.querySelectorAll('li'); if (!cur.length) return;
      li[sel].classList.remove('sel'); sel = (sel + d + cur.length) % cur.length; li[sel].classList.add('sel');
      li[sel].scrollIntoView({ block: 'nearest' });
    }
    input.addEventListener('input', render);
    input.addEventListener('focus', render);
    input.addEventListener('keydown', function (ev) {
      if (ev.key === 'ArrowDown') { ev.preventDefault(); move(1); }
      else if (ev.key === 'ArrowUp') { ev.preventDefault(); move(-1); }
      else if (ev.key === 'Enter' && cur[sel]) { ev.preventDefault(); ul.querySelectorAll('a')[sel].click(); ul.hidden = true; }
      else if (ev.key === 'Escape') { ul.hidden = true; input.blur(); }
    });
    ul.addEventListener('click', function (ev) { if (ev.target.closest('a')) ul.hidden = true; });
    document.addEventListener('click', function (ev) { if (!box.contains(ev.target)) ul.hidden = true; });
  }
  document.querySelectorAll('.sbox input').forEach(bind);
  if (Q.get('q')) window.addEventListener('load', function () { var qi = document.querySelector('.sbox input'); if (qi) { qi.value = Q.get('q'); qi.dispatchEvent(new Event('input')); } }); // ?q=text opens the results

  (function () { // phone: the small search moves under the top bar (sticky), else it lives in the top bar
    var sb = document.querySelector('.topbar .sbox.small'), bar = document.querySelector('.topbar'); if (!sb || !bar) return;
    var home = sb.parentNode, mq = window.matchMedia('(max-width: 480px)');
    if (document.querySelector('.hero .sbox')) sb.classList.add('on-home'); // the home has its big search: the small one stays in the bar
    function place() {
      if (mq.matches) { sb.classList.add('mob'); bar.after(sb); }
      else { sb.classList.remove('mob'); home.insertBefore(sb, home.firstChild); }
    }
    place(); mq.addEventListener('change', place);
  })();
  document.addEventListener('keydown', function (ev) { // / or Ctrl+K: the search
    if ((ev.key === '/' || ((ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === 'k')) && !/INPUT|TEXTAREA|SELECT/.test(document.activeElement.tagName) && !document.activeElement.isContentEditable) {
      var i = document.querySelector('.hero .sbox input') || document.querySelector('.sbox input'); if (i) { ev.preventDefault(); i.focus(); i.select(); }
    }
  });
  function ph() { var fr = lang() === 'fr'; document.querySelectorAll('input[data-ph-fr]').forEach(function (i) { if (!i.dataset.ph) i.dataset.ph = i.placeholder; i.placeholder = fr ? i.dataset.phFr : i.dataset.ph; }); own = {}; }
  ph(); new MutationObserver(ph).observe(root, { attributes: true, attributeFilter: ['lang'] });
  // a host page (an app panel showing the docs in a frame) can switch the language / theme of an embedded copy
  window.addEventListener('message', function (ev) {
    var d = ev.data || {};
    if (d.docsLang && window.docI18n) window.docI18n.set(d.docsLang);
    if (d.docsTheme) root.setAttribute('data-theme', d.docsTheme);
  });
})();
