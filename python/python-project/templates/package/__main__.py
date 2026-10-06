{{HEADER:__main__.py:Entry point of python -m {{PACKAGE}}}}

##### Import #####
# Import that can't be in the try
from .main import main
from sys import exit

##### Program #####
exit(main())
