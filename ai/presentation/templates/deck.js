/* Shared deck runtime: navigation, build-steps, chapters, map, hash, fullscreen, swipe, selftest. No look here. */
(function () {
    'use strict';
    var root = document.documentElement;
    var errors = [];
    window.addEventListener('error', function (e) { errors.push(String(e.message)); });
    root.classList.add('js');
    var deck = document.getElementById('deck'), stage = document.getElementById('stage');
    var slides = [].slice.call(deck.children).filter(function (e) { return e.classList.contains('slide'); });
    var N = slides.length, cur = 0, k = 0;
    var maxk = slides.map(function (s) { var m = 0; [].forEach.call(s.querySelectorAll('[data-s]'), function (e) { m = Math.max(m, +e.getAttribute('data-s') || 0); }); return m; });
    var $ = function (id) { return document.getElementById(id); };
    var chapters = [], chOf = [];
    slides.forEach(function (s, i) {
        var c = s.getAttribute('data-ch') || '';
        if (!chapters.length || chapters[chapters.length - 1].name !== c) chapters.push({ name: c, first: i });
        chOf[i] = chapters.length - 1;
    });
    var chBox = $('chapters'), bar = $('bar'), count = $('count'), dots = $('steps'), map = $('map');
    chapters.forEach(function (c, n) {
        var b = document.createElement('button'); b.type = 'button'; b.textContent = c.name; b.setAttribute('data-n', n);
        b.addEventListener('click', function () { go(c.first, 0); b.blur(); }); chBox.appendChild(b);
    });
    function title(s) { return s.getAttribute('data-title') || (s.querySelector('h1,h2') || {}).textContent || 'Diapo'; }
    chapters.forEach(function (c, n) {
        var g = document.createElement('div'); g.className = 'mapch';
        var h = document.createElement('h3'); h.textContent = c.name; g.appendChild(h);
        slides.forEach(function (s, i) {
            if (chOf[i] !== n) return;
            var b = document.createElement('button'); b.type = 'button'; b.textContent = (i + 1) + '. ' + title(s);
            b.addEventListener('click', function () { toggleMap(false); go(i, 0); b.blur(); }); g.appendChild(b);
        });
        map.appendChild(g);
    });
    function fit() {
        var w = innerWidth, h = innerHeight, portrait = w / h < 0.9;
        var sw = portrait ? 540 : 1280, sh = portrait ? 960 : 720, sc = Math.min(w / sw, h / sh);
        root.classList.toggle('portrait', portrait);
        stage.style.width = sw + 'px'; stage.style.height = sh + 'px';
        stage.style.transform = 'translate(' + ((w - sw * sc) / 2) + 'px,' + ((h - sh * sc) / 2) + 'px) scale(' + sc + ')';
    }
    function render() {
        slides.forEach(function (s, i) {
            var kk = i === cur ? k : (i < cur ? maxk[i] : 0);
            s.setAttribute('data-state', i < cur ? 'past' : i > cur ? 'future' : 'active');
            s.setAttribute('data-k', kk); s.setAttribute('aria-hidden', i !== cur);
            [].forEach.call(s.querySelectorAll('[data-s]'), function (e) { e.classList.toggle('on', (+e.getAttribute('data-s')) <= kk); });
        });
        var p = (cur + (maxk[cur] ? k / (maxk[cur] + 1) : 0)) / (N - 1);
        bar.style.setProperty('--p', Math.min(1, p).toFixed(4));
        count.textContent = (cur + 1) + ' / ' + N;
        dots.innerHTML = '';
        for (var i = 0; i <= maxk[cur]; i++) { var d = document.createElement('i'); if (i <= k) d.className = 'on'; dots.appendChild(d); }
        dots.style.visibility = 'visible'; /* a slide without build still shows its one square */
        [].forEach.call(chBox.children, function (b, n) { b.className = n === chOf[cur] ? 'cur' : (n < chOf[cur] ? 'done' : ''); });
        root.setAttribute('data-slide', cur + 1); root.setAttribute('data-first', cur === 0); root.setAttribute('data-last', cur === N - 1);
        var h = '#' + (cur + 1) + (k ? '.' + k : '');
        if (location.hash !== h) { try { history.replaceState(null, '', h); } catch (e) { /* file: pages may refuse */ } }
    }
    function go(i, kk) { cur = Math.max(0, Math.min(N - 1, i)); k = Math.max(0, Math.min(maxk[cur], kk || 0)); render(); }
    function next() { if (k < maxk[cur]) { k++; render(); } else if (cur < N - 1) go(cur + 1, 0); }
    function prev() { if (k > 0) { k--; render(); } else if (cur > 0) go(cur - 1, maxk[cur - 1]); }
    function toggleMap(on) { map.hidden = on === undefined ? !map.hidden : !on; root.classList.toggle('mapopen', !map.hidden); }
    function fullscreen() {
        if (document.fullscreenElement) { document.exitFullscreen(); return; }
        var f = root.requestFullscreen || root.webkitRequestFullscreen; if (f) f.call(root);
    }
    function parseHash() {
        var m = /^#(\d+)(?:\.(\d+))?$/.exec(location.hash); if (!m) return false;
        go(+m[1] - 1, +(m[2] || 0)); return true;
    }
    function key(e) {
        if (e.ctrlKey || e.metaKey || e.altKey) return;
        if (/^(INPUT|TEXTAREA|SELECT)$/.test((e.target || {}).tagName)) return;
        var c = e.key, used = true;
        if (c === 'ArrowRight' || c === 'ArrowDown' || c === 'PageDown' || c === 'Enter' || (c === ' ' && !e.shiftKey)) next();
        else if (c === 'ArrowLeft' || c === 'ArrowUp' || c === 'PageUp' || c === 'Backspace' || (c === ' ' && e.shiftKey)) prev();
        else if (c === 'Home') go(0, 0);
        else if (c === 'End') go(N - 1, maxk[N - 1]);
        else if (c === 'o' || c === 'O' || c === 'm' || c === 'M') toggleMap();
        else if (c === 'Escape') toggleMap(false);
        else if (c === 'f' || c === 'F') fullscreen();
        else if (c === 'n' || c === 'N') root.classList.toggle('notes');
        else used = false;
        if (used) e.preventDefault();
    }
    window.addEventListener('keydown', key);
    window.addEventListener('resize', fit);
    window.addEventListener('hashchange', parseHash);
    $('prev').addEventListener('click', function (e) { prev(); e.currentTarget.blur(); });
    $('next').addEventListener('click', function (e) { next(); e.currentTarget.blur(); });
    $('zl').addEventListener('click', prev); $('zr').addEventListener('click', next);
    $('ovbtn').addEventListener('click', function (e) { toggleMap(); e.currentTarget.blur(); });
    $('fsbtn').addEventListener('click', function (e) { fullscreen(); e.currentTarget.blur(); });
    map.addEventListener('click', function (e) { if (e.target === map) toggleMap(false); });
    var tx = 0, ty = 0;
    stage.addEventListener('touchstart', function (e) { tx = e.changedTouches[0].clientX; ty = e.changedTouches[0].clientY; }, { passive: true });
    stage.addEventListener('touchend', function (e) {
        var dx = e.changedTouches[0].clientX - tx, dy = e.changedTouches[0].clientY - ty;
        if (Math.abs(dx) > 50 && Math.abs(dx) > Math.abs(dy) * 1.5) { if (dx < 0) next(); else prev(); }
    }, { passive: true });
    deck.addEventListener('click', function (e) {
        var o = e.target.closest ? e.target.closest('.opt') : null;
        if (o) {
            var q = o.parentNode; if (q.getAttribute('data-done')) return;
            q.setAttribute('data-done', '1');
            [].forEach.call(q.querySelectorAll('.opt'), function (x) { x.classList.add(x.getAttribute('data-ok') === '1' ? 'right' : 'wrong'); });
            o.classList.add('picked'); return;
        }
        var t = e.target.closest ? e.target.closest('.tap') : null;
        if (t) { t.classList.toggle('shown'); t.textContent = t.classList.contains('shown') ? (t.getAttribute('data-a') || '') : '?'; }
    });
    fit();
    root.classList.add('noanim');
    if (!parseHash()) render();
    requestAnimationFrame(function () { requestAnimationFrame(function () { if (!/selftest/.test(location.search)) root.classList.remove('noanim'); }); });

    if (/selftest/.test(location.search)) {
        var rep = [], seen = {}, states = 0, sr = stage.getBoundingClientRect();
        var add = function (kind, i, kk, e) {
            var m = kind + ' slide ' + (i + 1) + ' <' + e.tagName.toLowerCase() + ' ' + (e.getAttribute('class') || '') + '> ' + (e.textContent || '').replace(/\s+/g, ' ').slice(0, 24);
            if (!seen[m]) { seen[m] = 1; rep.push(m + ' (first at step ' + kk + ')'); }
        };
        for (var i = 0; i < N; i++) {
            for (var kk = 0; kk <= maxk[i]; kk++) {
                go(i, kk); states++;
                var s = slides[i], all = s.querySelectorAll('*'), r0 = stage.getBoundingClientRect();
                for (var j = 0; j < all.length; j++) {
                    var e = all[j], cs = getComputedStyle(e);
                    if (cs.display === 'none' || cs.visibility === 'hidden' || +cs.opacity < 0.05 || e.closest('.notes,[data-nocheck]')) continue;
                    var hid = false, p = e.parentNode; while (p && p !== s) { if (p.nodeType === 1 && +getComputedStyle(p).opacity < 0.05) { hid = true; break; } p = p.parentNode; }
                    if (hid) continue;
                    var r = e.getBoundingClientRect(); if (r.width === 0 && r.height === 0) continue;
                    var tol = 3 * (r0.width / stage.offsetWidth), hr = $('hud').getBoundingClientRect();
                    if (r.right > hr.left + tol && r.left < hr.right - tol && r.bottom > hr.top + tol && r.top < hr.bottom - tol) add('HUD', i, kk, e);
                    var bx = e.closest('.col, .flow > li, .k-title .meta li');
                    if (bx && bx !== e) { var br = bx.getBoundingClientRect(); if (r.right > br.right + tol || r.left < br.left - tol || r.bottom > br.bottom + tol) add('BOX', i, kk, e); }
                    if (r.left < r0.left - tol || r.right > r0.right + tol || r.top < r0.top - tol || r.bottom > r0.bottom + tol)
                        add('OUT', i, kk, e);
                    if (!(e instanceof SVGElement) && cs.overflow !== 'visible' && (e.scrollWidth > e.clientWidth + 2 || e.scrollHeight > e.clientHeight + 2) && !e.classList.contains('slide'))
                        add('CLIP', i, kk, e);
                }
            }
        }
        go(0, 0);
        var presses = 0, ev = function (kk) { window.dispatchEvent(new KeyboardEvent('keydown', { key: kk, bubbles: true, cancelable: true })); };
        while (!(cur === N - 1 && k === maxk[N - 1]) && presses < 1000) { ev('ArrowRight'); presses++; }
        var fwd = presses; while (cur > 0 || k > 0) { ev('ArrowLeft'); presses++; if (presses > 2000) break; }
        ev('End'); var endOk = cur === N - 1; ev('Home'); var homeOk = cur === 0 && k === 0; ev(' '); var spaceOk = (cur > 0 || k > 0);
        ev('o'); var mapOk = !map.hidden; ev('Escape'); mapOk = mapOk && map.hidden; go(0, 0);
        if (fwd !== states - 1 || presses !== 2 * (states - 1) || !endOk || !homeOk || !spaceOk || !mapOk) rep.push('KEYS fwd=' + fwd + ' total=' + presses + ' expected=' + (states - 1) + ' end=' + endOk + ' home=' + homeOk + ' space=' + spaceOk + ' map=' + mapOk);
        var out = document.createElement('pre'); out.id = 'selftest';
        out.setAttribute('style', 'position:fixed;z-index:99999;left:0;top:0;right:0;margin:0;padding:12px;background:#fff;color:#000;font:16px/1.3 monospace;white-space:pre-wrap;max-height:100vh;overflow:hidden');
        out.textContent = 'SELFTEST ' + (rep.length || errors.length ? 'FAIL' : 'OK') + ': ' + N + ' slides, ' + states + ' states, ' + errors.length + ' js errors, ' + rep.length + ' layout issues, keys ' + (fwd === states - 1 ? 'OK' : 'BAD') + '\n' + errors.concat(rep.slice(0, 25)).join('\n');
        document.body.appendChild(out); document.title = 'SELFTEST ' + (rep.length || errors.length ? 'FAIL' : 'OK');
    }
})();
