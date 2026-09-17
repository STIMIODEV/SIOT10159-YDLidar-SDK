# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

A Stimio fork (`STIMIODEV/YDLidar-SDK`, upstream `YDLIDAR/YDLidar-SDK`) of the vendor C/C++ SDK for
YDLIDAR sensors. Almost everything (`core/`, `src/`, `cmake/`, `doc/`, `python/`, `csharp/`, most of
`examples/`) is upstream vendor code — treat it as third-party and avoid gratuitous edits, since the
fork is expected to stay mergeable with upstream.

The Stimio-specific work is:

| Path | Role |
|---|---|
| `examples/multiple_lidar.cpp` | The product application: one process per GS2 lidar, logging to CSV |
| `services/multiple_lidar/` | systemd units that run one `multiple_lidar` process per device |
| `build_deb.sh` | Builds the `semitan-height-measure` .deb (binary + units) |
| `docs/superpowers/` | Specs and plans for the above |

Use case: measuring tramway pavement height ("semitan") with a bar of GS2 lidars on a Stimio bench.
Commit messages on this fork are conventionally prefixed `QB : `.

## Build and test

```bash
cmake -S . -B build && cmake --build build -j$(nproc)   # whole SDK + all examples
cmake --build build --target multiple_lidar             # just the product binary
sudo cmake --build build --target install               # installs lib, headers, examples to /usr/local
./build_deb.sh                                          # .deb into build/, version from `git describe`
```

Every `examples/*.cpp` and `*.c` is globbed into its own executable named after the file, output to
`build/`. Adding a source file there needs no CMake edit.

Tests are gtest and **off by default** (`BUILD_TEST=OFF`, and they also require `GTest` to be found):

```bash
cmake -S . -B build -DBUILD_TEST=ON && cmake --build build -j$(nproc)
cd build && ctest                       # all
./lidar_test --gtest_filter=Suite.Case  # a single test
```

Other options: `BUILD_SHARED_LIBS` (default OFF — the SDK builds as a static lib),
`BUILD_EXAMPLES` (ON), `BUILD_CSHARP` (OFF, Windows only). The Python/SWIG `add_subdirectory` is
**commented out** in the root `CMakeLists.txt`; `pip install .` / `setup.py` is the path for Python.

There is no linter or formatter configured. `Doxyfile` generates the API docs.

Serial device permissions come from `startup/initenv.sh` (udev rules granting 0666 on the CP210x /
PL2303 / STM32-CDC USB-serial IDs). Run it once per host, or lidar opens fail.

## Architecture

Layered, bottom to top:

- `core/` — platform plumbing: `serial/` (with `impl/unix`, `impl/windows`), `network/`, `base/`
  (timers, threads, locks), `math/`, `json/`. `core/common/` holds the shared vocabulary:
  `ydlidar_def.h` (the `LidarProperty` and `TYPE_*` enums, `LaserScan`/`LaserPoint`),
  `ydlidar_protocol.h` (wire structs), `DriverInterface.h` (the abstract driver contract),
  `ydlidar_help.h` (model-capability predicates like `isGSLidar()`, `hasIntensity()`, plus logging).
- `src/*LidarDriver.{h,cpp}` — one driver per protocol family, each implementing `DriverInterface`:
  `YDlidarDriver` (triangle/TOF serial), `ETLidarDriver` (network TOF), `GSLidarDriver` (GS series —
  what this project uses), `SDMLidarDriver`, `TiaLidarDriver`, `DTSLidarDriver`.
- `src/CYdLidar.{h,cpp}` — the C++ facade. Picks and owns the right driver based on
  `LidarPropLidarType` + `LidarPropDeviceType`, then handles frequency/sample-rate checks, angle
  offset, resampling, reconnect and error description.
- `src/filters/` — post-scan filters implementing `FilterInterface`: `NoiseFilter`,
  `StrongLightFilter` (strategies `FS_1`/`FS_2`, used by `multiple_lidar` to cut the near-field
  "dead view").
- `src/ydlidar_sdk.{h,cpp}` — flat C API wrapping `CYdLidar`, and the surface SWIG binds for
  Python/C#.

**The configuration idiom is a property bag, not setters.** All tuning goes through
`setlidaropt(LidarProp<Name>, &value, sizeof(value))` before `initialize()`. The size argument is
checked, so passing the wrong type silently fails the call — match the enum's documented type in
`core/common/ydlidar_def.h`.

**Lifecycle** (same in C++, C, Python): `os_init()` → `setlidaropt(...)`× → `initialize()` →
`turnOn()` → loop on `doProcessSimple(scan)` while `os_isOk()` → `turnOff()` → `disconnecting()`.

**CMake wiring.** `core/` and `src/` recurse through subdirectories automatically (`subdirlist`), and
each leaf `CMakeLists.txt` just calls `aux_source_directory` + the `add_to_ydlidar_sources` /
`add_to_ydlidar_headers` macros from `cmake/common/ydlidar_base.cmake`, accumulating into the single
`ydlidar_sdk` target built at the root. So a new `.cpp` in an existing directory is picked up for
free; a new *directory* needs its own `CMakeLists.txt` using those macros.

## The `multiple_lidar` application

```
multiple_lidar <serial-number> <index>
```

One process drives exactly one lidar. It enumerates `ydlidar::lidarPortList()`, and for each USB
port configures a `CYdLidar` (fixed 921600 baud, `TYPE_GS`, serial, intensity on, 28 Hz),
`initialize()`s it and reads the serial number via `getDeviceInfo()`; only the port whose SN matches
`argv[1]` is kept and turned on, the rest are disconnected.

Consequences worth knowing before changing this file:

- Serial numbers are decoded from `device_info.serialnum` by adding 48 to each byte (ASCII offset),
  mirroring `CYdLidar.cpp`. `SDK_SNLEN` bytes.
- A lidar already opened by another `multiple_lidar` process answers `getDeviceInfo` with an **empty**
  vector; that is the normal "busy, skip it" path, not an error. This is why N processes probing the
  same bus concurrently still converge on distinct devices.
- Output goes to `/data_lidar/<YYYYmmdd-HHMM>/lidar_<NN>.csv`, `NN` being `argv[2]` zero-padded. The
  directory is created at startup; the run timestamp is fixed at process start, so restarts produce a
  new directory. One CSV row per scan: timestamp, then `angle;range;` per point, flushed each scan.
- `argv[2]` only names the CSV — but the unit files also embed it in `ExecStop=pkill '...'`, so the
  binary's two arguments and the unit's command line must stay in sync.

## systemd units (`services/multiple_lidar/`)

`lidar_<N>.service` hardcodes one device serial number and index; `multiple_lidar_barre_<N>.target`
`Wants=` the six services of that bar. The `*_light` variants are a reduced set (lidars 1, 3, 4, 6 on
bar 1) grouped under `multiple_lidar_barre_1_light.target` — a partially populated bar, not a
different program. Everything is driven at the target level:

```bash
systemctl start multiple_lidar_barre_1.target
systemctl stop  multiple_lidar_barre_1.target
ps -aux -ww | grep multi     # expect one process per service
```

Adding a lidar means: a new `lidar_<N>.service` with its serial number, added to the relevant
`.target`'s `Wants=`. `build_deb.sh` globs `*.service` / `*.target`, so no packaging change is needed.

`services/multiple_lidar/readme.md` documents manual (non-.deb) installation and ships to
`/usr/share/doc/semitan-height-measure/`.
