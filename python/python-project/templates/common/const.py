{{HEADER:const.py:Constants of the project}}

##### Import #####
# Import that can't be in the try
from sys import exit, stderr

# Import that can be checked
try:
    from dataclasses import dataclass
    from typing import ClassVar
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255) # Special exit code (only place used)

##### Const #####
VERSION = "{{VERSION}}" # Version of the project, single source (a literal: read by the packaging without import)

@dataclass(frozen=True)
class Return():
    """
        Return values
    """
    OK: int = 0 # Return value upon success on a call function
    KO: int = 1 # Return value upon fail on a call function

@dataclass(frozen=True)
class Error():
    """
        Error values
    """
    FATAL: int  = 0b1000    # Global error, the whole execution stops                  (100% execution stop)
    LOCAL: int  = 0b100     # Local error, the local execution probably can't go on   (some chance of execution stop)
    ACTION: int = 0b10      # Same~~ as Return.KO, an action of the program fails      (low chance of execution stop)

@dataclass(frozen=True)
class Values:
    """
        Different values without a precise category
    """
    NAME: str = "{{NAME}}" # Name of the project
    VERSION: str = VERSION # Version of the project
    DEFAULTS: ClassVar[dict[str, str]] = {} # Default settings

@dataclass(frozen=True)
class Files:
    """
        Different files path
    """
    DATA: str = "data/" # Data folder of the project

##### Declaration #####
RETURN  = Return()
ERROR   = Error()
VALUES  = Values()
FILES   = Files()
