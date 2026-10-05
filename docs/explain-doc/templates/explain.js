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
  var reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  var FADE = 160; // ms of each half of a fade (out, then in)
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
    controls.innerHTML = '<button type="button" data-a="first" aria-label="First step" title="First step">' + ICON.first + '</button>' +
      '<button type="button" data-a="prev" aria-label="Previous step" title="Previous step (←)">' + ICON.prev + '</button>' +
      '<button type="button" class="play" data-a="play" aria-label="Play">' + ICON.play + '<span>Play</span></button>' +
      '<button type="button" data-a="next" aria-label="Next step" title="Next step (→)">' + ICON.next + '</button>' +
      '<span class="progress"><i></i></span><span class="count"></span>';
    var svg = fig.querySelector('svg');
    var view = document.createElement('div');
    view.className = 'view';
    svg.replaceWith(view);
    view.appendChild(svg);
    // Controls + explanation above the drawing: what happens is read while the drawing changes
    var head = document.createElement('div');
    head.className = 'step-head';
    head.appendChild(controls);
    if (list) { list.setAttribute('aria-live', 'polite'); head.appendChild(list); }
    view.insertAdjacentElement('beforebegin', head);
    var play = controls.querySelector('[data-a="play"]');
    function render(animate) {
      svg.querySelectorAll('[data-show]').forEach(function (el) { el.classList.toggle('off', !inRange(el.dataset.show, step)); });
      svg.querySelectorAll('[data-hl]').forEach(function (el) { el.classList.toggle('hl', inRange(el.dataset.hl, step)); });
      svg.querySelectorAll('[data-warn]').forEach(function (el) { el.classList.toggle('hl-warn', inRange(el.dataset.warn, step)); });
      svg.querySelectorAll('[data-at]').forEach(function (el) { moveTo(el, valueAt(el.dataset.at, step), animate); });
      svg.querySelectorAll('[data-text]').forEach(function (el) { textTo(el, valueAt(el.dataset.text, step), animate); });
      captions.forEach(function (li, i) { li.classList.toggle('on', i === step); li.setAttribute('aria-hidden', i === step ? 'false' : 'true'); });
      controls.querySelector('.count').textContent = (step + 1) + ' / ' + total;
      controls.querySelector('.progress i').style.width = (100 * step / Math.max(1, total - 1)) + '%';
    }
    function stop() { clearInterval(timer); timer = null; play.innerHTML = ICON.play + '<span>Play</span>'; play.setAttribute('aria-label', 'Play'); }
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
        play.innerHTML = ICON.pause + '<span>Pause</span>'; play.setAttribute('aria-label', 'Pause');
        timer = setInterval(function () { if (step >= total - 1) stop(); else go(step + 1); }, reduced ? Math.max(delay, 4500) : delay);
      }
    });
    fig.tabIndex = 0;
    fig.addEventListener('keydown', function (e) {
      if (e.key === 'ArrowRight') { stop(); go(step + 1); e.preventDefault(); }
      if (e.key === 'ArrowLeft') { stop(); go(step - 1); e.preventDefault(); }
    });
    render(false);
  });

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

  // ---------- Glossary terms reachable with the keyboard ----------
  document.querySelectorAll('dfn[data-def]').forEach(function (d) { d.tabIndex = 0; d.setAttribute('aria-label', d.textContent + ': ' + d.dataset.def); });
})();
