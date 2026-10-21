/* docs site - embed mode, search (in-binary endpoint or the static index), shortcuts */
(function () {
  'use strict';
  var Q = new URLSearchParams(location.search);
  var EMBED = Q.get('embed') === '1';
  var root = document.documentElement;
  if (EMBED) {
    root.classList.add('embed');
    document.querySelectorAll('a[href]').forEach(function (a) {
      var h = a.getAttribute('href');
      if (h && !/^(#|https?:|mailto:|[a-z-]+\.(pdf|rpm|deb|apk|sh|json)(\?|#|$))/.test(h) && h.indexOf('embed=') < 0) a.setAttribute('href', h + (h.indexOf('?') < 0 ? '?' : '&') + 'embed=1');
    });
  }
  if (Q.get('theme') === 'dark' || Q.get('theme') === 'light') root.setAttribute('data-theme', Q.get('theme'));
  if (Q.get('lang') === 'fr' || Q.get('lang') === 'en') { try { window.docI18n.set(Q.get('lang')); } catch (e) { root.lang = Q.get('lang'); } }
  var meta = document.querySelector('meta[name="docs-search"]');
  var ENDPOINT = meta ? meta.content : '';
  function index() { return (window.DOCS_INDEX && window.DOCS_INDEX.entries) || []; } // search-index.js is deferred: read it when searching

  function lang() { return root.lang === 'fr' ? 'fr' : 'en'; }
  // score: prefix 3000 > substring 2000 > subsequence 1000 (tighter = better), -1 = no match
  function score(q, s) {
    q = q.toLowerCase(); s = (s || '').toLowerCase();
    if (!q) return -1;
    var i = s.indexOf(q);
    if (i === 0) return 3000 - s.length;
    if (i > 0) return 2000 - i * 10 - s.length;
    var p = 0, first = -1, last = 0;
    for (var j = 0; j < s.length && p < q.length; j++) if (s[j] === q[p]) { if (first < 0) first = j; last = j; p++; }
    if (p === q.length) return 1000 - (last - first);
    return typo(q, s);
  }
  // typo tolerance (the idea of c2dmp-hsm: a missing, doubled, wrong or swapped letter still finds it): a word of the text
  // within 1 edit of the query (2 from 8 letters), swaps counted as one
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
  function typo(q, s) {
    if (q.length < 4) return -1;
    var max = q.length >= 8 ? 2 : 1, words = s.split(/[^a-z0-9_]+/), best = max + 1;
    for (var k = 0; k < words.length; k++) {
      var w = words[k];
      if (w.length < 3) continue;
      var d = Math.min(dist(q, w, max), dist(q, w.slice(0, q.length + 1), max) + 1);
      if (d < best) best = d;
    }
    return best <= max ? 900 - best * 100 : -1;
  }
  function esc(x) { return String(x).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function mark(q, s) {
    var l = s.toLowerCase(), ql = q.toLowerCase(), i = l.indexOf(ql), out = '', p = 0;
    if (i >= 0) return esc(s.slice(0, i)) + '<mark>' + esc(s.slice(i, i + q.length)) + '</mark>' + esc(s.slice(i + q.length));
    for (var j = 0; j < s.length; j++) {
      if (p < ql.length && l[j] === ql[p]) { out += '<mark>' + esc(s[j]) + '</mark>'; p++; } else out += esc(s[j]);
    }
    return out;
  }
  function localSearch(q) {
    var l = lang();
    return index().filter(function (e) { return !e.l || e.l === l; }).map(function (e) {
      return { e: e, s: Math.max(score(q, e.t), score(q, e.d) - 500, score(q, e.x) - 1000) };
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
        var u = e.u + (EMBED ? (e.u.indexOf('?') < 0 ? '?' : '&') + 'embed=1' : '');
        if (EMBED && e.u.indexOf('#') >= 0) u = e.u.replace('#', '?embed=1#');
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
      else if (ev.key === 'Enter' && cur[sel]) location.href = ul.querySelectorAll('a')[sel].getAttribute('href');
      else if (ev.key === 'Escape') { ul.hidden = true; input.blur(); }
    });
    document.addEventListener('click', function (ev) { if (!box.contains(ev.target)) ul.hidden = true; });
  }
  document.querySelectorAll('.sbox input').forEach(bind);
  if (Q.get('q')) window.addEventListener('load', function () { var qi = document.querySelector('.sbox input'); if (qi) { qi.value = Q.get('q'); qi.dispatchEvent(new Event('input')); } }); // ?q=text opens the results

  (function () { // phone: the small search moves under the top bar (sticky), else it lives in the top bar
    var sb = document.querySelector('.sbox.small'); if (!sb) return;
    var home = sb.parentNode, mq = window.matchMedia('(max-width: 480px)');
    function place() {
      if (mq.matches) { sb.classList.add('mob'); document.querySelector('.topbar').after(sb); }
      else { sb.classList.remove('mob'); home.insertBefore(sb, home.firstChild); }
    }
    place(); mq.addEventListener('change', place);
  })();
  document.addEventListener('keydown', function (ev) {
    if ((ev.key === '/' || (ev.ctrlKey && ev.key.toLowerCase() === 'k')) && !/INPUT|TEXTAREA/.test(document.activeElement.tagName)) {
      var i = document.querySelector('.sbox input'); if (i) { ev.preventDefault(); i.focus(); }
    }
  });
  function ph() { var fr = lang() === 'fr'; document.querySelectorAll('input[data-ph-fr]').forEach(function (i) { if (!i.dataset.ph) i.dataset.ph = i.placeholder; i.placeholder = fr ? i.dataset.phFr : i.dataset.ph; }); }
  ph(); new MutationObserver(ph).observe(root, { attributes: true, attributeFilter: ['lang'] });
  // the host page (web panel) can switch the language / theme of an embedded copy
  window.addEventListener('message', function (ev) {
    var d = ev.data || {};
    if (d.docsLang && window.docI18n) window.docI18n.set(d.docsLang);
    if (d.docsTheme) root.setAttribute('data-theme', d.docsTheme);
  });
})();
