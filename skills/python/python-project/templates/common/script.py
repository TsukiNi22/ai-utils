{{HEADER:{{NAME}}.py:{{DESC}}}}

##### Import #####
# Import that can't be in the try
from const import RETURN, ERROR
from sys import argv, exit, stderr

# Import that can be checked
try:
    from pathlib import Path # Used to read / write the files
    import argparse # Used for the arguments
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(ERROR.FATAL)

##### Tools #####

##### Program #####
parser = argparse.ArgumentParser(description="{{DESC}}")
args = parser.parse_args(argv[1:])

exit(RETURN.OK)
