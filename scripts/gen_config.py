#!/usr/bin/env python3
"""
gen_config.py — Auto-generate EPICS RELEASE and firmware sdkconfig.defaults
from project.conf.

Usage: python3 scripts/gen_config.py [--conf project.conf]
"""
import sys
import os
import argparse

def parse_conf(conf_file):
    """Parse project.conf into a dict, stripping comments and whitespace."""
    cfg = {}
    with open(conf_file) as f:
        for line in f:
            line = line.split('#')[0].strip()
            if '=' in line:
                key, _, val = line.partition('=')
                cfg[key.strip()] = val.strip()
    return cfg

def write_epics_release(cfg, release_file):
    os.makedirs(os.path.dirname(release_file), exist_ok=True)
    with open(release_file, 'w') as f:
        f.write("# Auto-generated from project.conf — do not edit manually\n")
        f.write(f"EPICS_BASE={cfg.get('EPICS_BASE', '')}\n")
        f.write(f"ASYN={cfg.get('ASYN_PATH', '')}\n")
        f.write(f"STREAM={cfg.get('STREAM_PATH', '')}\n")
        f.write("-include $(TOP)/../RELEASE.$(EPICS_HOST_ARCH).local\n")
        f.write("-include $(TOP)/configure/RELEASE.local\n")
    print(f"[gen-config] Written: {release_file}")

def write_sdkconfig_defaults(cfg, sdk_file):
    with open(sdk_file, 'w') as f:
        f.write("# Auto-generated from project.conf — do not edit manually\n")
        f.write(f"CONFIG_ESPTOOLPY_MONITOR_BAUD={cfg.get('FIRMWARE_BAUD', '115200')}\n")
        ssid = cfg.get('WIFI_SSID', '')
        pw   = cfg.get('WIFI_PASSWORD', '')
        tcp  = cfg.get('TCP_PORT', '7070')
        wui  = cfg.get('WEBUI_PORT', '80')
        gpio = cfg.get('ESP32_LED_GPIO', '2')
        f.write(f'CONFIG_PROJECT_WIFI_SSID="{ssid}"\n')
        f.write(f'CONFIG_PROJECT_WIFI_PASSWORD="{pw}"\n')
        f.write(f'CONFIG_PROJECT_TCP_PORT={tcp}\n')
        f.write(f'CONFIG_PROJECT_WEBUI_PORT={wui}\n')
        f.write(f'CONFIG_PROJECT_LED_GPIO={gpio}\n')
    print(f"[gen-config] Written: {sdk_file}")

def write_ioc_env(cfg, env_file):
    os.makedirs(os.path.dirname(env_file), exist_ok=True)
    with open(env_file, 'w') as f:
        f.write("# Auto-generated from project.conf — do not edit manually\n")
        f.write(f'epicsEnvSet("SERIAL_PORT", "{cfg.get("SERIAL_PORT", "/dev/ttyACM0")}")\n')
        f.write(f'epicsEnvSet("ESP32_IP",     "{cfg.get("ESP32_IP", "")}")\n')
        f.write(f'epicsEnvSet("TCP_PORT",     "{cfg.get("TCP_PORT", "7070")}")\n')
        f.write(f'epicsEnvSet("DEVICE",       "ESP32")\n')
    print(f"[gen-config] Written: {env_file}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--conf', default='project.conf')
    args = parser.parse_args()

    if not os.path.exists(args.conf):
        print(f"ERROR: config file '{args.conf}' not found", file=sys.stderr)
        sys.exit(1)

    cfg = parse_conf(args.conf)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(script_dir)

    write_epics_release(cfg, os.path.join(root, 'epics_ioc', 'configure', 'RELEASE'))
    write_sdkconfig_defaults(cfg, os.path.join(root, 'firmware', 'sdkconfig.defaults'))
    write_ioc_env(cfg, os.path.join(root, 'epics_ioc', 'iocBoot', 'iocesp32', 'env_project.cmd'))
    print("[gen-config] Done.")

if __name__ == '__main__':
    main()
