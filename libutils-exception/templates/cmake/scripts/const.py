"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗ 
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  28/07/2026 by Tsukini

File Name:
##  const.py

File Description:
##  Different const used in the python scripts
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

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

# Check if the program is imported and not call
if __name__ == "__main__":
    stderr.write(f"The {__file__} can only be imported and not executed!\n")
    exit(255) # Special exit code (only place used)

##### Const #####
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
    FATAL: bin  = 0b1000    # Global error, the program whole execution won't be able to run after this         (100% execution stop)
    LOCAL: bin  = 0b100     # Local error, the program local execution won't probably be able to run after this (some chance of execution stop)
    ACTION: bin = 0b10      # Same~~ as Return.KO, a program action execution won't be able to run after this   (low chance of execution stop)

@dataclass(frozen=True)
class Values:
    """
        Different values wihtout a precise category
    """
    # Default values
    EXCEPTION_TYPE: ClassVar[dict[str, int]] = {
        "None":    0b0001, # Forced to exist
        "Fatal":   0b0010,
        "Error":   0b0100,
        "Warning": 0b1000,
    }

    # Limits
    SIZE_MAX: int = 2**64 - 1  # size_t max on 64 bits machine

@dataclass(frozen=True)
class Files:
    """
        Different files path
    """
    REQUIREMENTS: str = "cmake/scripts/requirements.txt"
    CONFIG_EXCEPTION: str = "cmake/config/exceptions/"
    GENERATED_EXCEPTION_HEADER: str = "include/exception/generated_external_exception_header.hpp"

@dataclass(frozen=True)
class Names:
    """
        Different names
    """
    EXCEPTION_SCOPE: str = "utils::exception"
    EXCEPTION_TYPE_ENUM: str = "Type"

    # Sections
    GUARD: str = ""
    CODE_SECTION: str = "ExternalCode"
    MESSAGES_SECTION: str = "ExternalMessages"
    INFO_SECTION: str = "ExternalInfo"
    RESTRICTION_SECTION: str = "ExternalRestriction"

##### Declaration #####
RETURN  = Return()
ERROR   = Error()
VALUES  = Values()
FILES   = Files()
NAMES   = Names()
