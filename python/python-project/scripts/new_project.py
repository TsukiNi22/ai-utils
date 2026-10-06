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
##  new_project.py

File Description:
##  Create a Python project in the user's layout: script, application (MAGIC) or installable package
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from sys import argv, exit, stderr

# Import that can be checked
try:
    from importlib.util import spec_from_file_location, module_from_spec # Used to load header.py (cpp-class)
    from datetime import date # Used for the headers
    from pathlib import Path # Used to create the files
    import argparse # Used for the arguments
    import re # Used for the placeholders
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(255)

##### Const #####
SKILL = Path(__file__).resolve().parent.parent
TEMPLATES = SKILL / "templates"
HEADER_SCRIPT = SKILL.parent.parent / "cpp" / "cpp-class" / "scripts" / "header.py"
LAYOUTS = { # kind -> [(template, destination)]
    "script": [("common/script.py", "{{NAME}}.py"), ("common/const.py", "const.py"), ("common/gitignore", ".gitignore")],
    "app": [("app/main.py", "src/main.py"), ("app/app.py", "src/app.py"), ("common/const.py", "src/const.py"), ("common/gitignore", ".gitignore")],
    "package": [("package/__init__.py", "src/{{PACKAGE}}/__init__.py"), ("package/__main__.py", "src/{{PACKAGE}}/__main__.py"),
        ("common/const.py", "src/{{PACKAGE}}/const.py"), ("package/exception.py", "src/{{PACKAGE}}/exception.py"),
        ("package/main.py", "src/{{PACKAGE}}/main.py"), ("package/pyproject.toml", "pyproject.toml"), ("package/test_main.py", "tests/test_main.py"),
        ("common/gitignore", ".gitignore")],
}
FOLDERS = {"script": [], "app": ["src/Class", "src/tool", "data"], "package": []} # empty folders of the layout (with a .gitkeep)

##### Tools #####
def load_header() -> object:
    spec = spec_from_file_location("header", HEADER_SCRIPT)
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def fill(text: str, values: dict[str, str], header: object, banner: str) -> str:
    # {{HEADER:<file>:<description>}} first (it can contain the other placeholders), then {{KEY}}
    for key, value in values.items():
        text = text.replace("{{" + key + "}}", value)
    return re.sub(r"\{\{HEADER:([^:}]+):([^}]*)\}\}", lambda m: header.header(m[1], banner, m[2] or None, "Tsukini", date.today().strftime("%d/%m/%Y"), "py"), text)

##### Program #####
parser = argparse.ArgumentParser(description="Create a Python project in the user's layout")
parser.add_argument("name", help="project name (folder, distribution / command name)")
parser.add_argument("--kind", choices=list(LAYOUTS), default="package", help="script | app (MAGIC layout) | package (installable, pyproject)")
parser.add_argument("--dir", default=".", help="parent folder")
parser.add_argument("--desc", default="...", help="one line description")
parser.add_argument("--python", default="3.12", help="minimal Python version")
parser.add_argument("--version", default="1.0.0", help="first version")
parser.add_argument("--banner", default="xartania", help="banner of the headers: xartania | none | <text>")
args = parser.parse_args(argv[1:])

root = Path(args.dir) / args.name
if root.exists() and any(root.iterdir()):
    stderr.write(f"Error: {root} exists and is not empty\n")
    exit(1)
values = {"NAME": args.name, "PACKAGE": re.sub(r"\W", "_", args.name.lower()), "DESC": args.desc, "PYTHON": args.python, "VERSION": args.version}
header = load_header()
for template, destination in LAYOUTS[args.kind]:
    path = root / fill(destination, values, header, args.banner)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(fill((TEMPLATES / template).read_text(encoding="utf-8"), values, header, args.banner), encoding="utf-8")
    print(f"created {path}")
for folder in FOLDERS[args.kind]:
    (root / folder).mkdir(parents=True, exist_ok=True)
    (root / folder / ".gitkeep").touch()
(root / "README.md").write_text(f"# {args.name}\n\n{args.desc}\n", encoding="utf-8")
if args.kind == "app":
    (root / "requirements.txt").write_text("", encoding="utf-8")
exit(0)
