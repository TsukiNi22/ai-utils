#!/usr/bin/env python3
"""
Check how a page really renders: headless Firefox opens it at several widths, in both themes (and in every language and level
it offers), and every text shown is measured - what check_style.py cannot see from the sources.

Usage:
    check_render.py <page.html[?query]> ... [--widths 1280,768,375] [--themes light,dark] [--click SELECTOR ...] [--shots DIR] [--max 12] [--timeout 180]

Errors (exit 1):
- contrast: a text under 4.5:1 on what is really behind it (3:1 for a large text), black on black included, SVG text on its shape;
- page-scroll: the page scrolls sideways on that width (with the elements that stick out);
- out-of-box: a text past the edge of its card, cell, button, or of its drawing;
- word-cut: a word under 30 characters broken in two by a column too narrow for it;
- word-per-line: three words or more of a cell, one per line, in a column under 150 px (wider: long names, nothing would pair them).
Warnings: two words of a cell on two lines, a sentence squeezed in a column (10 lines and more, under 3 words a line), a column twice too wide next to
one that wraps, a text cut by an overflow: hidden, a text crossing the border of its SVG box, an SVG text drawn under 8 px, a field without contrast.
--click SELECTOR measures again after a click on it (a menu, a popover, the next step); ?q=word after a page is given to it (a search open, an embed mode).

- A tab panel is shown and a <details> opened for the check: what they hold is measured too.
- --shots DIR writes a screenshot per theme and width to look at, nothing replaces looking at it.
Needs: firefox (a profile of its own, deleted after: nothing touches the user's). Exit 2 when the browser gave no result.
"""

import argparse
import functools
import http.server
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import urllib.parse
from pathlib import Path

HARNESS = r"""<!doctype html><html style="color-scheme:%THEME%"><head><meta charset="utf-8"><title>check</title></head><body><script>
const PAGES = %PAGES%, WIDTHS = %WIDTHS%, THEME = "%THEME%", CLICKS = %CLICKS%;
const wait = ms => new Promise(done => setTimeout(done, ms));

function parseColor(text) {
  if (!text) return null;
  let found = text.match(/^rgba?\(([^)]+)\)$/);
  if (found) {
    const part = found[1].split(/[\s,\/]+/).filter(Boolean);
    const alpha = part.length > 3 ? (part[3].endsWith('%') ? parseFloat(part[3]) / 100 : parseFloat(part[3])) : 1;
    return [parseFloat(part[0]), parseFloat(part[1]), parseFloat(part[2]), alpha];
  }
  found = text.match(/^color\(srgb ([^)]+)\)$/);
  if (found) {
    const part = found[1].split(/[\s\/]+/).filter(Boolean).map(parseFloat);
    return [part[0] * 255, part[1] * 255, part[2] * 255, part.length > 3 ? part[3] : 1];
  }
  return null;
}
const over = (top, under) => {
  const alpha = top[3] + under[3] * (1 - top[3]);
  if (alpha === 0) return [0, 0, 0, 0];
  const mix = at => (top[at] * top[3] + under[at] * under[3] * (1 - top[3])) / alpha;
  return [mix(0), mix(1), mix(2), alpha];
};
const luminance = color => {
  const channel = value => { value /= 255; return value <= 0.03928 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4); };
  return 0.2126 * channel(color[0]) + 0.7152 * channel(color[1]) + 0.0722 * channel(color[2]);
};
const contrast = (a, b) => { const la = luminance(a), lb = luminance(b); return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05); };
const hex = color => '#' + color.slice(0, 3).map(value => Math.round(value).toString(16).padStart(2, '0')).join('');

function audit(win, doc) {
  const problems = [];
  const style = el => win.getComputedStyle(el);
  const width = doc.documentElement.clientWidth;
  const short = text => text.replace(/\s+/g, ' ').trim().slice(0, 50);
  const textOf = el => el.innerText || el.textContent || ''; // what is shown: not the language that is hidden
  const path = el => {
    const parts = [];
    for (let node = el, depth = 0; node && depth < 3 && node !== doc.body && node !== doc.documentElement; node = node.parentElement, depth++) {
      let name = node.tagName.toLowerCase();
      if (node.id) name += '#' + node.id;
      else if (typeof node.className === 'string' && node.className.trim()) name += '.' + node.className.trim().split(/\s+/).slice(0, 2).join('.');
      parts.unshift(name);
    }
    return parts.join(' > ');
  };
  const add = (level, kind, el, detail) => problems.push({ level, kind, where: el ? path(el) : '', detail, text: el ? short(textOf(el)) : '' });
  const shown = el => {
    const rect = el.getBoundingClientRect();
    if ((rect.width <= 1 && rect.height <= 1) || rect.right <= 0 || rect.bottom <= -200) return false;
    if (style(el).visibility !== 'visible') return false;
    for (let node = el; node; node = node.parentElement) {
      const cs = style(node);
      if (cs.display === 'none' || parseFloat(cs.opacity) < 0.05) return false;
    }
    return true;
  };
  const opacityOf = el => { let total = 1; for (let node = el; node; node = node.parentElement) total *= parseFloat(style(node).opacity); return total; };
  const canvas = () => (style(doc.documentElement).colorScheme.includes('dark') ? [28, 27, 34, 1] : [255, 255, 255, 1]);
  const backgroundOf = el => {
    const layers = [];
    for (let node = el; node; node = node.parentElement) {
      const cs = style(node);
      if (cs.backgroundImage && cs.backgroundImage !== 'none') return null;
      const color = parseColor(cs.backgroundColor);
      if (color && color[3] > 0) {
        layers.push(color);
        if (color[3] >= 1) break;
      }
    }
    if (!layers.length || layers[layers.length - 1][3] < 1) layers.push(canvas());
    return layers.reduceRight((under, top) => over(top, under));
  };
  const shapesUnder = (el, svg) => {
    const rect = el.getBoundingClientRect(), x = rect.left + rect.width / 2, y = rect.top + rect.height / 2, found = [];
    for (const shape of svg.querySelectorAll('rect, circle, ellipse, path, polygon')) {
      if (el.compareDocumentPosition(shape) & win.Node.DOCUMENT_POSITION_FOLLOWING) break;
      if (shape.closest('defs, marker, clipPath, symbol, pattern, mask')) continue;
      const cs = style(shape), fill = parseColor(cs.fill);
      if (!fill || fill[3] === 0 || cs.display === 'none') continue;
      const box = shape.getBoundingClientRect();
      if (x < box.left || x > box.right || y < box.top || y > box.bottom) continue;
      try {
        const point = svg.createSVGPoint();
        point.x = x; point.y = y;
        const local = point.matrixTransform(shape.getScreenCTM().inverse());
        if (!shape.isPointInFill(local)) continue;
      } catch (error) { continue; }
      found.push({ shape, fill: [fill[0], fill[1], fill[2], fill[3] * parseFloat(cs.fillOpacity) * opacityOf(shape)] });
    }
    return found;
  };
  const checkContrast = (el, foreground, under) => {
    if (!foreground || !under || foreground[3] === 0 || el.closest('[disabled], [aria-disabled="true"]')) return;
    const color = over(foreground, under), ratio = contrast(color, under), cs = style(el), size = parseFloat(cs.fontSize);
    const need = size >= 24 || (size >= 18.66 && parseInt(cs.fontWeight, 10) >= 700) ? 3 : 4.5;
    if (ratio < need) add('error', 'contrast', el, ratio.toFixed(2) + ':1 (' + hex(color) + ' on ' + hex(under) + ', ' + need + ' needed)');
  };
  const textRects = el => {
    const range = doc.createRange();
    range.selectNodeContents(el);
    return [...range.getClientRects()].filter(rect => rect.width > 0 && rect.height > 0);
  };
  const lineCount = rects => {
    const sorted = rects.slice().sort((a, b) => a.top - b.top);
    let lines = 0, last = -1e9;
    for (const rect of sorted) {
      const middle = (rect.top + rect.bottom) / 2;
      if (middle - last > Math.max(4, rect.height * 0.6)) { lines++; last = middle; }
    }
    return lines;
  };

  const owners = [], seen = new Set();
  const walker = doc.createTreeWalker(doc.body, win.NodeFilter.SHOW_TEXT);
  while (walker.nextNode()) {
    const node = walker.currentNode, el = node.parentElement;
    if (!node.nodeValue.trim() || !el || seen.has(el) || /^(script|style|noscript|template|title|option|desc)$/i.test(el.tagName)) continue;
    seen.add(el);
    if (el.closest('defs, marker, symbol, clipPath') || !shown(el)) continue;
    owners.push(el);
  }

  // The page itself
  if (doc.documentElement.scrollWidth > width + 1) {
    add('error', 'page-scroll', null, 'the page scrolls sideways: ' + doc.documentElement.scrollWidth + ' px on a screen of ' + width + ' px');
    let told = 0;
    for (const el of doc.body.querySelectorAll('*')) {
      if (told >= 5 || el.getBoundingClientRect().right <= width + 1 || el.children.length || !shown(el)) continue;
      let scrolled = false;
      for (let node = el.parentElement; node && node !== doc.body; node = node.parentElement) if (style(node).overflowX !== 'visible') scrolled = true;
      if (!scrolled) { add('error', 'page-scroll', el, 'sticks out at ' + Math.round(el.getBoundingClientRect().right) + ' px'); told++; }
    }
  }

  for (const el of owners) {
    const svg = el.ownerSVGElement;
    const cs = style(el);
    const rects = textRects(el);
    if (svg) {
      const fill = parseColor(cs.fill);
      if (fill) {
        const shapes = shapesUnder(el, svg);
        let under = backgroundOf(svg);
        if (under) for (const item of shapes) under = over(item.fill, under);
        checkContrast(el, [fill[0], fill[1], fill[2], fill[3] * parseFloat(cs.fillOpacity) * opacityOf(el)], under);
        const matrix = el.getScreenCTM && el.getScreenCTM();
        const drawn = matrix ? parseFloat(cs.fontSize) * Math.hypot(matrix.a, matrix.b) : 99;
        if (drawn < 8) add('warning', 'small-text', el, 'drawn at ' + drawn.toFixed(1) + ' px: unreadable, let the drawing scroll instead of shrinking');
        const top = shapes[shapes.length - 1], box = el.getBoundingClientRect(), edge = svg.getBoundingClientRect();
        if (box.right > edge.right + 1 || box.left < edge.left - 1) add('error', 'out-of-box', el, 'past the edge of its drawing by ' + Math.round(Math.max(box.right - edge.right, edge.left - box.left)) + ' px');
        else if (top && top.shape.tagName.toLowerCase() === 'rect') {
          const frame = top.shape.getBoundingClientRect();
          if (box.right > frame.right + 1 || box.left < frame.left - 1) add('warning', 'out-of-box', el, 'crosses the border of its box by ' + Math.round(Math.max(box.right - frame.right, frame.left - box.left)) + ' px');
        }
      }
      continue;
    }
    const color = parseColor(cs.color);
    if (color) checkContrast(el, [color[0], color[1], color[2], color[3] * opacityOf(el)], backgroundOf(el));

    // Past the edge of what holds it
    let box = null, clip = null, scrolled = false;
    for (let node = cs.display === 'inline' ? el.parentElement : el; node && node !== doc.documentElement; node = node.parentElement) {
      const ns = style(node);
      if (ns.overflowX === 'auto' || ns.overflowX === 'scroll') { scrolled = true; break; }
      if (ns.overflowX === 'hidden' || ns.overflowX === 'clip') { clip = node; break; }
      if (ns.display === 'inline') continue;
      const fillColor = parseColor(ns.backgroundColor);
      if ((fillColor && fillColor[3] > 0) || (parseFloat(ns.borderLeftWidth) > 0 && parseFloat(ns.borderRightWidth) > 0)) { box = node; break; }
    }
    // off the left of the screen: the page does not scroll for that, it is just lost
    const lost = rects.reduce((most, rect) => Math.max(most, -rect.left), 0);
    if (!scrolled && lost > 1.5) add('error', 'page-scroll', el, 'starts ' + Math.round(lost) + ' px off the left of the screen');
    const past = holder => {
      const frame = holder.getBoundingClientRect();
      return rects.reduce((most, rect) => Math.max(most, rect.right - frame.right, frame.left - rect.left), 0);
    };
    if (box && past(box) > 1.5) add('error', 'out-of-box', el, 'past the edge of ' + path(box) + ' by ' + Math.round(past(box)) + ' px');
    if (clip && past(clip) > 1.5 && style(clip).textOverflow !== 'ellipsis') add('warning', 'cut', el, 'cut by the overflow of ' + path(clip) + ' (' + Math.round(past(clip)) + ' px hidden)');

    // A word per line, anywhere
    if (!el.closest('td, th') && cs.whiteSpace === 'normal') {
      const words = (el.innerText || '').trim().split(/\s+/).filter(Boolean);
      if (words.length >= 3 && el.getBoundingClientRect().width < 200 && lineCount(rects) >= words.length) add('warning', 'word-per-line', el, words.length + ' words on ' + lineCount(rects) + ' lines, ' + Math.round(el.getBoundingClientRect().width) + ' px wide');
    }
  }

  // Tables: what they do to their words
  const measure = doc.createElement('canvas').getContext('2d');
  const wordWidth = (word, parent) => {
    const cs = style(parent);
    measure.font = cs.fontStyle + ' ' + cs.fontWeight + ' ' + cs.fontSize + ' ' + cs.fontFamily;
    const text = cs.textTransform === 'uppercase' ? word.toUpperCase() : word;
    return measure.measureText(text).width + (parseFloat(cs.letterSpacing) || 0) * text.length;
  };
  for (const table of doc.querySelectorAll('table')) {
    if (!shown(table)) continue;
    const columns = new Map();
    for (const row of table.rows) for (const cell of row.cells) {
      if (!shown(cell)) continue;
      const cs = style(cell), rects = textRects(cell);
      const room = cell.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight);
      const words = (cell.innerText || '').trim().split(/\s+/).filter(Boolean);
      const lines = lineCount(rects);
      // a column of a good width holding long names, one a line, or a word longer than the column taking lines of its own: nothing to fix
      const narrow = cell.getBoundingClientRect().width < 150 && !words.some(word => word.length > 30);
      if (narrow && words.length >= 2 && lines >= words.length) add(words.length >= 3 ? 'error' : 'warning', 'word-per-line', cell, words.length + ' words on ' + lines + ' lines, column of ' + Math.round(cell.getBoundingClientRect().width) + ' px');
      else if (lines >= 10 && words.length / lines < 3) add('warning', 'squeezed', cell, words.length + ' words on ' + lines + ' lines, column of ' + Math.round(cell.getBoundingClientRect().width) + ' px');
      let widest = 0;
      const nodes = doc.createTreeWalker(cell, win.NodeFilter.SHOW_TEXT);
      while (nodes.nextNode()) {
        const parent = nodes.currentNode.parentElement;
        if (!shown(parent)) continue;
        for (const word of nodes.currentNode.nodeValue.split(/\s+/).filter(Boolean)) {
          // the pieces a browser may break it into by itself (a path after its /, a compound after its -)
          for (const piece of word.split(/(?<=[\/\-?])/)) {
            const size = wordWidth(piece, parent);
            widest = Math.max(widest, size);
            if (piece.length <= 30 && size > room + 2 && style(parent).whiteSpace !== 'nowrap' && style(cell).whiteSpace !== 'nowrap') add('error', 'word-cut', cell, '"' + piece + '" (' + Math.round(size) + ' px) in a column of ' + Math.round(room) + ' px');
          }
        }
      }
      if (cell.colSpan === 1) {
        const column = columns.get(cell.cellIndex) || { width: 0, text: 0, wraps: false };
        column.width = Math.max(column.width, cell.getBoundingClientRect().width);
        const lineSpan = new Map(); // the width of each line: its pieces (a key cap, a chip) together
        for (const rect of rects) {
          const key = Math.round((rect.top + rect.bottom) / 8);
          const span = lineSpan.get(key) || [rect.left, rect.right];
          lineSpan.set(key, [Math.min(span[0], rect.left), Math.max(span[1], rect.right)]);
        }
        const whole = [...lineSpan.values()].reduce((most, span) => Math.max(most, span[1] - span[0]), 0);
        column.text = Math.max(column.text, whole, widest);
        column.wraps = column.wraps || lines > 1;
        columns.set(cell.cellIndex, column);
      }
    }
    const list = [...columns.values()];
    const spans = table.querySelector('[colspan]:not([colspan="1"])') !== null; // a column widened by a cell over several of them: told nothing
    if (!spans && list.some(column => column.wraps)) for (const [at, column] of columns) if (!column.wraps && column.width > 140 && column.text < column.width * 0.45) add('warning', 'wide-column', table, 'column ' + (at + 1) + ' is ' + Math.round(column.width) + ' px wide for ' + Math.round(column.text) + ' px of text, next to one that wraps');
  }

  // Fields: the text typed in them
  for (const field of doc.querySelectorAll('input:not([type=checkbox]):not([type=radio]):not([type=range]):not([type=color]):not([type=hidden]), select, textarea')) {
    if (!shown(field)) continue;
    const color = parseColor(style(field).color), under = backgroundOf(field);
    if (color && under && contrast(over(color, under), under) < 4.5) add('warning', 'contrast', field, 'field: ' + contrast(over(color, under), under).toFixed(2) + ':1 (' + hex(color) + ' on ' + hex(under) + ')');
  }
  return problems;
}

function load(frame, url) {
  return new Promise(done => {
    const timer = setTimeout(() => done(false), 30000);
    frame.onload = () => { clearTimeout(timer); done(true); };
    frame.src = url;
  });
}

async function checkState(frame, results, page, width, state) {
  const win = frame.contentWindow, doc = frame.contentDocument;
  await wait(120);
  try { results.push({ page, width, theme: THEME, state, height: doc.documentElement.scrollHeight, problems: audit(win, doc) }); }
  catch (error) { results.push({ page, width, theme: THEME, state, problems: [{ level: 'error', kind: 'check', where: '', text: '', detail: String(error) }] }); }
}

async function main() {
  const results = [];
  for (const page of PAGES) for (const width of WIDTHS) {
    const frame = document.createElement('iframe');
    frame.style.cssText = 'border:0;width:' + width + 'px;height:900px;color-scheme:' + THEME;
    document.body.replaceChildren(frame);
    const separator = page.includes('?') ? '&' : '?';
    if (!await load(frame, page + separator + 'theme=' + THEME + '&lang=en&level=simple')) {
      results.push({ page, width, theme: THEME, state: '', problems: [{ level: 'error', kind: 'check', where: '', text: '', detail: 'the page did not load' }] });
      continue;
    }
    const win = frame.contentWindow, doc = frame.contentDocument;
    if (doc.documentElement.hasAttribute('data-theme') || doc.querySelector('#themeToggle')) doc.documentElement.setAttribute('data-theme', THEME);
    const extra = doc.createElement('style');
    extra.textContent = '.ctab-panel, [role=tabpanel] { display: block !important; } html { scroll-behavior: auto !important; }';
    doc.head.appendChild(extra);
    doc.querySelectorAll('details').forEach(item => { item.open = true; });
    if (doc.fonts && doc.fonts.ready) await doc.fonts.ready;
    await wait(400);
    frame.style.height = Math.min(doc.documentElement.scrollHeight + 40, 24000) + 'px';
    win.dispatchEvent(new win.Event('resize'));
    await wait(250);
    const langs = doc.querySelector('[data-set-lang="fr"]') ? ['en', 'fr'] : [''];
    const levels = doc.querySelector('[data-set-level="technical"]') ? ['simple', 'technical'] : [''];
    for (const lang of langs) for (const level of levels) {
      if (lang) doc.querySelector('[data-set-lang="' + lang + '"]').click();
      if (level) doc.querySelector('[data-set-level="' + level + '"]').click();
      await wait(250);
      await checkState(frame, results, page, width, [lang, level].filter(Boolean).join(' '));
    }
    for (const selector of CLICKS) { // what only shows after a click: a menu, a popover, a step
      const target = doc.querySelector(selector);
      if (!target) continue;
      target.click();
      await wait(300);
      await checkState(frame, results, page, width, 'after ' + selector);
    }
  }
  await fetch('/__result__', { method: 'POST', body: JSON.stringify(results) });
  document.body.textContent = 'done';
}
main().catch(error => fetch('/__result__', { method: 'POST', body: JSON.stringify([{ page: '', width: 0, theme: THEME, state: '', problems: [{ level: 'error', kind: 'check', where: '', text: '', detail: String(error) }] }]) }));
</script></body></html>
"""

SHOT_HEIGHT_MAX = 12000 # px of a screenshot: its page, cut at that

PREFS = """user_pref("layout.css.prefers-color-scheme.content-override", {scheme});
user_pref("ui.systemUsesDarkTheme", {dark});
user_pref("ui.prefersReducedMotion", 1);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.aboutwelcome.enabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("app.update.enabled", false);
user_pref("browser.startup.page", 0);
user_pref("privacy.sanitize.sanitizeOnShutdown", false);
"""

class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    harness = ""
    result = None
    done = None

class Handler(http.server.SimpleHTTPRequestHandler):
    server: Server

    def do_GET(self) -> None:
        if self.path.startswith("/__check__"):
            data = self.server.harness.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        super().do_GET()

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        self.server.result = json.loads(self.rfile.read(length).decode("utf-8"))
        self.send_response(204)
        self.end_headers()
        self.server.done.set()

    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, format: str, *args: object) -> None:
        pass

def new_profile(theme: str) -> str:
    profile = tempfile.mkdtemp(prefix="check-render-")
    Path(profile, "user.js").write_text(PREFS.format(scheme=0 if theme == "dark" else 1, dark=1 if theme == "dark" else 0), encoding="utf-8")
    return profile

def stop(process: subprocess.Popen) -> None:
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except OSError:
        pass
    process.wait()

def browse(server: Server, url: str, theme: str, timeout: int) -> list:
    """The results of one run of the harness, in one browser (its theme is a setting of its profile)."""
    profile = new_profile(theme)
    server.result, server.done = None, threading.Event()
    env = dict(os.environ, MOZ_HEADLESS="1", MOZ_HEADLESS_WIDTH="1400", MOZ_HEADLESS_HEIGHT="1000")
    process = subprocess.Popen(["firefox", "--headless", "--no-remote", "--profile", profile, url], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    try:
        server.done.wait(timeout)
    finally:
        stop(process)
        shutil.rmtree(profile, ignore_errors=True)
    return server.result if server.result is not None else []

def shoot(url: str, out: Path, width: int, height: int, theme: str) -> None:
    profile = new_profile(theme)
    process = subprocess.Popen(["firefox", "--headless", "--no-remote", "--profile", profile, f"--window-size={width},{height}",
                                "--screenshot", str(out), url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    try:
        process.wait(90)
    except subprocess.TimeoutExpired:
        sys.stderr.write(f"no screenshot: {out}\n")
    finally:
        stop(process)
        shutil.rmtree(profile, ignore_errors=True)

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pages", nargs="+")
    ap.add_argument("--widths", default="1280,768,375")
    ap.add_argument("--themes", default="light,dark")
    ap.add_argument("--click", nargs="*", default=[], help="selectors clicked one after the other, the page measured after each")
    ap.add_argument("--shots")
    ap.add_argument("--max", type=int, default=12, help="problems told per run")
    ap.add_argument("--timeout", type=int, default=0, help="seconds, 0 = from the number of pages")
    a = ap.parse_args()
    if shutil.which("firefox") is None:
        sys.stderr.write("firefox is missing\n")
        return 2

    pages = [page.partition("?") for page in a.pages]
    paths = [Path(page[0]).resolve() for page in pages]
    root = Path(os.path.commonpath([str(path.parent) for path in paths]))
    server = Server(("127.0.0.1", 0), functools.partial(Handler, directory=str(root)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{server.server_address[1]}/"
    urls = {base + urllib.parse.quote(str(path.relative_to(root))) + page[1] + page[2]: path for page, path in zip(pages, paths)}
    stems = [path.stem for path in paths]
    widths = [int(width) for width in a.widths.split(",")]
    errors, warnings, failed = 0, 0, False
    heights = {}

    for theme in a.themes.split(","):
        server.harness = HARNESS.replace("%PAGES%", json.dumps(list(urls))).replace("%WIDTHS%", json.dumps(widths)).replace("%THEME%", theme).replace("%CLICKS%", json.dumps(a.click))
        results = browse(server, base + "__check__.html", theme, a.timeout or 90 + 30 * len(urls) * len(widths))
        if not results:
            sys.stderr.write(f"error: the browser gave no result ({theme})\n")
            failed = True
        for run in results:
            heights[(run["page"], run["theme"], run["width"])] = run.get("height", 900)
            label = f"{urls.get(run['page'], run['page'])}: {run['theme']} {run['width']}" + (f" {run['state']}" if run["state"] else "")
            told, kept = set(), []
            for problem in run["problems"]:
                key = (problem["level"], problem["kind"], problem["where"], problem["detail"])
                if key not in told:
                    told.add(key)
                    kept.append(problem)
            count_e = sum(1 for problem in kept if problem["level"] == "error")
            errors += count_e
            warnings += len(kept) - count_e
            print(f"== {label}: {count_e} error(s), {len(kept) - count_e} warning(s)")
            kept.sort(key=lambda problem: problem["level"])
            for problem in kept[:a.max]:
                print(f"  {problem['level']}: {problem['kind']}: {problem['detail']} | {problem['where']} | {problem['text']}")
            if len(kept) > a.max:
                print(f"  ... {len(kept) - a.max} more (--max)")
    if a.shots:
        Path(a.shots).mkdir(parents=True, exist_ok=True)
        for url, path in urls.items():
            for theme in a.themes.split(","):
                for width in widths:
                    height = min(max(heights.get((url, theme, width), 900), 600), SHOT_HEIGHT_MAX)
                    name = path.stem if stems.count(path.stem) == 1 else f"{path.parent.name}-{path.stem}"  # en/page.html, de/page.html: one file each
                    shoot(url + ("&" if "?" in url else "?") + f"theme={theme}", Path(a.shots, f"{name}-{theme}-{width}.png").resolve(), width, height, theme)
    server.shutdown()
    print(f"{errors} error(s), {warnings} warning(s)")
    if failed:
        return 2
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
