#!/usr/bin/env python3
"""Generate a new paired-build guardian identity only when no token exists."""
from pathlib import Path
import secrets
path=Path(__file__).resolve().parent.parent/'pid1-handoff/reset-token.h'
if path.exists():
    raise SystemExit('Retain the existing identity: changing only one paired component breaks recovery.')
values=','.join('0x'+secrets.token_hex(4)+'U' for _ in range(4))
with path.open('x') as f:
    f.write('/* Identity shared by this guardian, control helper and bootstrap build. */\n#define GW_RESET_TOKEN {'+values+'}\n')
path.chmod(0o600)
print('New paired-build identity generated. Rebuild and qualify all dependent components together.')
