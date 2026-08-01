/* config.h -- OpenBSD fvwm configuration */

#define FVWM_ICONDIR "/usr/X11R6/lib/X11/fvwm/icons"

#define XPM 1
#define SHAPE 1
#define ACTIVEDOWN_BTNS 1
#define INACTIVE_BTNS 1
#define MINI_ICONS 1
#define VECTOR_BUTTONS 1
#define PIXMAP_BUTTONS 1
#define GRADIENT_BUTTONS 1
#define MULTISTYLE 1
#define EXTENDED_TITLESTYLE 1
#define BORDERSTYLE 1
#define USEDECOR 1
#define WINDOWSHADE 1

#define PACKAGE "fvwm"
#define VERSION "2.2.5"

#define HAVE_FCNTL_H 1
#define HAVE_SIGACTION 1
#define HAVE_SIGINTERRUPT 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_WAITPID 1

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef abs
#define abs(a) (((a) >= 0) ? (a) : -(a))
#endif
