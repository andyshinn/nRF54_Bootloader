# Repo role

This repo ships as the PlatformIO package `framework-arduinoadafruitnrf54-bootloader`, consumed by [`caveman99/platform-nordicnrf54`](https://github.com/caveman99/platform-nordicnrf54). The root `package.json` is the PlatformIO package manifest — keep `name`, `version`, `system`, and `url` accurate.

# Hex layout contract

`builder/frameworks/arduino/adafruit.py` in the platform repo resolves the DFU bootloader hex with:

```
{BOOTLOADER_DIR}/release/{variant}_bootloader.hex
```

`{variant}` matches `build.variant` in the consuming platform's `boards/<variant>.json` and the directory name under `src/boards/<variant>/` here. Renaming a board means renaming all three in lockstep.

The `release/` directory keeps the released package's source tree (master) free of generated binaries — only release tags carry `release/`. All per-board hex files live alongside each other (filenames already carry the variant prefix), no nested per-board folders.

Note: `bin/` is in `.gitignore` for local-build output (legacy from the upstream Makefile), which is why this uses `release/` instead.

# Release flow

`.github/workflows/githubci.yml` builds a hex per board in matrix and, on `release` events only, runs a `commit-binaries` job that:

1. Checks out the release tag.
2. Drops each `release/<variant>_bootloader.hex` into the worktree.
3. Commits and **force-updates the release tag** so the tag points at the new commit containing the binaries.

Master will not gain the `bin/` tree — that's intentional. Consumers pin to a tag (`#vX.Y.Z`) and get a tree with matching binaries.

If GitHub tag-protection rules are added, `github-actions[bot]` needs permission to force-update release tags or this job will fail.

# Adding a new board

The CI matrix is derived from `src/boards/*`. Adding `src/boards/<new>/` (with the standard `board.h`, `board.cmake`, `board.mk`, `pinconfig.c`) automatically schedules a build for it. A matching `boards/<new>.json` must be added to the platform repo for PlatformIO consumers to use it.
