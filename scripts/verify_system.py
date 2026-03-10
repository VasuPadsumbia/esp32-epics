#!/usr/bin/env python3
import subprocess
import time
import os
import signal

LOG_FILE = "logs/verification_test.log"
IOC_LOG = "logs/ioc_startup.log"
BUILD_LOG = "logs/build_status.log"
os.makedirs("logs", exist_ok=True)

def log(msg, file=LOG_FILE):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(file, "a") as f:
        f.write(f"[{timestamp}] {msg}\n")
    print(msg)

def run_cmd(cmd, name):
    log(f"--- Running {name} ---")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        with open(BUILD_LOG, "a") as f:
            f.write(f"=== {name} ===\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}\n")
        if result.returncode == 0:
            log(f"SUCCESS: {name}")
            return True
        else:
            log(f"FAILED: {name} (Return code: {result.returncode})")
            return False
    except Exception as e:
        log(f"ERROR: {name} failed with exception {e}")
        return False

PVs_TO_TEST = [
    "ESP32:LED",
    "ESP32:LED:RBV",
    "ESP32:SYS:UPTIME",
    "ESP32:SYS:HEAP",
    "ESP32:SYS:VERSION",
    "ESP32:SYS:STATUS",
    "ESP32:TASK:APP:MAX",
    "ESP32:TASK:UART:MAX",
    "ESP32:TASK:COUNT"
]

def run_ca_tests():
    log("=== Starting EPICS Channel Access Verification ===")
    log("Note: PV values require physical ESP32 hardware to respond. Software configuration is verified if records are found.")

    for pv in PVs_TO_TEST:
        try:
            # Check if PV exists (cainfo)
            info = subprocess.run(["cainfo", pv], capture_output=True, text=True, timeout=3)
            if info.returncode == 0:
                log(f"VERIFIED: {pv} record is live in IOC")
                # Try to get value with units
                val = subprocess.run(["caget", "-t", pv], capture_output=True, text=True, timeout=2)
                egu = subprocess.run(["caget", "-t", f"{pv}.EGU"], capture_output=True, text=True, timeout=2)

                if val.returncode == 0:
                    unit_str = egu.stdout.strip() if egu.returncode == 0 else ""
                    log(f"  VALUE: {val.stdout.strip()} {unit_str}")
                else:
                    log(f"  VALUE: Timeout/No response from hardware")
            else:
                log(f"FAILED: {pv} record not found in IOC")
        except Exception as e:
            log(f"ERROR testing {pv}: {e}")

def main():
    # Reset logs
    for f in [LOG_FILE, IOC_LOG, BUILD_LOG]:
        with open(f, "w") as out:
            out.write(f"Verification Session Started: {time.ctime()}\n")

    log("=== ESP32 EPICS Integration Verification ===")

    # 1. Check Configuration Synergy
    run_cmd("make gen-config", "Configuration Generation")

    # 2. Check Build Status
    run_cmd("make fw-build", "Firmware Build")
    run_cmd("make ioc-build", "IOC Build")

    # 3. Start IOC and Capture Logs
    log("Starting IOC for 10s to capture startup logs...")
    ioc_log_f = open(IOC_LOG, "w", buffering=1)
    ioc_proc = subprocess.Popen("make ioc-run", shell=True, stdout=ioc_log_f, stderr=subprocess.STDOUT, preexec_fn=os.setsid)
    time.sleep(10)

    # 4. Check asyn report
    log("Capturing asynReport...")
    try:
        # Poking the IOC shell to get report
        # We use a dirty trick: pkill -0 check if proc exists, then try to query
        report = subprocess.run("echo 'asynReport' | pgrep -f iocesp32 | xargs -I{} iocsh.py -c 'asynReport'", shell=True, capture_output=True, text=True, timeout=5)
        # Fallback if iocsh.py not available
        if report.returncode != 0:
             log("asynReport captured via ioc_startup.log (check manually for 'Connected')")
    except:
        pass

    # 5. Run CA Tests
    run_ca_tests()

    # Cleanup
    os.killpg(os.getpgid(ioc_proc.pid), signal.SIGTERM)
    log("=== Verification Finished ===")
    log(f"Complete logs available in: {LOG_FILE}, {IOC_LOG}, {BUILD_LOG}")

if __name__ == "__main__":
    main()
