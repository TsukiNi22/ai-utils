#!/usr/bin/env python3
# A question in the web page, its own code run by node on a small stand-in of a browser: what is written on Other, or in the chat of the question, stays when the page draws it again (it did so at
# every refresh, the text gone with no Enter), its keyboard and its cursor too; it reaches the computer as it is written; nothing drawn again when nothing changed.
# Usage: tests/page_question.py [ignored]   (needs node; no node: nothing run, ok)
"""What is written in a question of the page is never lost."""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = open(os.path.join(HERE, "..", "templates", "web.html"), encoding="utf-8").read()
if shutil.which("node") is None:
    print("ok   no node here: not run")
    sys.exit(0)
at = PAGE.index("const askTyped = {};")
end = PAGE.index("// the preview of an option:", at)
CODE = PAGE[at:end]
HARNESS = r"""
const vm = require("vm");
let failed = false;
const check = (name, ok, detail) => { failed = failed || !ok; console.log((ok ? "ok   " : "FAIL ") + name + (!ok && detail !== undefined ? "  (" + JSON.stringify(detail) + ")" : "")); };
class Node {
    constructor(tag, cls, text) { this.tag = tag; this.className = cls || ""; this.children = []; this.events = {}; this.own = text === undefined ? "" : String(text); this.value = ""; this.dataset = {}; this.style = {};
        this.selectionStart = 0; this.selectionEnd = 0; this.classList = {add() {}, remove() {}, toggle() {}, contains() {return false;}}; }
    get firstChild() { return this.children[0] || null; }
    get textContent() { return this.own + this.children.map((c) => c.textContent).join(" "); }
    set textContent(text) { this.own = text; this.children = []; drawn += 1; }
    append(...nodes) { for (const n of nodes) this.children.push(typeof n === "string" ? new Node("#text", "", n) : n); }
    addEventListener(name, f) { this.events[name] = f; }
    focus() { sandbox.document.activeElement = this; }
    setSelectionRange(a, b) { this.selectionStart = a; this.selectionEnd = b; }
    all(found = []) { for (const c of this.children) { found.push(c); c.all(found); } return found; }
}
let drawn = 0;
const ids = {ask: new Node("div")};
const commands = [];
const sandbox = {console, JSON, Math, Array, Promise, Object, String, setTimeout: (f) => { timers.push(f); return timers.length; }, clearTimeout: (t) => { if (t) timers[t - 1] = null; }, $: (id) => ids[id] || null,
    el: (tag, cls, text) => new Node(tag, cls, text), document: {createElement: (tag) => new Node(tag), createTextNode: (text) => new Node("#text", "", text), activeElement: null}, current: "s1", shownAsk: "",
    cmd: async (c) => { commands.push(c); return {}; }, act: async (c) => { commands.push(c); return {}; }, toast() {}, renderPreview() {}};
const timers = [];
vm.createContext(sandbox);
vm.runInContext(process.argv[1] + "\nthis.draw_ = renderQuestion;", sandbox);
const ask = (other) => ({session: "s1", name: "s", request_id: "r1", index: 0, cursor: 0, picked: [], other, answers: {}, chat: [], chatting: false, review: false,
    questions: [{question: "Which one?", header: "h", options: [{label: "a", description: ""}, {label: "b", description: ""}], multiSelect: false}]});
const field = (kind) => ids.ask.all().find((n) => n.dataset.askField === kind);
sandbox.draw_(ask(""));
check("the question drawn", field("other") !== undefined && field("chat") !== undefined);
const first = drawn;
sandbox.draw_(ask(""));
check("nothing changed: not drawn again", drawn === first, drawn);
let input = field("other");
input.focus();
input.value = "written, no Enter";
input.selectionStart = input.selectionEnd = 5;
input.events.input();
check("nothing sent at each key", commands.length === 0, commands);
timers.filter(Boolean).forEach((f) => f());
check("a moment after: on the computer, not validated", commands.length === 1 && commands[0].action === "other" && commands[0].text === "written, no Enter", commands);
// the state of the computer changes (another device, its answer not back yet): drawn again
const again = ask("");
again.chat = [{role: "assistant", text: "a word"}];
sandbox.draw_(again);
input = field("other");
check("drawn again: still written", input.value === "written, no Enter", input.value);
check("     its keyboard kept", sandbox.document.activeElement === input);
check("     its cursor kept", input.selectionStart === 5 && input.selectionEnd === 5, [input.selectionStart, input.selectionEnd]);
// the chat of the question too
let chat = field("chat");
chat.focus();
chat.value = "a question about it";
chat.events.input();
const third = ask("");
third.chat = [{role: "assistant", text: "another word"}];
sandbox.draw_(third);
chat = field("chat");
check("the chat of the question: still written", chat.value === "a question about it", chat.value);
check("     its keyboard kept", sandbox.document.activeElement === chat);
chat.events.keydown({key: "Enter", preventDefault() {}});
check("     Enter sends it, then empty", commands.some((c) => c.action === "chat" && c.text === "a question about it") && chat.value === "", commands);
// another question: its own field
const next = ask("");
next.index = 0;
next.request_id = "r2";
sandbox.draw_(next);
check("another question: its own Other, empty", field("other").value === "", field("other").value);
process.exit(failed ? 1 : 0);
"""
run = subprocess.run(["node", "-e", HARNESS, "--", CODE], capture_output=True, text=True, timeout=60)
print(run.stdout + run.stderr[-1500:], end="", flush=True)
sys.exit(run.returncode)
