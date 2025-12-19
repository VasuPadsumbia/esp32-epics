# Makefile at top of caClientLib
TOP = ..
include $(TOP)/configure/CONFIG

# Avoid races like: `make -j clean install` deleting O.* while compiling.
.NOTPARALLEL: clean install

DIRS += $(wildcard src* *Src*)

include $(TOP)/configure/RULES_DIRS
