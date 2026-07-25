# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023

BLOCKSDS	?= /opt/blocksds/core
BLOCKSDSEXT	?= /opt/blocksds/external

# Build artifacts
# ---------------

INSTALLNAME	:= ntxm

# Tools
# -----

MAKE		:= make
RM		:= rm -rf
CP		:= cp
INSTALL	:= install

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Targets
# -------

.PHONY: all arm7 arm9 clean install format include

all: arm9 arm7 arm9d arm7d include

arm9:
	@+$(MAKE) -f Makefile.arm9 --no-print-directory DEBUG=false

arm7:
	@+$(MAKE) -f Makefile.arm7 --no-print-directory DEBUG=false

arm9d:
	@+$(MAKE) -f Makefile.arm9 --no-print-directory DEBUG=true

arm7d:
	@+$(MAKE) -f Makefile.arm7 --no-print-directory DEBUG=true

clean:
	@echo "  CLEAN"
	@$(RM) $(VERSION_HEADER) lib build

include:
	@mkdir -p dist/nds/include
	@cp -aR include/* dist/nds/include/

install: all
	@echo "  INSTALL $(BLOCKSDSEXT)/$(INSTALLNAME)/"
	$(V)$(RM) $(BLOCKSDSEXT)/$(INSTALLNAME)/
	$(V)$(INSTALL) -d $(BLOCKSDSEXT)/$(INSTALLNAME)/
	$(V)$(CP) -r dist/nds/include dist/nds/lib $(BLOCKSDSEXT)/$(INSTALLNAME)/

format:
	@find include source -iname '*.h' -o -iname '*.cpp' -o -iname '*.c' | xargs clang-format -i
