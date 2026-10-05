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
  root.lang = saved === 'fr' ? 'fr' : (root.lang || 'en');
  window.docI18n = { tr: tr, set: set, onChange: function (f) { listeners.push(f); }, lang: function () { return root.lang; } };
  document.querySelectorAll('[data-set-lang]').forEach(function (b) {
    b.addEventListener('click', function () { set(b.dataset.setLang); });
  });
  apply();
})();

// Theme: sun / moon switch, choice saved, default = system preference
(function () {
  var root = document.documentElement;
  var key = (root.getAttribute('data-storage-key') || 'page') + '-theme';
  var btn = document.getElementById('themeToggle');
  try { var saved = localStorage.getItem(key); if (saved) root.setAttribute('data-theme', saved); } catch (e) { /* storage unavailable */ }
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
