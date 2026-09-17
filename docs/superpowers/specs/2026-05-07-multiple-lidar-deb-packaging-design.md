# Design — `semitan-height-measure` Debian package build script

**Date:** 2026-05-07
**Author:** Quentin Bossard
**Status:** Approved for implementation

## Goal

Provide a single shell script `build_deb.sh` at the root of the YDLidar-SDK
repository that compiles the `multiple_lidar` binary and produces an installable
`.deb` package for Ubuntu 24.04 (amd64). Installing the package must:

- Place the binary at `/usr/bin/multiple_lidar`.
- Install all `*.service` and `*.target` files from `services/multiple_lidar/`
  into `/etc/systemd/system/`.
- Install the `services/multiple_lidar/readme.md` at
  `/usr/share/doc/semitan-height-measure/readme.md`.
- Run `systemctl daemon-reload` after install so units are immediately
  available.
- On uninstall, stop any active `multiple_lidar_barre_*.target` and reload
  systemd.

## Non-goals

- Cross-compilation (build host = target host = Ubuntu 24.04 amd64).
- Multi-arch package (single architecture per build, autodetected).
- GPG signing of the `.deb`.
- Maintaining a Debian `changelog` (version is derived from git).
- Automatic `lintian` enforcement (called if available, warnings only).
- Modifying the upstream `CMakeLists.txt` of the YDLidar SDK.

## Approach

A self-contained shell script (`build_deb.sh`) drives:

1. CMake out-of-tree build of the `multiple_lidar` target only.
2. Staging of an FHS-compliant directory tree under `build/pkg/`.
3. Generation of `DEBIAN/control`, `DEBIAN/postinst`, `DEBIAN/postrm`.
4. `dpkg-deb --build` to produce the final `.deb`.

This keeps the SDK's CMake configuration untouched. All packaging logic lives
in one shell file that is easy to read, debug, and modify without touching the
build system of the upstream library.

## Package metadata

| Field        | Value                                                                |
|--------------|----------------------------------------------------------------------|
| Package      | `semitan-height-measure`                                             |
| Version      | `git describe --tags --always --dirty` (fallback `0.0.0-<shortsha>`) |
| Architecture | `dpkg --print-architecture` (e.g. `amd64`)                           |
| Section      | `misc`                                                               |
| Priority     | `optional`                                                           |
| Depends      | `libc6, libstdc++6`                                                  |
| Maintainer   | `Quentin Bossard <quentin.bossard@stimio.fr>`                        |
| Description  | Multiple lidar acquisition service for tramway pavement height measurement |

## Package layout (staged in `build/pkg/`)

```
build/pkg/
├── DEBIAN/
│   ├── control          # 0644
│   ├── postinst         # 0755
│   └── postrm           # 0755
├── usr/
│   ├── bin/
│   │   └── multiple_lidar             # 0755
│   └── share/
│       └── doc/
│           └── semitan-height-measure/
│               └── readme.md           # 0644
└── etc/
    └── systemd/
        └── system/
            ├── lidar_1.service         # 0644
            ├── lidar_2.service
            ├── ...                     # all *.service from services/multiple_lidar/
            ├── multiple_lidar_barre_1.target
            ├── multiple_lidar_barre_2.target
            └── multiple_lidar_barre_1_light.target
```

The `etc/systemd/system/` location (rather than `/lib/systemd/system/`) is
chosen because these unit files are project-specific deployment units, not
upstream-provided units. It also matches the path already used in the
existing `services/multiple_lidar/readme.md`.

The script copies every `*.service` and `*.target` it finds in
`services/multiple_lidar/` via glob, so any future unit file added in that
directory is packaged automatically. The script fails fast if the glob expands
to zero files.

## Maintainer scripts

### `postinst`

```bash
#!/bin/bash
set -e
if [ "$1" = "configure" ]; then
    systemctl daemon-reload || true
fi
exit 0
```

No service is enabled or started on install. The administrator chooses which
target to start (`systemctl start multiple_lidar_barre_1.target` etc.). The
`|| true` keeps install working in environments without systemd (chroot,
container).

### `postrm`

```bash
#!/bin/bash
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    systemctl stop 'multiple_lidar_barre_*.target' 2>/dev/null || true
    systemctl daemon-reload || true
fi
exit 0
```

Best-effort stop of any active barre target before the unit files disappear,
followed by a reload so systemd forgets the removed units.

## Script behavior (`build_deb.sh`)

### Preconditions checked at start

- Required commands available: `cmake`, `make`, `g++`, `dpkg-deb`, `git`.
- Run from the repo root (script verifies `services/multiple_lidar/` exists).
- At least one `*.service` present in `services/multiple_lidar/`.

### Steps

1. `set -euo pipefail`.
2. Detect architecture: `ARCH=$(dpkg --print-architecture)`.
3. Detect version: `VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "0.0.0-$(git rev-parse --short HEAD)")`.
4. CMake configure if `build/CMakeCache.txt` is missing: `cmake -S . -B build`.
5. Build only the needed target: `cmake --build build --target multiple_lidar -j"$(nproc)"`.
6. Verify `build/multiple_lidar` exists; abort otherwise.
7. Wipe staging dir: `rm -rf build/pkg && mkdir -p build/pkg/DEBIAN`.
8. Create FHS tree under `build/pkg/`.
9. Copy files (binary, services, targets, readme).
10. Render `DEBIAN/control` from inline heredoc with `@VERSION@` and `@ARCH@`
    substituted.
11. Write `DEBIAN/postinst` and `DEBIAN/postrm` from heredocs and `chmod 0755`.
12. Set permissions on staged files (binary 0755, units 0644, doc 0644).
13. `dpkg-deb --build --root-owner-group build/pkg build/semitan-height-measure_${VERSION}_${ARCH}.deb`.
14. Print the absolute path to the produced `.deb`.
15. Print `dpkg-deb --info` and `dpkg-deb --contents` of the package for
    confirmation.
16. If `lintian` is on PATH, run it on the `.deb` (informational, non-fatal).

### Failure modes

| Condition                              | Behavior                          |
|----------------------------------------|-----------------------------------|
| Missing tool                           | Print which one, exit 1.          |
| Not at repo root                       | Print expected layout, exit 1.    |
| `cmake` / `make` failure               | Inherit cmake exit code, exit.    |
| `build/multiple_lidar` not produced    | Explicit error, exit 1.           |
| No `*.service` found                   | Explicit error, exit 1.           |
| `dpkg-deb --build` failure             | Inherit dpkg-deb exit code, exit. |

## Output

A single file: `build/semitan-height-measure_<version>_<arch>.deb`.

Example: `build/semitan-height-measure_1.2.16-3-g7804cf7_amd64.deb`.

## Usage

```
$ ./build_deb.sh
# ... build output ...
Built: /home/quentin/.../build/semitan-height-measure_1.2.16-3-g7804cf7_amd64.deb

# Install on target Ubuntu 24.04 machine:
sudo dpkg -i semitan-height-measure_*.deb

# Then start a target:
sudo systemctl start multiple_lidar_barre_1.target
```

## Validation plan

1. Run the script on a clean clone of the repo on the dev machine. Expect a
   `.deb` in `build/`.
2. `dpkg-deb --contents build/*.deb` — verify the layout matches the spec.
3. `dpkg-deb --info build/*.deb` — verify metadata (name, version, archi,
   maintainer, description).
4. Install on a fresh Ubuntu 24.04 VM/machine: `sudo dpkg -i build/*.deb`.
5. `systemctl status multiple_lidar_barre_1.target` — must list units as
   loaded (not active).
6. Start a target, verify processes run (`ps aux | grep multiple_lidar`).
7. `sudo apt remove semitan-height-measure` — verify target is stopped and
   files are removed.

## Open points

None — design is final.
