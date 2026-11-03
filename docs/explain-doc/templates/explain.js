// Explanation pages (explain-doc): level switch, steppers, playgrounds, glossary terms.
// API for the page scripts: window.explain.play(name, fn), explain.level(), explain.onLevel(fn), explain.fmt(x, d).
(function () {
  var root = document.documentElement;
  var key = (root.getAttribute('data-storage-key') || 'page') + '-level';
  var levelListeners = [];

  // ---------- Level: ?level= in the address > saved choice > data-default-level > simple ----------
  function setLevel(level, save) {
    level = level === 'technical' ? 'technical' : 'simple';
    root.setAttribute('data-level', level);
    document.querySelectorAll('[data-set-level]').forEach(function (b) {
      b.classList.toggle('on', b.dataset.setLevel === level);
      b.setAttribute('aria-pressed', b.dataset.setLevel === level ? 'true' : 'false');
    });
    if (save) { try { localStorage.setItem(key, level); } catch (e) { /* storage unavailable */ } }
    levelListeners.forEach(function (f) { f(level); });
  }
  var asked = new URLSearchParams(location.search).get('level'), saved = null;
  try { saved = localStorage.getItem(key); } catch (e) { /* storage unavailable */ }
  setLevel(asked || saved || root.getAttribute('data-default-level') || 'simple', false);
  document.querySelectorAll('[data-set-level]').forEach(function (b) {
    b.addEventListener('click', function () { setLevel(b.dataset.setLevel, true); });
  });

  // ---------- Blocks that scroll: drawings and formulas ----------
  // Under 860px a drawing scrolls in a box of its own instead of shrinking until its labels cannot be read: its smallest size is
  // 560px, never above its viewBox (a small drawing is not blown up).
  var DRAWN_MIN = 560;
  function drawingBox(svg) {
    var view = document.createElement('div');
    view.className = 'view';
    svg.replaceWith(view);
    view.appendChild(svg);
    var width = svg.viewBox && svg.viewBox.baseVal ? svg.viewBox.baseVal.width : 0;
    if (width) svg.style.setProperty('--drawn-min', Math.min(DRAWN_MIN, width) + 'px');
    return view;
  }
  // A block that really scrolls is reached and scrolled with the keyboard (RGAA 7.3), one that fits is not a stop of the tab order
  var SCROLL_BOXES = '.formula, .stepper .view, figure > .view';
  function reachable() {
    document.querySelectorAll(SCROLL_BOXES).forEach(function (box) {
      if (!box.clientWidth) return; // hidden (the other level, a closed tab): done when it is shown
      var scrolls = box.scrollWidth > box.clientWidth + 1;
      if (scrolls && !box.hasAttribute('tabindex')) {
        var label = box.querySelector('svg[aria-label]');
        box.setAttribute('tabindex', '0');
        box.setAttribute('role', 'region');
        box.setAttribute('aria-label', label ? label.getAttribute('aria-label') : tr(box.classList.contains('formula') ? 'Formula' : 'Drawing'));
      } else if (!scrolls && box.hasAttribute('tabindex')) {
        box.removeAttribute('tabindex');
        box.removeAttribute('role');
        box.removeAttribute('aria-label');
      }
    });
  }

  // ---------- Steppers: <figure class="stepper" data-steps="N"> ----------
  // In the SVG:
  // - data-show="2" (only step 2), "2-" (from 2), "1-3", "1,3": visible at those steps (short fade in / out);
  // - data-hl / data-warn="3,5": class hl / hl-warn (accent / warn highlight) at those steps;
  // - data-at="0:0,0;3:85,0": offset (x,y in SVG units) from the drawn position, the last entry <= step applies.
  //   A short move slides; a long one (> 2 times the element size, a wrap-around) fades out and back in at the new
  //   place instead of crossing the drawing; data-fade forces the fade, data-slide forces the slide;
  // - data-text="0:19;2:d4": text per step (same rule), a change cross-fades then pulses (class changed).
  // Captions: <ol class="captions"><li> per step (first = step 0), shown above the drawing with the controls.
  function inRange(spec, step) {
    return spec.split(',').some(function (part) {
      var m = part.trim().match(/^(\d+)(-)?(\d+)?$/);
      if (!m) return false;
      var a = +m[1], b = m[2] ? (m[3] !== undefined ? +m[3] : Infinity) : a;
      return step >= a && step <= b;
    });
  }
  function valueAt(spec, step) {
    var best = null, at = -1;
    spec.split(';').forEach(function (part) {
      var i = part.indexOf(':');
      if (i < 0) return;
      var k = +part.slice(0, i).trim();
      if (k <= step && k > at) { at = k; best = part.slice(i + 1); }
    });
    return best;
  }
  // French of the UI of the explanation pages (merged into the dictionary of the page)
  var FR = { "Level": "Niveau", "Simple": "Simple", "Technical": "Technique", "Explanation": "Explication", "Play": "Lecture",
    "Pause": "Pause", "First step": "Première étape", "Previous step (←)": "Étape précédente (←)", "Next step (→)": "Étape suivante (→)",
    "Previous step": "Étape précédente", "Next step": "Étape suivante", "Contents": "Sommaire", "Formula": "Formule", "Drawing": "Dessin" };
  window.DOC_FR = window.DOC_FR || {};
  Object.keys(FR).forEach(function (k) { if (!(k in window.DOC_FR)) window.DOC_FR[k] = FR[k]; });
  var tr = function (s) { return window.docI18n ? window.docI18n.tr(s) : s; };
  var reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  var FADE = 160; // ms of each half of a fade (out, then in)
  var TALL = 480; // a drawing higher than that is not fitted in the 400px of a stepper: its labels could not be read
  var HEAD_MAX = 0.45; // part of the window a head that follows the scroll may hide
  // Icons of the controls: same 16x16 grid, filled with currentColor
  var svg16 = function (d) { return '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="' + d + '"/></svg>'; };
  var ICON = {
    first: svg16('M3 3h2v10H3zM13 3v10L6 8z'),
    prev: svg16('M11.5 3v10L4.5 8z'),
    play: svg16('M4.5 3v10l8-5z'),
    pause: svg16('M4 3h3v10H4zM9 3h3v10H9z'),
    next: svg16('M4.5 3v10l7-5z')
  };
  function moveTo(el, spec, animate) {
    var p = (spec || '0,0').split(',').map(Number), dx = p[0] || 0, dy = p[1] || 0;
    var old = el._at || [0, 0];
    el._at = [dx, dy];
    var t = 'translate(' + dx + 'px, ' + dy + 'px)';
    if (old[0] === dx && old[1] === dy) { el.style.transform = t; return; }
    var box = { width: 1, height: 1 };
    try { box = el.getBBox(); } catch (e) { /* not rendered */ }
    var far = Math.abs(dx - old[0]) > 2 * Math.max(box.width, 1) || Math.abs(dy - old[1]) > 2 * Math.max(box.height, 1);
    var fade = el.hasAttribute('data-fade') || (far && !el.hasAttribute('data-slide'));
    if (!animate || reduced) { el.style.transition = 'none'; el.style.transform = t; return; }
    if (!fade) { el.style.transition = ''; el.style.transform = t; return; }
    el.classList.add('fading');
    clearTimeout(el._fadeTimer);
    el._fadeTimer = setTimeout(function () {
      el.style.transition = 'none';
      el.style.transform = t;
      el.getBoundingClientRect(); // apply the jump before fading back in
      el.style.transition = '';
      el.classList.remove('fading');
    }, FADE);
  }
  function textTo(el, value, animate) {
    if (value === null || el.textContent === value) return;
    if (!animate || reduced) { el.textContent = value; return; }
    el.classList.add('fading');
    clearTimeout(el._textTimer);
    el._textTimer = setTimeout(function () {
      el.textContent = value;
      el.classList.remove('fading');
      el.classList.add('changed');
      setTimeout(function () { el.classList.remove('changed'); }, 900);
    }, FADE);
  }
  document.querySelectorAll('.stepper').forEach(function (fig) {
    var total = +fig.dataset.steps || 1, step = 0, timer = null;
    var delay = +fig.dataset.delay || 3200;
    var list = fig.querySelector('.captions');
    var captions = fig.querySelectorAll('.captions > li');
    var controls = document.createElement('div');
    controls.className = 'controls';
    controls.innerHTML = '<button type="button" data-a="first" aria-label="First step" title="First step" data-i18n-aria-label data-i18n-title>' + ICON.first + '</button>' +
      '<button type="button" data-a="prev" aria-label="Previous step" title="Previous step (←)" data-i18n-aria-label data-i18n-title>' + ICON.prev + '</button>' +
      '<button type="button" class="play" data-a="play" aria-label="Play">' + ICON.play + '<span>' + tr('Play') + '</span></button>' +
      '<button type="button" data-a="next" aria-label="Next step" title="Next step (→)" data-i18n-aria-label data-i18n-title>' + ICON.next + '</button>' +
      '<span class="progress"><i></i></span><span class="count"></span>';
    var svg = fig.querySelector('svg');
    var view = drawingBox(svg);
    if (svg.viewBox && svg.viewBox.baseVal && svg.viewBox.baseVal.height > TALL) svg.style.maxHeight = 'none'; // drawn at its size: the head follows the scroll
    // Controls + explanation above the drawing: what happens is read while the drawing changes
    var head = document.createElement('div');
    head.className = 'step-head';
    head.appendChild(controls);
    if (list) { list.setAttribute('aria-live', 'polite'); head.appendChild(list); }
    view.insertAdjacentElement('beforebegin', head);
    var play = controls.querySelector('[data-a="play"]');
    // Sticky head only for a figure taller than the visible area (under the 56px top bar) and a head that leaves the drawing some
    // room: a figure that fits the screen keeps its controls in place, a long caption on a phone never covers the drawing
    function fit() {
      var high = window.innerHeight;
      fig.classList.toggle('tall', fig.getBoundingClientRect().height > high - 72 && head.getBoundingClientRect().height < high * HEAD_MAX);
    }
    fit();
    window.addEventListener('resize', fit);
    if (window.docI18n) window.docI18n.onChange(fit);
    levelListeners.push(function () { setTimeout(fit, 0); });
    // A drawing that scrolls (a phone): what this step lights stays in the box
    function follow() {
      if (view.scrollWidth <= view.clientWidth + 1) return;
      var lit = svg.querySelector('.hl, .hl-warn');
      if (!lit) return;
      var box = lit.getBoundingClientRect(), frame = view.getBoundingClientRect();
      if (box.left < frame.left || box.right > frame.right) view.scrollLeft += box.left + box.width / 2 - (frame.left + frame.width / 2);
    }
    function render(animate) {
      svg.querySelectorAll('[data-show]').forEach(function (el) { el.classList.toggle('off', !inRange(el.dataset.show, step)); });
      svg.querySelectorAll('[data-hl]').forEach(function (el) { el.classList.toggle('hl', inRange(el.dataset.hl, step)); });
      svg.querySelectorAll('[data-warn]').forEach(function (el) { el.classList.toggle('hl-warn', inRange(el.dataset.warn, step)); });
      svg.querySelectorAll('[data-at]').forEach(function (el) { moveTo(el, valueAt(el.dataset.at, step), animate); });
      svg.querySelectorAll('[data-text]').forEach(function (el) { textTo(el, valueAt(el.dataset.text, step), animate); });
      captions.forEach(function (li, i) { li.classList.toggle('on', i === step); li.setAttribute('aria-hidden', i === step ? 'false' : 'true'); });
      controls.querySelector('.count').textContent = (step + 1) + ' / ' + total;
      controls.querySelector('.progress i').style.width = (100 * step / Math.max(1, total - 1)) + '%';
      follow();
    }
    function playLabel() { var k = timer ? 'Pause' : 'Play'; play.innerHTML = (timer ? ICON.pause : ICON.play) + '<span>' + tr(k) + '</span>'; play.setAttribute('aria-label', tr(k)); }
    if (window.docI18n) window.docI18n.onChange(playLabel);
    function stop() { clearInterval(timer); timer = null; playLabel(); }
    // One step at a time animates; a jump (first step, restart) is applied at once
    function go(s) { var next = Math.max(0, Math.min(total - 1, s)); var near = Math.abs(next - step) === 1; step = next; render(near); }
    controls.addEventListener('click', function (e) {
      var b = e.target.closest('button');
      if (!b) return;
      var a = b.dataset.a;
      if (a !== 'play') stop();
      if (a === 'first') go(0);
      else if (a === 'prev') go(step - 1);
      else if (a === 'next') go(step + 1);
      else if (timer) stop();
      else {
        if (step === total - 1) go(0);
        timer = setInterval(function () { if (step >= total - 1) stop(); else go(step + 1); }, reduced ? Math.max(delay, 4500) : delay);
        playLabel();
      }
    });
    fig.tabIndex = 0;
    fig.addEventListener('keydown', function (e) {
      if (e.target === view) return; // the box of a drawing that scrolls: the arrows scroll it
      if (e.key === 'ArrowRight') { stop(); go(step + 1); e.preventDefault(); }
      if (e.key === 'ArrowLeft') { stop(); go(step - 1); e.preventDefault(); }
    });
    // On paper: the whole drawing (its last state), the captions under each other (explain.css)
    var before = 0;
    window.addEventListener('beforeprint', function () { before = step; stop(); step = total - 1; render(false); });
    window.addEventListener('afterprint', function () { step = before; render(false); });
    // First state drawn without transitions (no flash of every step at load)
    fig.classList.add('init');
    render(false);
    requestAnimationFrame(function () { requestAnimationFrame(function () { fig.classList.remove('init'); }); });
  });
  document.querySelectorAll('figure:not(.stepper) svg[viewBox]').forEach(drawingBox);
  reachable();
  if ('ResizeObserver' in window) { // the width of the screen, a level or a tab shown
    var boxWatch = new ResizeObserver(reachable);
    document.querySelectorAll(SCROLL_BOXES).forEach(function (box) { boxWatch.observe(box); });
  } else window.addEventListener('resize', reachable);

  if (window.docI18n && window.docI18n.apply) window.docI18n.apply(); // labels of the controls created above

  // ---------- Playgrounds: <div class="playground" data-play="name"> ----------
  // Inputs [data-in="x"] (range / number / text / checkbox / select), outputs [data-out="y"]; the page registers
  // explain.play('name', function (values, el) { return { y: 'text' }; }): called at load and on every input.
  var plays = {};
  function values(el) {
    var v = {};
    el.querySelectorAll('[data-in]').forEach(function (i) {
      v[i.dataset.in] = i.type === 'checkbox' ? i.checked : (i.type === 'range' || i.type === 'number') ? +i.value : i.value;
    });
    return v;
  }
  function run(name) {
    var el = document.querySelector('.playground[data-play="' + name + '"]');
    if (!el || !plays[name]) return;
    var out = plays[name](values(el), el) || {};
    Object.keys(out).forEach(function (k) {
      el.querySelectorAll('[data-out="' + k + '"]').forEach(function (o) { o.textContent = out[k]; });
    });
  }
  function fmt(x, d) {
    if (!isFinite(x)) return '∞';
    return Number(x).toLocaleString(root.lang || 'en', { minimumFractionDigits: d || 0, maximumFractionDigits: d === undefined ? 2 : d });
  }
  window.explain = {
    play: function (name, fn) {
      plays[name] = fn;
      var el = document.querySelector('.playground[data-play="' + name + '"]');
      if (el) el.addEventListener('input', function () { run(name); });
      run(name);
    },
    run: run,
    fmt: fmt,
    level: function () { return root.getAttribute('data-level'); },
    onLevel: function (f) { levelListeners.push(f); }
  };

  // ---------- Glossary terms: hover, focus or tap opens the definition, Escape closes it ----------
  // One box in <body>, fixed on the screen: no scrolling block clips it, it is put inside the screen (above the term when there is
  // no room under it) and a long definition scrolls in it. It stays open while the pointer is on it, or when it was tapped.
  var pop = null, popTerm = null, popTimer = null;
  var POP_GAP = 6, POP_EDGE = 8, POP_DELAY = 150;
  function popHide() {
    clearTimeout(popTimer);
    if (pop) pop.hidden = true;
    popTerm = null;
  }
  function popPlace() {
    if (!popTerm) return;
    var lines = popTerm.getClientRects();
    if (!lines.length || lines[0].bottom < 0 || lines[lines.length - 1].top > window.innerHeight) { popHide(); return; }
    var first = lines[0], last = lines[lines.length - 1], high = pop.offsetHeight, wide = pop.offsetWidth;
    var below = last.bottom + POP_GAP;
    pop.style.left = Math.max(POP_EDGE, Math.min(last.left, root.clientWidth - wide - POP_EDGE)) + 'px';
    pop.style.top = (below + high <= window.innerHeight - POP_EDGE ? below : Math.max(POP_EDGE, first.top - POP_GAP - high)) + 'px';
  }
  function popShow(term) {
    if (!pop) {
      pop = document.createElement('div');
      pop.className = 'dfn-pop';
      pop.setAttribute('role', 'tooltip');
      pop.tabIndex = -1;
      document.body.appendChild(pop);
      pop.addEventListener('pointerenter', function () { clearTimeout(popTimer); });
      pop.addEventListener('pointerleave', popLater);
      pop.addEventListener('focusout', function (e) { if (e.relatedTarget !== popTerm) popHide(); });
    }
    clearTimeout(popTimer);
    popTerm = term;
    pop.textContent = term.dataset.def;
    pop.scrollTop = 0;
    pop.hidden = false;
    popPlace();
  }
  function popLater() {
    clearTimeout(popTimer);
    popTimer = setTimeout(function () { if (document.activeElement !== popTerm && document.activeElement !== pop) popHide(); }, POP_DELAY);
  }
  document.querySelectorAll('dfn[data-def]').forEach(function (d) {
    d.tabIndex = 0;
    d.setAttribute('aria-label', d.textContent + ': ' + d.dataset.def);
    d.addEventListener('pointerenter', function (e) { if (e.pointerType === 'mouse') popShow(d); });
    d.addEventListener('pointerleave', function (e) { if (e.pointerType === 'mouse') popLater(); });
    d.addEventListener('focus', function () { popShow(d); });
    d.addEventListener('click', function () { popShow(d); }); // a tap that gives no focus (iOS)
    d.addEventListener('blur', function (e) { if (e.relatedTarget !== pop) popHide(); });
  });
  document.addEventListener('pointerdown', function (e) { if (popTerm && e.target !== popTerm && !pop.contains(e.target)) popHide(); });
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape') popHide(); });
  window.addEventListener('scroll', function (e) { if (e.target !== pop) popPlace(); }, true);
  window.addEventListener('resize', popPlace);
})();
