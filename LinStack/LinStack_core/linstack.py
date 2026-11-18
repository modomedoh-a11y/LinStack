#!/usr/bin/env python3
# Copyright 2021 Oliver Smith
# SPDX-License-Identifier: GPL-3.0-or-later
# PYTHON_ARGCOMPLETE_OK
import os
import sys
from pathlib import Path

# Add local directory and standard install directories to sys.path
_current_dir = Path(__file__).resolve().parent
_possible_tools_paths = [
    _current_dir,
    _current_dir / "tools",
    Path("/usr/lib/linstack"),
    Path("/usr/share/linstack"),
    Path("/usr/local/lib/linstack"),
]
for _p in _possible_tools_paths:
    if (_p / "tools").is_dir() or (_p / "__init__.py").is_file():
        if str(_p) not in sys.path:
            sys.path.insert(0, str(_p))
        break

import tools

if __name__ == "__main__":
    os.umask(0o0022)
    sys.exit(tools.main())
