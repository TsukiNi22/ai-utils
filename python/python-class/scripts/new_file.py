"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  06/10/2026 by Tsukini

File Name:
##  new_file.py

File Description:
##  Scaffold Python files in the user's style (class, abstract, tool, module, exception, const, test), empty bodies
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from sys import argv, exit, stderr

# Import that can be checked
try:
    from importlib.util import spec_from_file_location, module_from_spec # Used to load header.py (cpp-class)
    from datetime import date # Used for the headers
    from pathlib import Path # Used to create / edit the files
    import argparse # Used for the arguments
    import re # Used to parse the signatures
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(255)

##### Const #####
HEADER_SCRIPT = Path(__file__).resolve().parents[3] / "cpp" / "cpp-class" / "scripts" / "header.py"
DEFAULTS = {"int": "0", "float": "0.0", "str": '""', "bool": "False", "bytes": 'b""', "list": "[]", "dict": "{}", "set": "set()",
    "tuple": "()"} # smallest return of an empty body
SIGNATURE = re.compile(r"^\s*(\w+)\s*\(\)\s*(?:->\s*([^#]+?))?\s*(?:#\s*(.*))?$") # parameters taken out before

##### Tools #####
def header(file: str, desc: str, banner: str) -> str:
    spec = spec_from_file_location("header", HEADER_SCRIPT)
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.header(file, banner, desc or None, "Tsukini", date.today().strftime("%d/%m/%Y"), "py")

def snake(name: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()

def parse(specs: str) -> list[tuple[str, str, str, str]]:
    # "name(a: int, b: str = '') -> bool # what it does; other()" -> [(name, params, return, comment)]
    result = []
    for spec in filter(None, (s.strip() for s in specs.split(";"))):
        # Parameters up to the matching parenthesis (a comment can contain parentheses)
        open_at = spec.find("(")
        depth = 0
        close_at = -1
        for i in range(open_at, len(spec)):
            depth += 1 if spec[i] == "(" else -1 if spec[i] == ")" else 0
            if depth == 0:
                close_at = i
                break
        match = SIGNATURE.match(spec[:open_at] + "()" + spec[close_at + 1:]) if open_at > 0 and close_at > 0 else None
        if not match:
            stderr.write(f"Error: invalid signature '{spec}' (name(params) -> type # comment)\n")
            exit(1)
        result.append((match[1], spec[open_at + 1:close_at].strip(), (match[2] or "None").strip(), (match[3] or "").strip()))
    return result

def body(returned: str, indent: str, abstract: bool = False) -> list[str]:
    # Empty body (the logic is written by the user, the behavior is in the comment of the def): pass, or the smallest return
    base = returned.split("[")[0].strip()
    if abstract:
        return [indent + "..."]
    if base == "None":
        return [indent + "pass"]
    if "None" in returned.split("|")[1:] or returned.endswith("| None"):
        return [indent + "return None"]
    if base in DEFAULTS:
        return [indent + f"return {DEFAULTS[base]}"]
    return [indent + "raise NotImplementedError # empty body: written by the user"]

def function(name: str, params: str, returned: str, comment: str, indent: str, method: bool, abstract: bool = False) -> list[str]:
    parameters = ", ".join(filter(None, ["self" if method else "", params]))
    lines = [indent + "@abstractmethod"] if abstract else []
    docstring = method and not name.startswith("_") and comment # public method: docstring, else a trailing comment
    lines.append(f"{indent}def {name}({parameters}) -> {returned}:" + (f" # {comment}" if comment and not docstring else ""))
    if docstring:
        lines += [indent + '    """', indent + f"        {comment[0].upper() + comment[1:]}", indent + '    """']
    return lines + body(returned, indent + "    ", abstract)

def imports(relative: bool, extra: list[str]) -> list[str]:
    # In a package: plain relative imports (an import error is an error of the package); elsewhere the checked try + guard
    if relative:
        return ["##### Import #####", "# Import that can't be in the try", "from .const import RETURN, ERROR"] + extra
    lines = ["##### Import #####", "# Import that can't be in the try", "from const import RETURN, ERROR", "from sys import exit, stderr"]
    if extra:
        lines += ["", "# Import that can be checked", "try:"] + [f"    {line}" for line in extra]
        lines += ["except ImportError as e:", '    stderr.write(f"Import Error ({__file__}): {e}\\n")', "    exit(ERROR.FATAL)"]
    lines += ["", "# Check if the program is imported and not call", 'if __name__ == "__main__":',
        '    stderr.write(f"The {__file__} can only be imported and not executed!\\n")', "    exit(ERROR.FATAL)"]
    return lines

##### Program #####
parser = argparse.ArgumentParser(description="Scaffold Python files in the user's style (empty bodies)")
parser.add_argument("kind", choices=["class", "abstract", "tool", "module", "exception", "const", "test"])
parser.add_argument("name", help="class / function / module / category name")
parser.add_argument("--dir", default=".", help="folder of the file (Class/, tool/, src/<pkg>/, tests/...)")
parser.add_argument("--desc", default="", help="description (header, docstring)")
parser.add_argument("--base", default="", help="parent class (class, exception)")
parser.add_argument("--attributes", default="", help="'name: type = default # comment; ...' (class, const)")
parser.add_argument("--methods", default="", help="'name(params) -> type # comment; ...' (class, abstract, module, test: names)")
parser.add_argument("--relative", action="store_true", help="inside a package: from .const import ..., no __main__ guard")
parser.add_argument("--export", action="store_true", help="add the name to __all__ of the __init__.py of --dir")
parser.add_argument("--banner", default="xartania", help="banner of the header: xartania | none | <text>")
args = parser.parse_args(argv[1:])
folder = Path(args.dir)
folder.mkdir(parents=True, exist_ok=True)
attributes = [a.strip() for a in args.attributes.split(";") if a.strip()]
methods = parse(args.methods)

if args.kind in ("class", "abstract", "tool", "module", "test"):
    file = folder / (f"test_{snake(args.name)}.py" if args.kind == "test" else f"{snake(args.name)}.py")
    if file.exists():
        stderr.write(f"Error: {file} already exists\n")
        exit(1)
    lines = [header(file.name, args.desc, args.banner), ""]
    if args.kind == "test":
        lines += ["##### Import #####", "import pytest", "", "##### Tests #####"]
        for name, _, _, comment in methods or [(snake(args.name), "", "None", "")]:
            lines += [f"def test_{name}() -> None:" + (f" # {comment}" if comment else ""), "    pass", ""]
    elif args.kind in ("tool", "module"):
        functions = methods or [(snake(args.name), "", "None", args.desc)]
        lines += imports(args.relative, []) + ["", "##### Tools #####" if args.kind == "module" else "##### Program #####"]
        for name, params, returned, comment in functions:
            lines += function(name, params, returned, comment, "", False) + [""]
    else:
        abstract = args.kind == "abstract"
        extra = ["from abc import ABC, abstractmethod # Used for the abstract methods"] if abstract else []
        if args.base and (folder / f"{snake(args.base)}.py").exists(): # parent class of the same folder
            extra.insert(0, f"from {'.' if args.relative else ''}{snake(args.base)} import {args.base} # Parent class")
        base = ", ".join(filter(None, [args.base, "ABC" if abstract else ""]))
        lines += imports(args.relative, extra) + ["", "##### Class #####", f"class {args.name}" + (f"({base})" if base else "") + ":",
            '    """', f"        {args.desc or 'What it is'}", '    """', ""]
        lines.append("    def __init__(self) -> None:")
        if args.base:
            lines.append("        super().__init__()")
        for attribute in attributes:
            lines.append(f"        self.{attribute}")
        if not attributes and not args.base:
            lines.append("        pass")
        lines.append("")
        for name, params, returned, comment in methods:
            lines += function(name, params, returned, comment, "    ", True, abstract) + [""]
    file.write_text("\n".join(lines).rstrip("\n") + "\n", encoding="utf-8")
    print(f"created {file}")
elif args.kind == "exception":
    file = folder / "exception.py"
    if not file.exists():
        stderr.write(f"Error: {file} not found (python-project creates it)\n")
        exit(1)
    text = file.read_text(encoding="utf-8").rstrip("\n")
    text += f"\n\nclass {args.name}({args.base or 'ProjectError'}):\n    \"\"\"\n        {args.desc or 'Error of ...'}\n    \"\"\"\n"
    text += "\n    def __init__(self, message: str, info: str = \"[None]\") -> None:\n        super().__init__(message, ERROR.ACTION, info) # code of this error\n"
    file.write_text(text, encoding="utf-8")
    print(f"updated {file}")
else:
    file = folder / "const.py"
    if not file.exists():
        stderr.write(f"Error: {file} not found\n")
        exit(1)
    text = file.read_text(encoding="utf-8")
    block = [f"@dataclass(frozen=True)", f"class {args.name}:", '    """', f"        {args.desc or 'Different values'}", '    """']
    block += [f"    {attribute}" for attribute in attributes] or ["    pass"]
    if "##### Declaration #####" not in text:
        stderr.write(f"Error: no '##### Declaration #####' section in {file}\n")
        exit(1)
    text = text.replace("##### Declaration #####", "\n".join(block) + "\n\n##### Declaration #####", 1).rstrip("\n")
    text += f"\n{args.name.upper()} = {args.name}()\n"
    # Instances aligned on the longest name (RETURN  = Return())
    head, _, declarations = text.partition("##### Declaration #####")
    rows = [line.split("=", 1) for line in declarations.strip("\n").splitlines() if "=" in line]
    width = max(len(name.strip()) for name, _ in rows)
    text = head + "##### Declaration #####\n" + "\n".join(f"{name.strip():<{width}} = {value.strip()}" for name, value in rows) + "\n"
    file.write_text(text, encoding="utf-8")
    print(f"updated {file}")

# Public API of the package
if args.export:
    init = folder / "__init__.py"
    if init.exists() and "__all__" in init.read_text(encoding="utf-8"):
        text = init.read_text(encoding="utf-8")
        text = re.sub(r"__all__ = \[", f'__all__ = ["{args.name}", ', text, count=1)
        text = text.replace("##### Import #####\n# Import that can't be in the try\n",
            f"##### Import #####\n# Import that can't be in the try\nfrom .{snake(args.name)} import {args.name}\n", 1)
        # Imports sorted like the rest of the user's code: longest module first
        lines = text.split("\n")
        start = lines.index("# Import that can't be in the try") + 1
        end = start
        while end < len(lines) and lines[end].startswith("from ."):
            end += 1
        lines[start:end] = sorted(lines[start:end], key=lambda line: -len(line.split()[1]))
        text = "\n".join(lines)
        init.write_text(text, encoding="utf-8")
        print(f"updated {init}")
exit(0)
