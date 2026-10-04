#!/usr/bin/env node
// Build the graph explorer: the graph page of the skill without embedded project, which asks the user for a public
// GitHub / GitLab repository (remembered until Ctrl + Shift + F3, or ?repo=owner/name) and builds its graph in the browser.
//
// Usage: node build_explorer.mjs [--out graph.html] [--home https://github.com/TsukiNi22/skills] [--template <html>]
// Published on the gh-pages branch of the skills repository (graph.html + index.html redirecting to it).

import {readFileSync, writeFileSync, mkdirSync} from "node:fs";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const SKILL_DIR = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const argv = process.argv.slice(2);
const opt = (name, def) => { const i = argv.indexOf("--" + name); return i >= 0 ? argv[i + 1] : def; };
const out = resolve(opt("out", "graph.html"));
const home = opt("home", "https://github.com/TsukiNi22/skills");

let html = readFileSync(opt("template", join(SKILL_DIR, "templates", "graph.html")), "utf8");
const json = (o) => JSON.stringify(o).replace(/<\//g, "<\\/");
html = html.replace(/(<script id="graph-config" type="application\/json">)[\s\S]*?(<\/script>)/, (_, a, b) => a + json({project: "Graph explorer", repo: "", branch: "", storageKey: "graph-explorer", explorer: true}) + b);
html = html.replace(/(<script id="graph-snapshot" type="application\/json">)[\s\S]*?(<\/script>)/, (_, a, b) => a + json({meta: {}, nodes: [], links: []}) + b);
html = html.replace(/<nav class="tabs"[\s\S]*?<\/nav>\s*/, ""); // single page
html = html.replace(/ data-repo="\{\{GITHUB_REPO\}\}"/, "");
html = html.replaceAll("{{GITHUB_REPO}}", "").replaceAll("{{VERSION}}", "").replaceAll("{{PROJECT}}", "Graph explorer").replaceAll("{{STORAGE_KEY}}", "graph-explorer").replaceAll("{{REPO_URL}}", home);
mkdirSync(dirname(out), {recursive: true});
writeFileSync(out, html);
console.log("explorer -> " + out);
