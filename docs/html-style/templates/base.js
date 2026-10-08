// Behaviour shared by the user's HTML pages (from html-doc): theme switch, optional EN / FR switch, copy buttons
// on the code blocks, heading anchors, content tabs, mobile menu. Inlined in each page by scripts/new_page.py.

// Language (only when the page has a .lang-switch): [data-i18n] elements and [data-i18n-<attr>] attributes are
// translated with window.DOC_FR (English text -> French text), [data-lang] blocks are shown for their language only.
(function () {
  var root = document.documentElement;
  var key = (root.getAttribute('data-storage-key') || 'page') + '-lang';
  var FR = window.DOC_FR || {};
  var listeners = [];
  function tr(s) { return root.lang === 'fr' && Object.prototype.hasOwnProperty.call(FR, s) ? FR[s] : s; }
  function apply() {
    document.querySelectorAll('[data-i18n]').forEach(function (el) {
      if (el.dataset.i18nEn === undefined) el.dataset.i18nEn = el.innerHTML.trim();
      el.innerHTML = tr(el.dataset.i18nEn);
    });
    ['aria-label', 'title', 'placeholder'].forEach(function (attr) {
      document.querySelectorAll('[data-i18n-' + attr + ']').forEach(function (el) {
        var store = 'i18nEn' + attr.replace(/-(\w)/g, function (m, c) { return c.toUpperCase(); });
        if (el.dataset[store] === undefined) el.dataset[store] = el.getAttribute(attr) || '';
        el.setAttribute(attr, tr(el.dataset[store]));
      });
    });
    document.querySelectorAll('[data-set-lang]').forEach(function (b) {
      b.classList.toggle('on', b.dataset.setLang === root.lang);
      b.setAttribute('aria-pressed', b.dataset.setLang === root.lang ? 'true' : 'false');
    });
  }
  function set(lang) {
    root.lang = lang === 'fr' ? 'fr' : 'en';
    try { localStorage.setItem(key, root.lang); } catch (e) { /* storage unavailable */ }
    apply();
    listeners.forEach(function (f) { f(root.lang); });
  }
  var saved = null;
  if (document.querySelector('.lang-switch')) {
    try { saved = localStorage.getItem(key); } catch (e) { /* storage unavailable */ }
  }
  // Choice carried by a link of the site (?lang=): pages opened from file:// don't share their storage
  var askedLang = new URLSearchParams(location.search).get('lang');
  if (askedLang === 'en' || askedLang === 'fr') { saved = askedLang; try { localStorage.setItem(key, askedLang); } catch (e) { /* storage unavailable */ } }
  root.lang = saved === 'fr' ? 'fr' : (root.lang || 'en');
  window.docI18n = { tr: tr, set: set, apply: apply, onChange: function (f) { listeners.push(f); }, lang: function () { return root.lang; } };
  document.querySelectorAll('[data-set-lang]').forEach(function (b) {
    b.addEventListener('click', function () { set(b.dataset.setLang); });
  });
  apply();
})();

// Links between the pages of the site carry the language, theme and level (?lang=fr&theme=dark&level=technical):
// the choice applies to every page, also from file:// where each page has its own storage
(function () {
  var root = document.documentElement;
  function carry(e) {
    var a = e.target.closest && e.target.closest('a[href]');
    if (!a || a.target === '_blank') return;
    var href = a.getAttribute('href');
    if (!/^[^:?#\/]+\.html(#.*)?$/.test(href) && !/^\.\/[^:?#\/]+\.html(#.*)?$/.test(href)) return; // pages of the same folder only
    var u = new URL(a.href);
    u.searchParams.set('lang', root.lang || 'en');
    if (root.getAttribute('data-theme')) u.searchParams.set('theme', root.getAttribute('data-theme'));
    if (root.getAttribute('data-level')) u.searchParams.set('level', root.getAttribute('data-level'));
    a.href = u.href;
  }
  document.addEventListener('click', carry, true);
  document.addEventListener('auxclick', carry, true);
})();

// Theme: sun / moon switch, choice saved, default = system preference
(function () {
  var root = document.documentElement;
  var key = (root.getAttribute('data-storage-key') || 'page') + '-theme';
  var btn = document.getElementById('themeToggle');
  try { var saved = localStorage.getItem(key); if (saved) root.setAttribute('data-theme', saved); } catch (e) { /* storage unavailable */ }
  var askedTheme = new URLSearchParams(location.search).get('theme'); // carried by a link of the site
  if (askedTheme === 'dark' || askedTheme === 'light') { root.setAttribute('data-theme', askedTheme); try { localStorage.setItem(key, askedTheme); } catch (e) { /* storage unavailable */ } }
  function isDark() {
    return root.getAttribute('data-theme') === 'dark' ||
      (!root.getAttribute('data-theme') && window.matchMedia('(prefers-color-scheme: dark)').matches);
  }
  if (!btn) return;
  btn.setAttribute('aria-checked', isDark() ? 'true' : 'false');
  btn.addEventListener('click', function () {
    var next = isDark() ? 'light' : 'dark';
    root.setAttribute('data-theme', next);
    btn.setAttribute('aria-checked', next === 'dark' ? 'true' : 'false');
    try { localStorage.setItem(key, next); } catch (e) { /* storage unavailable */ }
    document.dispatchEvent(new CustomEvent('themechange', { detail: next }));
  });
})();

// Figures: an SVG is never drawn larger than its viewBox (its text would grow with the screen)
document.querySelectorAll('figure svg[viewBox]').forEach(function (svg) {
  var w = +svg.getAttribute('viewBox').split(/[\s,]+/)[2];
  if (w && !svg.style.maxWidth) { svg.style.maxWidth = w + 'px'; svg.style.marginInline = 'auto'; }
});

// Diagram links: an arrow is computed from the shapes it joins instead of typed coordinates, so it always touches
// them. <path class="edge" data-from="a" data-to="b" marker-end="url(#arr)"/>: a / b = id of a rect, circle, ellipse
// (or a <g> holding one). Options: data-from="a:right" (side: top, bottom, left, right), data-bend="0.2" (curve, +/-
// fraction of the length), data-gap="2" (space before the shape). A label <text data-label-for="pathId"
// data-offset="-8"> is placed on the middle of the path. Geometry is read from the attributes (works while hidden).
window.docDiagram = (function () {
  function translateOf(el, svg) {
    var x = 0, y = 0;
    for (var n = el; n && n !== svg; n = n.parentNode) {
      var t = n.getAttribute && n.getAttribute('transform');
      var m = t && t.match(/translate\(\s*(-?[\d.]+)[\s,]*(-?[\d.]+)?\s*\)/);
      if (m) { x += +m[1]; y += +(m[2] || 0); }
    }
    return [x, y];
  }
  function shape(svg, id) {
    var el = svg.querySelector('[id="' + id + '"]');
    if (el && el.tagName.toLowerCase() === 'g') el = el.querySelector('rect, circle, ellipse');
    if (!el) return null;
    var o = translateOf(el, svg), num = function (a) { return +(el.getAttribute(a) || 0); };
    var tag = el.tagName.toLowerCase();
    if (tag === 'rect') return { kind: 'rect', x: num('x') + o[0], y: num('y') + o[1], w: num('width'), h: num('height') };
    if (tag === 'circle') return { kind: 'ellipse', cx: num('cx') + o[0], cy: num('cy') + o[1], rx: num('r'), ry: num('r') };
    return { kind: 'ellipse', cx: num('cx') + o[0], cy: num('cy') + o[1], rx: num('rx'), ry: num('ry') };
  }
  function center(s) { return s.kind === 'rect' ? [s.x + s.w / 2, s.y + s.h / 2] : [s.cx, s.cy]; }
  function side(s, name) {
    var c = center(s), hw = s.kind === 'rect' ? s.w / 2 : s.rx, hh = s.kind === 'rect' ? s.h / 2 : s.ry;
    return { top: [c[0], c[1] - hh], bottom: [c[0], c[1] + hh], left: [c[0] - hw, c[1]], right: [c[0] + hw, c[1]] }[name];
  }
  // Point of the border of s in the direction of p, moved back by gap
  function border(s, p, gap) {
    var c = center(s), dx = p[0] - c[0], dy = p[1] - c[1], len = Math.hypot(dx, dy) || 1, k;
    if (s.kind === 'rect') k = Math.min(dx ? (s.w / 2) / Math.abs(dx) : Infinity, dy ? (s.h / 2) / Math.abs(dy) : Infinity);
    else k = 1 / Math.sqrt((dx * dx) / (s.rx * s.rx) + (dy * dy) / (s.ry * s.ry));
    return [c[0] + dx * k + dx / len * gap, c[1] + dy * k + dy / len * gap];
  }
  // Point of the border of s for an arrow coming from / going to toward; off spreads several arrows of the same
  // shape along its side (instead of all reaching the same point)
  function end(s, spec, toward, gap, off) {
    var name = spec.split(':')[1], p;
    if (name) p = side(s, name);
    else if (off && s.kind === 'rect') {
        var c = center(s), dx = toward[0] - c[0], dy = toward[1] - c[1];
        if (Math.abs(dy) * s.w >= Math.abs(dx) * s.h) p = [c[0] + off * s.w, c[1] + (dy < 0 ? -1 : 1) * s.h / 2];
        else p = [c[0] + (dx < 0 ? -1 : 1) * s.w / 2, c[1] + off * s.h];
    } else p = border(s, toward, 0);
    var vx = toward[0] - p[0], vy = toward[1] - p[1], len = Math.hypot(vx, vy) || 1;
    return [p[0] + vx / len * gap, p[1] + vy / len * gap];
  }
  // Slot (-0.3 .. 0.3 of the side) of each arrow end sharing a shape, ordered by where the other end is
  function slots(items) {
    var groups = {};
    items.forEach(function (it) { if (!it.spec.split(':')[1]) (groups[it.id] = groups[it.id] || []).push(it); });
    Object.keys(groups).forEach(function (id) {
      var g = groups[id], s = g[0].shape, c = center(s);
      if (g.length < 2) return;
      g.forEach(function (it) {
        var dx = it.toward[0] - c[0], dy = it.toward[1] - c[1];
        it.vertical = s.kind === 'rect' && Math.abs(dy) * s.w >= Math.abs(dx) * s.h;
        it.key = it.vertical ? dx : dy;
      });
      [true, false].forEach(function (v) {
        var sub = g.filter(function (it) { return it.vertical === v; }).sort(function (a, b) { return a.key - b.key; });
        if (sub.length > 1) sub.forEach(function (it, i) { it.off = ((i + 1) / (sub.length + 1) - 0.5) * 0.6; });
      });
    });
  }
  function layout(svg) {
    var items = [], links = [];
    svg.querySelectorAll('[data-from][data-to]').forEach(function (path) {
      var fa = path.dataset.from, ta = path.dataset.to;
      var a = shape(svg, fa.split(':')[0]), b = shape(svg, ta.split(':')[0]);
      if (!a || !b) { console.warn('diagram link: unknown shape', fa, ta); return; }
      var gap = +(path.dataset.gap || 2), bend = +(path.dataset.bend || 0);
      var ca = fa.indexOf(':') > 0 ? side(a, fa.split(':')[1]) : center(a);
      var cb = ta.indexOf(':') > 0 ? side(b, ta.split(':')[1]) : center(b);
      var mx = (ca[0] + cb[0]) / 2, my = (ca[1] + cb[1]) / 2, len = Math.hypot(cb[0] - ca[0], cb[1] - ca[1]) || 1;
      var ctrl = [mx - (cb[1] - ca[1]) / len * bend * len, my + (cb[0] - ca[0]) / len * bend * len];
      var ea = { spec: fa, id: fa.split(':')[0], shape: a, toward: bend ? ctrl : cb, off: 0 };
      var eb = { spec: ta, id: ta.split(':')[0], shape: b, toward: bend ? ctrl : ca, off: 0 };
      items.push(ea, eb);
      links.push({ path: path, gap: gap, bend: bend, ctrl: ctrl, ea: ea, eb: eb });
    });
    slots(items);
    links.forEach(function (l) {
      var path = l.path, gap = l.gap, bend = l.bend, ctrl = l.ctrl;
      var p1 = end(l.ea.shape, l.ea.spec, l.ea.toward, gap, l.ea.off), p2 = end(l.eb.shape, l.eb.spec, l.eb.toward, gap, l.eb.off);
      var f = function (p) { return p[0].toFixed(1) + ' ' + p[1].toFixed(1); };
      path.setAttribute('d', bend ? 'M' + f(p1) + ' Q' + f(ctrl) + ' ' + f(p2) : 'M' + f(p1) + ' L' + f(p2));
      var mid = bend ? [(p1[0] + 2 * ctrl[0] + p2[0]) / 4, (p1[1] + 2 * ctrl[1] + p2[1]) / 4] : [(p1[0] + p2[0]) / 2, (p1[1] + p2[1]) / 2];
      if (path.id) svg.querySelectorAll('[data-label-for="' + path.id + '"]').forEach(function (t) {
        var off = +(t.dataset.offset || -8), dx = p2[0] - p1[0], dy = p2[1] - p1[1], l = Math.hypot(dx, dy) || 1;
        var nx = -dy / l, ny = dx / l;
        if (ny > 0) { nx = -nx; ny = -ny; } // a negative offset is always above the path
        t.setAttribute('x', (mid[0] - nx * off).toFixed(1));
        t.setAttribute('y', (mid[1] - ny * off).toFixed(1));
        if (!t.getAttribute('text-anchor')) t.setAttribute('text-anchor', 'middle');
      });
    });
  }
  document.querySelectorAll('svg').forEach(layout);
  return { layout: layout };
})();

// Sidebar (mobile) + visible section, copy buttons, heading anchors, content tabs
(function () {
  var tr = function (s) { return window.docI18n ? window.docI18n.tr(s) : s; };
  var menu = document.getElementById('menuBtn'), side = document.getElementById('sidebar');
  if (menu && side) {
    menu.addEventListener('click', function () { side.classList.toggle('open'); });
    document.addEventListener('click', function (e) { if (!side.contains(e.target) && !menu.contains(e.target)) side.classList.remove('open'); });
  }
  var links = document.querySelectorAll('.sidebar a[href^="#"]');
  if (links.length && 'IntersectionObserver' in window) {
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        links.forEach(function (a) { a.classList.toggle('active', a.getAttribute('href') === '#' + entry.target.id); });
      });
    }, { rootMargin: '-20% 0px -70% 0px' });
    document.querySelectorAll('main section[id]').forEach(function (s) { observer.observe(s); });
  }
  document.querySelectorAll('main pre').forEach(function (pre) {
    if (pre.dataset.nocopy !== undefined) return;
    var b = document.createElement('button');
    b.type = 'button'; b.className = 'copy'; b.textContent = tr('Copy');
    b.addEventListener('click', function () {
      var code = pre.querySelector('code') || pre;
      var done = function () { b.textContent = tr('Copied'); setTimeout(function () { b.textContent = tr('Copy'); }, 1400); };
      if (navigator.clipboard) navigator.clipboard.writeText(code.innerText).then(done, function () { b.textContent = tr('Select & copy'); });
      else b.textContent = tr('Select & copy');
    });
    pre.appendChild(b);
  });
  document.querySelectorAll('main h2[id], main h3[id], main section[id] > h2').forEach(function (h) {
    var id = h.id || h.parentElement.id;
    if (!id || h.querySelector('.anchor')) return;
    var a = document.createElement('a');
    a.className = 'anchor'; a.href = '#' + id; a.textContent = '#';
    a.setAttribute('aria-label', tr('Link to this section'));
    h.appendChild(a);
  });
  document.querySelectorAll('.ctabs').forEach(function (bar) {
    var group = bar.dataset.tabs;
    var buttons = bar.querySelectorAll('button');
    var panels = document.querySelectorAll('.ctab-panel[data-tabs="' + group + '"]');
    function show(name) {
      buttons.forEach(function (b) { b.classList.toggle('active', b.dataset.tab === name); });
      panels.forEach(function (p) { p.classList.toggle('active', p.dataset.tab === name); });
    }
    buttons.forEach(function (b) { b.addEventListener('click', function () { show(b.dataset.tab); }); });
    if (buttons.length) show(buttons[0].dataset.tab);
  });
  if (window.docI18n) window.docI18n.onChange(function () {
    document.querySelectorAll('main pre .copy').forEach(function (b) { b.textContent = tr('Copy'); });
    document.querySelectorAll('.anchor').forEach(function (a) { a.setAttribute('aria-label', tr('Link to this section')); });
  });
})();
