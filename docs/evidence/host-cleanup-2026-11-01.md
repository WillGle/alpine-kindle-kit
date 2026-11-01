# Host workspace cleanup — 2026-11-01

## Result

| Group | Before | After |
|---|---:|---:|
| `work/private/evidence` | 5.8 GB | 2.7 GB |
| `work/archive` | 697 MB | 325 MB |
| `work/` total | 21 GB | 16 GB |

`work/private/evidence` now keeps `pretrial-backup/` and `captures/`
uncompressed because they are build and QEMU roundtrip inputs.

## Archives

Compressed evidence lives in `work/private/evidence/archive-20261101/`:

| Archive | Source directory |
|---|---|
| `phase4a-rescue-20261031.tar.zst` | `work/private/evidence/phase4a-rescue-20261031/` |
| `cleanup.tar.zst` | `work/private/evidence/cleanup/` |
| `phase4b-20261031.tar.zst` | `work/private/evidence/phase4b-20261031/` |
| `phase4c-20261031.tar.zst` | `work/private/evidence/phase4c-20261031/` |

Old experiment trees were compressed to
`work/archive/experiments-20261031.tar.zst`.

Each archive was verified with `zstd -t` and a file-count comparison before
the original directory was removed. `SHA256SUMS` in the archive directory
records the archive hashes. `work/private/hardware-trials/` is retained
uncompressed until the physical acceptance round is closed.
