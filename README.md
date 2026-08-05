# wip-openbsd-src

Standalone fork of selected OpenBSD userland utilities, compiled
independently of the CVS source tree.  Targets OpenBSD 7.9 and
OpenBSD 8.0-beta.  Produces binaries intended to replace the
system-provided versions.

## Components

### pax (tar + cpio)

POSIX.1-2024 compliant archive tool with pledge(2) and unveil(2)
sandboxing.  Includes tar(1), cpio(1), and pax(1) in a single
multicall binary.

### fvwm

FVWM 2.2.5 window manager with privilege separation (fvwm_exec
helper via imsg(3)), per-process pledge(2)/unveil(2) policies,
and OpenBSD-specific hardening.

## Build

Requires OpenBSD 7.9 or 8.0-beta with X11 development files.

```
$ ./build.ksh all        # build everything
$ ./build.ksh all install  # install to system paths
$ ./build.ksh clean      # remove build artifacts
```

Or using make directly:

```
$ make -C pax
$ make -C fvwm
```

## Security

Every process uses staged pledge(2) and unveil(2) to restrict
system calls and filesystem access to the minimum required.
See source code for per-process policy details.

## License

Components retain their original licenses (BSD, ISC).  New code
is ISC-licensed.  See individual source files for details.
