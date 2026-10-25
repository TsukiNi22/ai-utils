// The search of better-html-style (site.js) run against a built search-index.js: every page found first by its title, and found in
// the first three with a typo in its title; optional cases of the site (a JSON file: [["en", "query", "expected text of the url / title"]]).
//   node test_search.js <site dir> [cases.json]
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const dir = process.argv[2] || '.';
const site = path.join(__dirname, '..', '..', 'better-html-style', 'templates', 'site.js');
const window = { addEventListener() {}, matchMedia() { return { matches: false, addEventListener() {} }; } };
const root = { classList: { add() {} }, lang: 'en', setAttribute() {}, getAttribute() { return null; } };
const document = { documentElement: root, querySelector() { return null; }, querySelectorAll() { return []; }, addEventListener() {} };
class Observer { observe() {} }
const sandbox = { window, document, location: { search: '' }, URLSearchParams, MutationObserver: Observer, fetch: undefined };
window.window = window;
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(path.join(dir, 'search-index.js'), 'utf8'), sandbox);
vm.runInContext(fs.readFileSync(site, 'utf8'), sandbox);
const entries = sandbox.window.DOCS_INDEX.entries;
function search(lang, q) { root.lang = lang; let out = []; sandbox.window.docsSearch(q, (list) => { out = list; }); return out; }
let failed = 0;
function report(ok, label) { console.log((ok ? 'ok   ' : 'FAIL ') + label); if (!ok) failed++; }
for (const e of entries) {
    if (e.u.includes('#') || !e.u.endsWith('.html')) continue;
    const lang = e.l || 'en';
    const list = search(lang, e.t);
    report(list.length && list[0].u === e.u, lang + ' "' + e.t + '" -> ' + (list[0] ? list[0].u : 'nothing'));
    const word = e.t.split(/\s+/).find((w) => w.length >= 5);
    if (word) {
        const typo = e.t.replace(word, word.slice(0, 1) + word.slice(2)); // a letter missing
        const top = search(lang, typo).slice(0, 3).map((x) => x.u);
        report(top.includes(e.u), lang + ' "' + typo + '" (typo) -> ' + (top.join(', ') || 'nothing'));
    }
}
if (process.argv[3]) {
    for (const [lang, query, expect] of JSON.parse(fs.readFileSync(process.argv[3], 'utf8'))) {
        const first = search(lang, query)[0];
        report(first && (first.t + ' ' + first.u).toLowerCase().includes(expect.toLowerCase()), lang + ' "' + query + '" -> ' + (first ? first.t : 'nothing'));
    }
}
process.exit(failed ? 1 : 0);
