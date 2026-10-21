// Runs the search of src/assets/extra.js against a built search-index.js (node tools/test_search.js <site dir>)
// and checks that typical queries find the expected entry first.
'use strict';
const fs = require('fs');
const vm = require('vm');
const dir = process.argv[2] || '.';
const window = { addEventListener() {}, matchMedia() { return { matches: false, addEventListener() {} }; } };
const root = { classList: { add() {} }, lang: 'en', setAttribute() {} };
const document = { documentElement: root, querySelector() { return null; }, querySelectorAll() { return []; }, addEventListener() {} };
class Observer { observe() {} }
const sandbox = { window, document, location: { search: '' }, URLSearchParams, MutationObserver: Observer, fetch: undefined };
window.window = window;
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(dir + '/search-index.js', 'utf8'), sandbox);
vm.runInContext(fs.readFileSync(__dirname + '/../src/assets/extra.js', 'utf8'), sandbox);
const cases = [
  ['en', 'forq', 'compared with a fork'],
  ['en', 'sesion mode', 'session'],
  ['en', 'session_send', 'session_send'],
  ['en', 'sesend', 'session_send'],
  ['en', 'heavy', 'heavy_run'],
  ['en', 'taxonomy_edit', 'taxonomy_edit'],
  ['en', 'ctrl+k', 'palette'],
  ['en', 'install', 'install'],
  ['fr', 'sous-sesion', 'sous-session'],
  ['fr', 'installation', 'installation'],
];
let failed = 0;
for (const [lang, query, expect] of cases) {
  root.lang = lang;
  let first = null;
  sandbox.window.docsSearch(query, (list) => { first = list[0]; });
  const ok = first && (first.t + ' ' + first.u).toLowerCase().includes(expect.toLowerCase());
  console.log((ok ? 'ok   ' : 'FAIL ') + lang + ' "' + query + '" -> ' + (first ? first.t + ' [' + first.k + ']' : 'nothing'));
  if (!ok) failed++;
}
process.exit(failed ? 1 : 0);
