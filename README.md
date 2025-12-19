# esp32-epics

EPICS IOC + ESP32 firmware project.

This repository contains:

- An EPICS IOC called `espCmd` that talks to an ESP32 over USB serial using **asyn** + **StreamDevice**.
- An ESP-IDF firmware project (under `esp32/`) that implements a simple text command protocol.
- A small, modular Channel Access C++ client:
	- `caClientLib/` (reusable library)
	- `caClientApp/` (CLI tool `caClient`)

The goal is a clean EPICS PV interface (no “type raw commands into one PV” requirement), while still allowing firmware-like convenience commands when needed.

---

## Requirements

Host-side EPICS build requires:

- EPICS Base (tested with EPICS R7)
- asyn
- StreamDevice
- calc

Firmware (optional) requires:

- ESP-IDF installed (use your normal ESP-IDF workflow)

---

## Quick Start (clone → build → run IOC)

### 1) Configure EPICS paths (local-only)

This repo intentionally does **not** commit your machine-specific absolute paths.

Copy the example file and edit it:

```sh
cp configure/RELEASE.local.example configure/RELEASE.local
```

Edit `configure/RELEASE.local` and set absolute paths for:

- `EPICS_BASE`
- `ASYN`
- `STREAMDEVICE`
- `CALCSUPPORT`

Notes:

- `configure/RELEASE` includes `configure/RELEASE.local` when present.
- `configure/RELEASE.local` is ignored by git (`/configure/*.local`).
- EPICS `convertRelease.pl` (checkRelease) expects `EPICS_BASE` to be a literal absolute path.

### 2) Build everything

```sh
make -j
```

### 3) Run the IOC

The IOC boot directory is `iocBoot/iocespCmd/`.

```sh
cd iocBoot/iocespCmd
../../bin/$EPICS_HOST_ARCH/espCmd st.cmd
```

If `EPICS_HOST_ARCH` isn’t set in your shell, you can usually run the binary directly by path (e.g. `../../bin/linux-x86_64/espCmd st.cmd`).

#### Serial device

By default the IOC uses:

- Linux device: `/dev/ttyACM0`
- asyn port name: `vasu-usb`

Change these in `iocBoot/iocespCmd/st.cmd` if your device path differs.

---

## EPICS PV Interface

All records in this IOC use prefix `ESP:` (see `dbLoadRecords(..., "P=ESP:")` in `st.cmd`).

### Identity / firmware info

- `ESP:id` (stringin)
- `ESP:version` (stringin)
- `ESP:num_ai` (longin)
- `ESP:num_bin` (longin)

These are `SCAN=Passive` but some use `PINI=YES`, so you will see a few reads during IOC startup.

### Analog inputs

- Raw analog reads: `ESP:ai0`, `ESP:ai1`, `ESP:ai2`
- Mean values: `ESP:ai0:mean`, `ESP:ai1:mean`, `ESP:ai2:mean`, `ESP:ai3:mean`
- Mean accumulation enable: `ESP:ai0:watch` … `ESP:ai3:watch`

All are `SCAN=Passive` (read when processed / requested by records that process).

### Rate / timing / multiplier

- `ESP:rate` (ai)
- Period setpoint (seconds): `ESP:period` (ao)
- Period readback (us): `ESP:period_us` (longin)
- Period readback (seconds): `ESP:period:rb` (calc)
- Period limits (us): `ESP:period_min_us`, `ESP:period_max_us`

- Multiplier setpoint: `ESP:multiplier` (longout)
- Multiplier readback: `ESP:multiplier:rb` (longin)
- Multiplier limits: `ESP:multiplier_min`, `ESP:multiplier_max`

### PWM / LED

- `ESP:pwm11` (ao)
- `ESP:led` (bo) — onboard LED (GPIO8)

---

## GPIO control

There are two supported styles.

### Style A (recommended): per-pin PVs

For each GPIO pin `N` (generated from a template):

- `ESP:gpioN:dir` (bo) — 0=input, 1=output
- `ESP:gpioN:out` (bo) — drive low/high
- `ESP:gpioN:in`  (bi) — read low/high

Important behavior:

- `ESP:gpioN:in` is `SCAN=Passive` (no background polling).
- To force a fresh read from hardware, process the record:

```sh
caput ESP:gpio15:in.PROC 1
caget ESP:gpio15:in
```

### Style B (firmware-like): general command PVs

Because Channel Access writes can only send one value to one PV, the “two argument” commands are implemented as string PVs.

- `ESP:pin` (stringout) — writes `!pin <gpio> <0|1>`
- `ESP:bo`  (stringout) — writes `!bo  <gpio> <0|1>`

Examples:

```sh
caput -S ESP:pin "15 1"   # GPIO15 output
caput -S ESP:pin "15 0"   # GPIO15 input
caput -S ESP:bo  "15 1"   # GPIO15 high
caput -S ESP:bo  "15 0"   # GPIO15 low
```

---

## Channel Access client (caClient)

This repo builds a small CLI client `caClient` plus a reusable library `caClientLib`.

### Helper script

Use `run.sh` from the repo root:

```sh
./run.sh build
./run.sh rebuild
./run.sh client --help
```

Notes:

- `./run.sh client ...` never builds; it only runs an already-built `caClient`.
- If `caClient` is missing, run `./run.sh build` (or `./run.sh rebuild`).

### Examples

```sh
./run.sh client get led
./run.sh client put led 1

./run.sh client get ai0:mean
./run.sh client monitor ai0:mean --duration 5

./run.sh client get period
./run.sh client put period 0.50
```

If you need to point CA at a non-local IOC, use standard EPICS CA environment variables such as `EPICS_CA_ADDR_LIST` and `EPICS_CA_AUTO_ADDR_LIST`.

---

## Repository Structure

Key folders:

- `espCmdApp/`
	- `Db/` EPICS database files (`espCmd.db`, plus templates/substitutions)
	- `protocol/` StreamDevice protocol (`cmd_response.proto`)
	- `src/` IOC application source
- `iocBoot/iocespCmd/`
	- `st.cmd` IOC startup script
	- `envPaths` runtime environment (`TOP`, `EPICS_BASE`, etc.)
- `caClientLib/` reusable C++ CA client library
- `caClientApp/` CLI CA client application
- `esp32/` ESP-IDF firmware project

Build outputs:

- `bin/$EPICS_HOST_ARCH/` built host binaries
- `lib/$EPICS_HOST_ARCH/` built host libraries

---

## Troubleshooting

### GPIO template load fails (gpio.template not found)

The GPIO PVs are generated by loading `gpio.substitutions`, which references `file "gpio.template"`.
The IOC startup script handles this by `cd`’ing into `espCmdApp/Db` before `dbLoadTemplate`.

### "Why is the device printing output continuously?"

- If you enable asyn/StreamDevice trace in `st.cmd`, you will see serial I/O whenever records process.
- GPIO inputs are configured `SCAN=Passive` so they should not poll continuously.
- Some PVs use `PINI=YES` which triggers a few reads at IOC startup.

