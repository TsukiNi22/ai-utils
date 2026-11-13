{{HEADER:test_main.py:Unit tests of the main entry point}}

##### Import #####
# Import that can't be in the try
from {{PACKAGE}}.exception import ProjectError
from {{PACKAGE}}.const import RETURN, ERROR
from {{PACKAGE}}.main import main
import pytest

##### Tests #####
def test_main_ok() -> None:
    assert main([]) == RETURN.OK

def test_version(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit):
        main(["--version"])
    assert "{{NAME}}" in capsys.readouterr().out

def test_error_formated() -> None:
    error = ProjectError("Cannot read", ERROR.FATAL, "config.json")
    assert error.formated() == "[Fatal] Cannot read -> config.json"
