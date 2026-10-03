#!/usr/bin/env node
// Build (or refresh) the graph page of a project documentation.
//
// Usage: node build_graph.mjs --repo <project path> [--out <project>/docs/graph.html] [--ref HEAD]
//                             [--github owner/repo] [--branch main] [--project name] [--version vX.Y.Z] [--key slug] [--template <html>] [--keep-html]
//
// - reads the sources from the git commit --ref (never the working tree), so the embedded
//   snapshot matches its commit hash;
// - runs the same extractGraph() as the page (copied from the page between the <extractor> markers);
// - writes the page with the config (repo/branch/project) and the snapshot JSON embedded.
// The page is (re)generated from templates/graph.html of the skill; with --keep-html an existing page keeps its
// HTML/CSS (custom texts) and only gets the new script, config and snapshot (the markup must be up to date).

import {execFileSync} from "node:child_process";
import {existsSync, readFileSync, writeFileSync, mkdirSync} from "node:fs";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const SKILL_DIR = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const argv = process.argv.slice(2);
const opt = (name, def) => { const i = argv.indexOf("--" + name); return i >= 0 ? argv[i + 1] : def; };

const repo = resolve(opt("repo", "."));
const ref = opt("ref", "HEAD");
const out = resolve(opt("out", join(repo, "docs", "graph.html")));
const git = (...a) => execFileSync("git", ["-C", repo, ...a], {encoding: "utf8", maxBuffer: 1 << 28});

const sha = git("rev-parse", ref).trim();
const date = git("log", "-1", "--format=%cI", sha).trim();
let github = opt("github", "");
if (!github) {
    try {
        const url = git("remote", "get-url", "origin").trim();
        const m = /github\.com[:/]([^/]+\/[^/.]+?)(\.git)?$/.exec(url);
        if (m) github = m[1];
    } catch { /* no remote */ }
}
let version = opt("version", "");
if (!version) {
    try {
        const m = /project\([^)]*VERSION\s+([0-9.]+)/.exec(git("show", sha + ":CMakeLists.txt"));
        if (m) version = "v" + m[1];
    } catch { /* no CMakeLists.txt */ }
}
const project = opt("project", github ? github.split("/")[1] : repo.split("/").pop());
const branch = opt("branch", "main");
const key = opt("key", project.toLowerCase().replace(/[^a-z0-9]+/g, "-"));

// Sources at the commit (same filter as the page)
const paths = git("ls-tree", "-r", "--name-only", sha).split("\n").filter(p =>
    (/(^|\/)CMakeLists\.txt$/.test(p) || /\.(hpp|hh|hxx|h|tpp|inl|ipp|cpp|cc|cxx|c)$/.test(p)) &&
    !/(^|\/)(build|_deps|third_party|vendor|external|\.git|docs|node_modules)\//.test(p)).slice(0, 2500);
const files = paths.map(path => ({path, text: git("show", sha + ":" + path)}));

// Template & extractor: an existing page keeps its HTML/CSS, but always gets the script of the skill
const skillPage = readFileSync(opt("template", join(SKILL_DIR, "templates", "graph.html")), "utf8");
const lastScript = (h) => { const s = h.lastIndexOf("<script>"); return [s, h.indexOf("</script>", s) + "</script>".length]; };
let html = skillPage;
if (existsSync(out) && argv.includes("--keep-html")) {
    html = readFileSync(out, "utf8");
    const [a, b] = lastScript(html), [c, d] = lastScript(skillPage);
    if (a >= 0 && c >= 0) html = html.slice(0, a) + skillPage.slice(c, d) + html.slice(b);
}
const s = html.indexOf("// <extractor>"), e = html.indexOf("// </extractor>");
if (s < 0 || e < 0) throw new Error("extractor markers not found in the page");
const extractGraph = new Function(html.slice(s, e) + "; return extractGraph;")();
const graph = extractGraph(files);
graph.meta = {sha, date, source: "snapshot", files: files.length, generated: new Date().toISOString()};

// Embed config + snapshot
const json = (o) => JSON.stringify(o).replace(/<\//g, "<\\/");
const config = {project, repo: github, branch, storageKey: key};
html = html.replace(/(<script id="graph-config" type="application\/json">)[\s\S]*?(<\/script>)/, (_, a, b) => a + json(config) + b);
html = html.replace(/(<script id="graph-snapshot" type="application\/json">)[\s\S]*?(<\/script>)/, (_, a, b) => a + json(graph) + b);
html = html.replaceAll("{{GITHUB_REPO}}", github).replaceAll("{{VERSION}}", version).replaceAll("{{PROJECT}}", project).replaceAll("{{STORAGE_KEY}}", key).replaceAll("{{REPO_URL}}", github ? "https://github.com/" + github : "#");
mkdirSync(dirname(out), {recursive: true});
writeFileSync(out, html);
// Tabs of the pages that exist in the same folder (guide / technical / graph)
try { console.log("tabs: " + execFileSync("python3", [join(SKILL_DIR, "scripts", "sync_nav.py"), dirname(out)], {encoding: "utf8"}).trim()); }
catch { console.log("tabs: sync_nav.py not run (python3 missing)"); }

const kinds = {};
for (const n of graph.nodes) kinds[n.kind] = (kinds[n.kind] || 0) + 1;
console.log(`${project} @ ${sha.slice(0, 7)} (${date.slice(0, 10)}): ${files.length} files -> ${graph.nodes.length} nodes, ${graph.links.length} links -> ${out}`);
console.log(Object.entries(kinds).map(([k, v]) => `${k}: ${v}`).join(", "));
