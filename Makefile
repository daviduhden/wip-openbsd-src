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
#   make install-pax    install only pax
#   make install-fvwm   install only fvwm
#   make clean      remove build artifacts
#   make obj        create objdirs (pax only; fvwm builds in place)
#   make debug      build each component as <prog>_debug and run it under lldb
#   make test       run the host-side test suite
#   make rebuild    clean and rebuild everything
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

# Recurse into each component, build <prog>_debug and start it under lldb.
debug: _SUBDIRUSE

# Install a single component; the recursive install target would
# otherwise reach both pax and fvwm.  Run as root or via doas.
.for dir in ${SUBDIR}
install-${dir}:
	@echo "===> ${dir} (install)"
	@${MAKE} -C ${.CURDIR}/${dir} ${MAKE_FLAGS} install
.endfor

# Run the host-side test suite.
test:
	@${MAKE} -C ${.CURDIR}/tests test

# Clean and rebuild everything from scratch.
rebuild: clean .WAIT all

.PHONY: debug test rebuild ${SUBDIR:S/^/install-/}
