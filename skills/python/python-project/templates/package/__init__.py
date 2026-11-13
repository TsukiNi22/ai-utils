{{HEADER:__init__.py:Package {{PACKAGE}}: version and public API}}

##### Import #####
# Import that can't be in the try
from .const import VALUES
from .exception import ProjectError

##### Declaration #####
__version__ = VALUES.VERSION # Read by the packaging (pyproject.toml: dynamic version)
__all__ = ["ProjectError", "__version__"] # Public API of the package
