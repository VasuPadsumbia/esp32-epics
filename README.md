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

## Notes

- `configure/RELEASE` includes `configure/RELEASE.local` if present.
- Do **not** commit `configure/RELEASE.local` (it may contain your home directory).
