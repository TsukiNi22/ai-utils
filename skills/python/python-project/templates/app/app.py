{{HEADER:app.py:Setup of the app basic things & call of the main functions}}

##### Import #####
# Import that can't be in the try
from const import RETURN, ERROR
from sys import exit, stderr

# Check if the program is imported and not call
if __name__ == "__main__":
    stderr.write(f"The {__file__} can only be imported and not executed!\n")
    exit(ERROR.FATAL)

##### Program #####
def app() -> int:
    # Setup, then the main loop of the application
    return RETURN.OK
