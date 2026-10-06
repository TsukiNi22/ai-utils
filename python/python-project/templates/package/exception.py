{{HEADER:exception.py:Exception of the project: an error code (Error.*) and a context}}

##### Import #####
# Import that can't be in the try
from .const import ERROR

##### Class #####
class ProjectError(Exception):
    """
        Error of the project, with the code to exit with and a context
    """

    def __init__(self, message: str, code: int = ERROR.ACTION, info: str = "[None]") -> None:
        super().__init__(message)
        self.code = code # ERROR.FATAL | ERROR.LOCAL | ERROR.ACTION
        self.info = info # Context of the error (file, value...)

    def formated(self) -> str:
        # [Error] message -> info (same reading as the libutils exceptions)
        kind = "Fatal" if self.code == ERROR.FATAL else "Error"
        return f"[{kind}] {self.args[0]}" + ("" if self.info == "[None]" else f" -> {self.info}")
