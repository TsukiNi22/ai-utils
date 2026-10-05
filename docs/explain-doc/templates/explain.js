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
  // In the SVG: data-show="2" (only step 2), "2-" (from 2), "1-3" (1 to 3); data-hl / data-warn="3,5" add the
  // class hl / hl-warn at those steps. Captions: <ol class="captions"><li> per step (first = step 0).
  function inRange(spec, step) {
    return spec.split(',').some(function (part) {
      var m = part.trim().match(/^(\d+)(-)?(\d+)?$/);
      if (!m) return false;
      var a = +m[1], b = m[2] ? (m[3] !== undefined ? +m[3] : Infinity) : a;
      return step >= a && step <= b;
    });
  }
  var reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  document.querySelectorAll('.stepper').forEach(function (fig) {
    var total = +fig.dataset.steps || 1, step = 0, timer = null;
    var delay = +fig.dataset.delay || 2400;
    var captions = fig.querySelectorAll('.captions > li');
    var controls = document.createElement('div');
    controls.className = 'controls';
    controls.innerHTML = '<button type="button" data-a="first" aria-label="First step">&#x23EE;</button>' +
      '<button type="button" data-a="prev" aria-label="Previous step">&#x25C0;</button>' +
      '<button type="button" data-a="play" aria-label="Play">&#x25B6; Play</button>' +
      '<button type="button" data-a="next" aria-label="Next step">&#x25B6;&#x25B6;</button>' +
      '<span class="progress"><i></i></span><span class="count"></span>';
    var svg = fig.querySelector('svg');
    var view = document.createElement('div');
    view.className = 'view';
    svg.replaceWith(view);
    view.appendChild(svg);
    view.insertAdjacentElement('afterend', controls);
    var play = controls.querySelector('[data-a="play"]');
    function render() {
      svg.querySelectorAll('[data-show]').forEach(function (el) { el.classList.toggle('off', !inRange(el.dataset.show, step)); });
      svg.querySelectorAll('[data-hl]').forEach(function (el) { el.classList.toggle('hl', inRange(el.dataset.hl, step)); });
      svg.querySelectorAll('[data-warn]').forEach(function (el) { el.classList.toggle('hl-warn', inRange(el.dataset.warn, step)); });
      captions.forEach(function (li, i) { li.classList.toggle('on', i === step); });
      controls.querySelector('.count').textContent = (step + 1) + ' / ' + total;
      controls.querySelector('.progress i').style.width = (100 * step / Math.max(1, total - 1)) + '%';
    }
    function stop() { clearInterval(timer); timer = null; play.innerHTML = '&#x25B6; Play'; play.setAttribute('aria-label', 'Play'); }
    function go(s) { step = Math.max(0, Math.min(total - 1, s)); render(); }
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
        play.innerHTML = '&#x23F8; Pause'; play.setAttribute('aria-label', 'Pause');
        timer = setInterval(function () { if (step >= total - 1) stop(); else go(step + 1); }, reduced ? Math.max(delay, 4000) : delay);
      }
    });
    fig.tabIndex = 0;
    fig.addEventListener('keydown', function (e) {
      if (e.key === 'ArrowRight') { stop(); go(step + 1); e.preventDefault(); }
      if (e.key === 'ArrowLeft') { stop(); go(step - 1); e.preventDefault(); }
    });
    render();
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
