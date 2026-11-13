{{HEADER:main.py:Main of {{PACKAGE}}: arguments, run, exit code}}

##### Import #####
# Import that can't be in the try
from .const import RETURN, ERROR, VALUES
from .exception import ProjectError
from sys import argv, stderr

# Import that can be checked
try:
    import argparse # Used for the arguments
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    raise SystemExit(255)

##### Tools #####
def parse(arguments: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog=VALUES.NAME, description="{{DESC}}")
    parser.add_argument("-v", "--version", action="version", version=f"{VALUES.NAME} {VALUES.VERSION}")
    return parser.parse_args(arguments)

##### Program #####
def main(arguments: list[str] | None = None) -> int:
    # Entry point (console script, python -m), returns the exit code
    args = parse(argv[1:] if arguments is None else arguments)
    try:
        pass # The program
    except ProjectError as e:
        stderr.write(e.formated() + "\n")
        return e.code
    except KeyboardInterrupt:
        return ERROR.LOCAL
    return RETURN.OK
