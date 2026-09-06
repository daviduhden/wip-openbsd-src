# Copying the local components into OpenBSD checkouts

The standalone build layout is unchanged. The copy helper maps:

| Local source | Destination |
|---|---|
| pax/ | src checkout: bin/pax/ |
| fvwm/ | xenocara checkout: app/fvwm/ |

Use an existing CVS or Git checkout without host provisioning:

```sh
./fetch-src.ksh --list
doas ./fetch-src.ksh --copy-only /usr/src src/bin/pax
doas ./fetch-src.ksh --copy-only /usr/xenocara xenocara/app/fvwm
```

The short names pax and fvwm are accepted too. With no component arguments,
only components belonging to the detected target tree are copied.
The helper rejects the wrong tree, unknown components and symlinked roots.
It does not replace parent directories or neighboring utilities. Old component
directories are recoverable under TREE/.wip-backups/NAME.XXXXXXXX/component;
nested CVS metadata is retained. Sources are made readable to build users.

The repository's Makefiles remain the standalone fork's Makefiles. Copying
files is not validation of a full OpenBSD src/xenocara release build, and
installation prefixes/build integration still need native review.

Without options, the existing interactive bootstrap workflow checks out a
fresh tree and configures the host. Do not use that mode merely to copy
sources into an existing checkout.
