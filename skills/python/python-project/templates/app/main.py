{{HEADER:main.py:Main file of the project}}

##### Import #####
# Import that can't be in the try
from const import RETURN, ERROR
from sys import exit, stderr

# Import that can be checked
try:
    from app import app # Main app setup & call of the functions
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(ERROR.FATAL)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(ERROR.FATAL)

##### Program #####
# Call of the main program
ret = app()
if ret != RETURN.OK:
    exit(ret)

# Program end
exit(RETURN.OK)
