# Makefile at top of application tree
TOP = ..
include $(TOP)/configure/CONFIG

# Avoid races like: `make -j clean install` deleting O.* while compiling.
.NOTPARALLEL: clean install

# Directories to be built, in any order.
DIRS += $(wildcard src* *Src*)

include $(TOP)/configure/RULES_DIRS
