#!/usr/bin/env python3
"""
Rebuild the navigation of the documentation pages of a folder: runs html-style/scripts/sync_nav.py (shared with
explain-doc, so the html-doc pages and the explanation pages of the same folder link each other).

Usage:
    sync_nav.py [<docs dir>]
"""

import os
import runpy
import sys

SHARED = os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", "..", "html-style", "scripts", "sync_nav.py")

if __name__ == "__main__":
    if not os.path.isfile(SHARED):
        sys.exit("Error: the html-style skill is missing (html-style/scripts/sync_nav.py), install it next to html-doc")
    runpy.run_path(SHARED, run_name="__main__")
