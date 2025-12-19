# esp32-epics

This repository contains an EPICS IOC (`espCmd`) and an ESP-IDF firmware project.

## EPICS build (host)

This repo intentionally does **not** commit your machine-specific EPICS paths.
EPICS needs `EPICS_BASE` and support module paths to be **absolute paths**.

### 1) Create `configure/RELEASE.local`

Create a local-only file (it is ignored by git via `.gitignore`):

```sh
cp configure/RELEASE.local.example configure/RELEASE.local
```

Edit `configure/RELEASE.local` and set these paths:

- `EPICS_BASE` (absolute path to your EPICS Base)
- `ASYN`, `STREAMDEVICE`, `CALCSUPPORT` (absolute paths to those support modules)

### 2) Build

```sh
make -j
```

## Channel Access client (host)

This repo also builds a small Channel Access CLI client called `caClient` (plus a reusable C++ library under `caClientLib/`).

### Build and run via helper script

```sh
./run.sh build
./run.sh rebuild
./run.sh client --help
```

Client notes:

- `./run.sh client ...` never builds; it only runs an already-built `caClient`.
- If `caClient` is missing, run `./run.sh build` (or `./run.sh rebuild`).

### Examples (your StreamDevice IOC PVs)

These examples assume your IOC is running and the PVs have the `ESP:` prefix.

```sh
./run.sh client get led
./run.sh client put led 1

# Short names containing ':' are supported (prefix still applies):
./run.sh client get ai0:mean
./run.sh client monitor ai0:mean --duration 5

# PWM + timing PVs from espCmdApp/Db/espCmd.db:
./run.sh client get pwm11
./run.sh client put pwm11 2.50

./run.sh client get period
./run.sh client put period 0.50

./run.sh client get rate
./run.sh client monitor rate --duration 5

# Override prefix (or disable it entirely)
./run.sh client --prefix ESP: get pwm11
./run.sh client --prefix "" get ESP:ai0:mean
```

If you need to point CA at a non-local IOC, set your normal CA env (e.g. `EPICS_CA_ADDR_LIST`, `EPICS_CA_AUTO_ADDR_LIST`).

## Notes

- `configure/RELEASE` includes `configure/RELEASE.local` if present.
- Do **not** commit `configure/RELEASE.local` (it may contain your home directory).
