# wip-openbsd-src -- standalone fork of OpenBSD userland utilities
# Targets OpenBSD 7.9 / 8.0-beta.  Binaries replace system versions.
#
# Standalone components built from this directory:
#   pax   -- pax(1) with tar(1)/cpio(1) multicall links
#   fvwm  -- FVWM 2.2.5 with privilege separation and hardening
#
# Targets (via bsd.subdir.mk):
#   make            build everything (default)
#   make pax        build only pax
#   make fvwm       build only fvwm
#   make install    install both components (run as root or via doas)
#   make clean      remove build artifacts
#   make obj        create objdirs (pax only; fvwm builds in place)
#   make debug      build each component and run it under lldb
#
# Requires OpenBSD make(1) and /usr/share/mk.  fvwm additionally
# requires Xenocara/X11 development files.
#
# The ext4fs component is not built here; it stores files at
# OpenBSD source-tree-relative paths for copying into a checkout.

# Toolchain: clang compiles, lldb debugs.
CC =		clang
DEBUGGER =	lldb

SUBDIR = pax fvwm

.include <bsd.subdir.mk>

# Recurse into each component and start the built program under lldb.
debug: _SUBDIRUSE

.PHONY: debug
