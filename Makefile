# =============================================================================
# Root Makefile — ESP32 EPICS Integration Project
#
# DO NOT EDIT config values here. Edit project.conf instead.
#
# Targets:
#   make all          — build firmware + EPICS IOC
#   make fw-build     — build firmware only
#   make fw-flash     — flash firmware to device
#   make fw-monitor   — open serial monitor
#   make ioc-build    — build EPICS IOC
#   make ioc-run      — start EPICS IOC (blocking)
#   make gen-config   — regenerate EPICS RELEASE + firmware sdkconfig.defaults
#   make verify       — run comprehensive system verification and generate logs
#   make test-fw      — run Unity firmware tests, log to logs/unity/
#   make test-ioc     — run pytest IOC tests, log to logs/pytest/
#   make clean        — clean all build directories
#   make docs         — generate Doxygen + render docs/
# =============================================================================

# Load project configuration
include project.conf
export

# Derived paths
FIRMWARE_DIR := firmware
IOC_DIR      := epics_ioc
LOG_DIR      := logs
FW_BUILD     := $(FIRMWARE_DIR)/build

# Timestamp for log files
TIMESTAMP := $(shell date +%Y%m%d_%H%M%S)

.PHONY: all fw-build fw-flash fw-monitor fw-clean \
        ioc-build ioc-run ioc-clean \
        gen-config \
        test-fw test-ioc \
        clean docs help

# ---- Default target ----
all: gen-config fw-build ioc-build
	@echo ""
	@echo "=== Build complete ==="
	@echo "  Flash firmware : make fw-flash"
	@echo "  Start IOC      : make ioc-run"

# ---- Config generation ---- (this is the key target)
gen-config:
	@echo "[gen-config] Generating RELEASE and sdkconfig.defaults from project.conf..."
	@python3 scripts/gen_config.py --conf project.conf

# ---- Firmware ----
fw-build: gen-config
	@echo "[fw-build] Building ESP32 firmware..."
	bash -c "source $(IDF_PATH)/export.sh && cd $(FIRMWARE_DIR) && idf.py build"

fw-flash: gen-config
	@echo "[fw-flash] Flashing to $(FIRMWARE_PORT)..."
	bash -c "source $(IDF_PATH)/export.sh && cd $(FIRMWARE_DIR) && idf.py -p $(FIRMWARE_PORT) flash"

fw-monitor:
	@echo "[fw-monitor] Monitoring $(FIRMWARE_PORT)..."
	bash -c "source $(IDF_PATH)/export.sh && cd $(FIRMWARE_DIR) && idf.py -p $(FIRMWARE_PORT) monitor"

fw-flash-monitor: gen-config
	bash -c "source $(IDF_PATH)/export.sh && cd $(FIRMWARE_DIR) && idf.py -p $(FIRMWARE_PORT) flash monitor"

fw-clean:
	rm -rf $(FW_BUILD)

# ---- EPICS IOC ----
ioc-build: gen-config
	@echo "[ioc-build] Building EPICS IOC..."
	$(MAKE) -C $(IOC_DIR) CHECK_RELEASE=NO

ioc-run:
	@echo "[ioc-run] Starting EPICS IOC..."
	cd $(IOC_DIR)/iocBoot/iocesp32 && ./st.cmd

ioc-clean:
	$(MAKE) -C $(IOC_DIR) clean

# ---- Tests ----
test-fw: gen-config
	@echo "[test-fw] Running Unity firmware tests..."
	mkdir -p $(LOG_DIR)/unity
	bash -c "source $(IDF_PATH)/export.sh && cd $(FIRMWARE_DIR) && \
	         idf.py -p $(FIRMWARE_PORT) flash monitor 2>&1 | tee ../../$(LOG_DIR)/unity/run_$(TIMESTAMP).log"

test-ioc:
	@echo "[test-ioc] Running pytest EPICS IOC integration tests..."
	mkdir -p $(LOG_DIR)/pytest
	cd $(IOC_DIR)/test && python3 -m pytest test_ioc.py -v 2>&1 | tee ../../$(LOG_DIR)/pytest/run_$(TIMESTAMP).log

# ---- Verification ----
verify: gen-config
	@echo "[verify] Running comprehensive system verification..."
	@./scripts/verify_system.py

# ---- Clean all ----
clean: fw-clean ioc-clean

# ---- Docs ----
docs:
	@echo "[docs] Generating firmware Doxygen..."
	@command -v doxygen >/dev/null 2>&1 && doxygen $(FIRMWARE_DIR)/docs/Doxyfile || \
	  echo "doxygen not installed — skipping firmware docs"
	@echo "[docs] Done. See firmware/docs/html/index.html"

# ---- Help ----
help:
	@grep -E '^[a-zA-Z_-]+:' Makefile | grep -v '^\.' | awk -F: '{print "  make " $$1}'
