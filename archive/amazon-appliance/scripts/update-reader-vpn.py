#!/usr/bin/env python3
"""Compatibility entrypoint; all updates now use the complete appliance updater."""
from pathlib import Path
import runpy

if __name__=='__main__':
    runpy.run_path(str(Path(__file__).with_name('update-appliance.py')),run_name='__main__')
