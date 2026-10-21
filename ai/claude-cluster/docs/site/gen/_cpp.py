#!/usr/bin/env python3
# =============================================================================
# _cpp.py - a tolerant reader of the C++ tool schemas of Mcp.cpp (no compiler): string literals (adjacent ones joined),
#           calls name(args), brace lists {a, b}, "+" between strings; everything else is an identifier or ignored
# =============================================================================

import re

TOKEN = re.compile(r'''\s+|//[^\n]*|/\*.*?\*/|(?P<str>"(?:[^"\\]|\\.)*")|(?P<id>[A-Za-z_][\w:]*)|(?P<num>-?\d+(?:\.\d+)?)|(?P<p>[(){},+<>.!?:;=&|\[\]*/-])''', re.S)


def unescape(lit: str) -> str:
    body = lit[1:-1]
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c == "\\" and i + 1 < len(body):
            n = body[i + 1]
            out.append({"n": "\n", "t": "\t", '"': '"', "\\": "\\", "'": "'"}.get(n, n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def tokens(text: str) -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []
    for m in TOKEN.finditer(text):
        kind = m.lastgroup
        if kind:
            out.append((kind, m.group(kind)))
    return out


class Parser:
    def __init__(self, toks: list[tuple[str, str]]):
        self.t = toks
        self.i = 0

    def peek(self) -> tuple[str, str]:
        return self.t[self.i] if self.i < len(self.t) else ("end", "")

    def expr(self):
        """One expression up to a top level ',' or a closing bracket: strings joined, a call, a list."""
        parts: list = []
        while True:
            kind, val = self.peek()
            if kind == "end" or val in (",", ")", "}", ";"):
                break
            if kind == "str":
                s = unescape(val)
                self.i += 1
                if parts and isinstance(parts[-1], str):
                    parts[-1] += s
                else:
                    parts.append(s)
            elif val == "{":
                self.i += 1
                parts.append(self.items("}"))
            elif kind == "id" and self.t[self.i + 1:self.i + 2] == [("p", "(")]:
                self.i += 2
                parts.append(("call", val, self.items(")")))
            elif val == "(":
                self.i += 1
                parts.append(self.items(")"))
            elif kind == "id":
                self.i += 1
                parts.append(("id", val))
            else:
                self.i += 1
        if len(parts) == 1:
            return parts[0]
        if parts and all(isinstance(p, str) for p in parts):
            return "".join(parts)
        return parts

    def items(self, close: str) -> list:
        out: list = []
        while True:
            kind, val = self.peek()
            if kind == "end":
                break
            if val == close:
                self.i += 1
                break
            if val == ",":
                self.i += 1
                continue
            before = self.i
            out.append(self.expr())
            if self.i == before:  # a stray closing bracket or ';' of a body we do not read
                self.i += 1
        return out


def find_calls(text: str, name: str) -> list[list]:
    """The argument lists of every call name(...) of a source."""
    toks = tokens(text)
    out = []
    for n in range(len(toks) - 1):
        if toks[n] == ("id", name) and toks[n + 1] == ("p", "("):
            p = Parser(toks)
            p.i = n + 2
            out.append(p.items(")"))
    return out


def definitions(text: str) -> dict:
    """The local constants  Json NAME = expr;  of a source (the shared properties of the tools)."""
    toks = tokens(text)
    out: dict = {}
    for n in range(len(toks) - 3):
        if toks[n][1].split("::")[-1] == "Json" and toks[n + 1][0] == "id" and toks[n + 2] == ("p", "="):
            parser = Parser(toks)
            parser.i = n + 3
            out[toks[n + 1][1]] = parser.expr()
    return out


def brace_lists(text: str, head: int = 3) -> list[list]:
    """Every brace list {"a", "b", "c", Kind::X, ...} that starts with `head` string literals then an identifier (the settings)."""
    toks = tokens(text)
    out: list[list] = []
    for n in range(len(toks) - 2 * head - 1):
        if toks[n] != ("p", "{"):
            continue
        ok = True
        for k in range(head):
            if toks[n + 1 + 2 * k][0] != "str" or toks[n + 2 + 2 * k] != ("p", ","):
                ok = False
                break
        if ok and toks[n + 1 + 2 * head][0] == "id":
            parser = Parser(toks)
            parser.i = n + 1
            out.append(parser.items("}"))
    return out
