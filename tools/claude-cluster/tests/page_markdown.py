#!/usr/bin/env python3
# A line made to hang a regex in what the web page draws, its own code run by node: a "[" after another, a link never closed... in a display of an agent (markdown, details), the preview of a question,
# a prompt of a session, the word under the cursor: each one read in one pass (40 000 "[" took 5 s at each draw, 80 000 took 33 s); the markdown still does what it did, its text never run.
# Usage: tests/page_markdown.py [ignored]   (needs node; no node: nothing run, ok)
"""A long line never hangs the web page."""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = open(os.path.join(HERE, "..", "templates", "web.html"), encoding="utf-8").read()
if shutil.which("node") is None:
    print("ok   no node here: not run")
    sys.exit(0)

def part(start: str, end: str) -> str:
    at = PAGE.index(start)
    return PAGE[at:PAGE.index(end, at)]

CODE = part("function inlineMarkdown(text)", "function svgOf(") + part("function lastWord(before)", "function context(input)")
PREVIEW = re.search(r"else if \((/\^!.+?/)\.test\(line\.trim\(\)\)", PAGE).group(1)
USER = re.search(r"const re = (/\\\[Image: .+?/g);", PAGE).group(1)
HARNESS = r"""
const vm = require("vm");
const [code, previewSource, userSource] = process.argv.slice(1);
let failed = false;
const check = (name, ok, detail) => { failed = failed || !ok; console.log((ok ? "ok   " : "FAIL ") + name + (!ok && detail !== undefined ? "  (" + JSON.stringify(detail) + ")" : "")); };
const sandbox = {console, document: {createElement: (tag) => ({tag, className: "", text: "", set textContent(t) { this.text = t; }, get outerHTML() { return "<" + this.tag + ">" + this.text + "</" + this.tag + ">"; }})}};
vm.createContext(sandbox);
vm.runInContext(code + "\nthis.page_ = {inlineMarkdown, markdown, lastWord};", sandbox);
const {inlineMarkdown, markdown, lastWord} = sandbox.page_;
const preview = eval(previewSource);
const user = eval(userSource);

// each line drawn in a time that depends on its size alone: 200 000 characters, a few ms each, the worst 0.3 s on a busy machine (the regexes of before: 14 s to 3 minutes)
const LIMIT = 10000;
const N = 200000;
const repeated = (text) => text.repeat(Math.ceil(N / text.length));
const timed = (name, run) => { const start = Date.now(); run(); const took = Date.now() - start; check(name + ": at once", took < LIMIT, took + " ms"); };
for (const shape of ["[", "[a", "[[[]", "]", "](", "[a](", "[a](http://x", "[a](http://", "[](http://x ", "*", "**", "*a", "**a*", "`", "`a", "<", "&", "\\"])
    timed(`inline, a line of ${JSON.stringify(shape)}`, () => inlineMarkdown(repeated(shape)));
timed('inline, "[" and one "]" at its end', () => inlineMarkdown("[".repeat(N) + "]"));
timed("inline, an address never closed", () => inlineMarkdown("[a](http://" + "x".repeat(N)));
timed("inline, an address cut at each step", () => inlineMarkdown("[a](http://" + "x](".repeat(N / 3)));
for (const shape of ["[", "[a](http://x", "- ", "# ", " ", "```", "*"]) timed(`markdown, a line of ${JSON.stringify(shape)}`, () => markdown(repeated(shape)));
timed('markdown, 1000 lines of 2000 "["', () => markdown(("[".repeat(2000) + "\n").repeat(1000)));
timed('the preview of a question, "![" then "]("', () => preview.test("![" + "](".repeat(N / 2)));
timed('the preview of a question, "![a](" then ")"', () => preview.test("![a](" + ")".repeat(N) + "x"));
timed('the preview of a question, a line of "!["', () => preview.test(repeated("![")));
timed('a prompt shown, "[Image: /" again and again', () => repeated("[Image: /").replace(user, ""));
timed('a prompt shown, "@/" again and again', () => repeated("@/").replace(user, ""));
timed("the word under the cursor, a long word before it", () => lastWord("x".repeat(N) + " y"));
timed("the word under the cursor, a long word", () => lastWord("x".repeat(N)));
timed("the word under the cursor, spaces after a backslash", () => lastWord(repeated("\\ ")));

// what it did
const link = (url, text) => `<a href="${url}" target="_blank" rel="noreferrer noopener">${text}</a>`;
const same = (text, wanted) => { const got = inlineMarkdown(text); check(JSON.stringify(text), got === wanted, got); };
same("plain", "plain");
same("[a](http://x)", link("http://x", "a"));
same("[a b](https://x.y/z?a=1&b=2)", link("https://x.y/z?a=1&amp;b=2", "a b"));
same("see [a](http://x) and [b](http://y).", `see ${link("http://x", "a")} and ${link("http://y", "b")}.`);
same("**b** *i* `c`", "<b>b</b> <i>i</i> <code>c</code>");
same("[**b**](http://x)", link("http://x", "<b>b</b>"));
same("[a [b](http://x)", "[a " + link("http://x", "b"));
same("[x](http://[::1]:8080/)", link("http://[::1]:8080/", "x"));
same("[w](https://a.b/Foo_(bar))", link("https://a.b/Foo_(bar", "w") + ")");
same("[[1]](http://x)", "[[1]](http://x)");
same("[a](http://x [b](http://y)", "[a](http://x " + link("http://y", "b"));
same("[a](http://x[b](http://y)", "[a](http://x" + link("http://y", "b"));
same("[x](javascript:alert(1))", "[x](javascript:alert(1))");
same("[x](ftp://a)", "[x](ftp://a)");
same("[](http://x)", "[](http://x)");
same("<script>alert(1)</script>", "&lt;script&gt;alert(1)&lt;/script&gt;");
same("<img src=x onerror=alert(1)>", "&lt;img src=x onerror=alert(1)&gt;");
same('[x](http://a"onclick=alert(1))', link("http://a&quot;onclick=alert(1", "x") + ")");
same("[x](http://a'b)", link("http://a&#39;b", "x"));
same("[<b>](http://x)", link("http://x", "&lt;b&gt;"));
const lines = markdown("# title\n- [one](http://x)\n- two\ntext\n```\n[a](http://x) <b>\n```\n");
check("markdown: its heading, its list, its code as it is", lines === "<h4>title</h4><ul><li>" + link("http://x", "one") + "</li><li>two</li></ul><p>text</p><pre>[a](http://x) <b></pre>", lines);
check("the preview: a picture still seen", preview.test("![alt](path/to/a.png)") && preview.test("![](x(1).png)") && !preview.test("not ![a](b)"));
check("a prompt shown: its picture and its file still found", "[Image: /a/b.png]".match(user)[0] === "[Image: /a/b.png]" && "see @/a/b.txt now".match(user)[0] === "@/a/b.txt");

// the word under the cursor: the same as the regex it replaced
const before = /(?:\\ |[^\s])*$/;
for (const text of ["", " ", "abc", "a b", "a ", "a\\ b", "a\\\\ b", "\\ ", "\\", "a\tb", "a\nb", "a\\\tb", "é è", "@a/b\\ c"]) check(`the word of ${JSON.stringify(text)}`, lastWord(text) === text.match(before)[0], lastWord(text));
let seed = 20261102, differ = 0;
const pick = (n) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 16) % n; };
for (let i = 0; i < 5000; i++) {
    let text = "";
    for (let k = pick(14); k > 0; k--) text += ["a", "\\", " ", "\t", "\n", "é", "/", "@"][pick(8)];
    if (lastWord(text) !== text.match(before)[0]) differ += 1;
}
check("the word of 5000 random texts: the same", differ === 0, differ);
process.exit(failed ? 1 : 0);
"""
try:
    run = subprocess.run(["node", "-e", HARNESS, "--", CODE, PREVIEW, USER], capture_output=True, text=True, timeout=150)
except subprocess.TimeoutExpired:
    print("FAIL the page frozen: not done after 150 s", flush=True)
    sys.exit(1)
print(run.stdout + run.stderr[-1500:], end="", flush=True)
sys.exit(run.returncode)
