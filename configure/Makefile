TOP=..

include $(TOP)/configure/CONFIG

# Local override files (e.g. RELEASE.local) should not be installed.
# They are included by configure/RELEASE for local builds, and are git-ignored.
CONFIG_INSTALLS := $(filter-out ../RELEASE.local ../RELEASE.$(EPICS_HOST_ARCH).local,$(CONFIG_INSTALLS))

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES
