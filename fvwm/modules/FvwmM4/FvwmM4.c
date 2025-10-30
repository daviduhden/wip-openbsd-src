/* This module, and the entire FvwmM4 program, and the concept for
 * interfacing this module to the Window Manager, are all original work
 * by Robert Nation
 *
 * Copyright 1994, Robert Nation
 *  No guarantees or warantees or anything
 * are provided or implied in any way whatsoever. Use this program at your
 * own risk. Permission to use this program for any purpose is given,
 * as long as the copyright is kept intact. */

#define TRUE 1
#define FALSE 0

#include "config.h"

#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include "../../fvwm/module.h"

#include "../../libs/fvwmlib.h"
#include "FvwmM4.h"
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#define Resolution(pixels, mm) ((((pixels) * 100000 / (mm)) + 50) / 100)

char *MyName;
int fd[2];

int ScreenWidth, ScreenHeight;
int Mscreen;

long Vx, Vy;
static char *MkDef(const char *name, const char *def);
static char *MkNum(const char *name, int def);
static char *m4_defs(Display *display, const char *host,
                     char *m4_options, char *config_file);
#define MAXHOSTNAME 255
#define EXTRA 50

int m4_enable;                /* use m4? */
int m4_prefix;                /* Do GNU m4 prefixing (-P) */
char m4_options[BUFSIZ];      /* Command line options to m4 */
char m4_outfile[BUFSIZ] = ""; /* The output filename for m4 */
char *m4_prog = "m4";         /* Name of the m4 program */
int m4_default_quotes;        /* Use default m4 quotes */
char *m4_startquote = "`";    /* Left quote characters for m4 */
char *m4_endquote = "'";      /* Right quote characters for m4 */

/***********************************************************************
 *
 *  Procedure:

 *	main - start of module
 *
 ***********************************************************************/
int main(int argc, char **argv)
{
  Display *dpy; /* which display are we talking to */
  char *temp, *s;
  char *display_name = NULL;
  char *filename = NULL;
  char *tmp_file, read_string[80], delete_string[80];
  int i, m4_debug = 0;

  m4_enable = TRUE;
  m4_prefix = FALSE;
  m4_options[0] = '\0';
  m4_default_quotes = 1;

  /* Record the program name for error messages */
  temp = argv[0];

  s = strrchr(argv[0], '/');
  if (s != NULL)
    temp = s + 1;

  {
    size_t name_len = strlen(temp) + 2;
    MyName = safemalloc(name_len);
    strlcpy(MyName, "*", name_len);
    strlcat(MyName, temp, name_len);
  }

  if (argc < 6)
  {
    fprintf(stderr, "%s Version %s should only be executed by fvwm!\n",
            MyName, VERSION);
    fprintf(stderr, "Wanted argc == 6. Got %d\n", argc);
    exit(1);
  }

  /* Open the X display */
  if (!(dpy = XOpenDisplay(display_name)))
  {
    fprintf(stderr, "%s: can't open display %s", MyName,
            XDisplayName(display_name));
    exit(1);
  }

  Mscreen = DefaultScreen(dpy);
  ScreenHeight = DisplayHeight(dpy, Mscreen);
  ScreenWidth = DisplayWidth(dpy, Mscreen);

  /* We should exit if our fvwm pipes die */
  signal(SIGPIPE, DeadPipe);

  fd[0] = atoi(argv[1]);
  fd[1] = atoi(argv[2]);

  for (i = 6; i < argc; i++)
  {
    if (strcasecmp(argv[i], "-m4-prefix") == 0)
    {
      m4_prefix = TRUE;
    }
    else if (strcasecmp(argv[i], "-m4opt") == 0)
    {
      /* leaving this in just in case-- any option starting with '-'
 	     will get passed on to m4 anyway */
      strlcat(m4_options, argv[++i], sizeof(m4_options));
      strlcat(m4_options, " ", sizeof(m4_options));
    }
    else if (strcasecmp(argv[i], "-m4-squote") == 0)
    {
      m4_startquote = argv[++i];
      m4_default_quotes = 0;
    }
    else if (strcasecmp(argv[i], "-m4-equote") == 0)
    {
      m4_endquote = argv[++i];
      m4_default_quotes = 0;
    }
    else if (strcasecmp(argv[i], "-m4prog") == 0)
    {
      m4_prog = argv[++i];
    }
    else if (strcasecmp(argv[i], "-outfile") == 0)
    {
      strlcpy(m4_outfile, argv[++i], sizeof(m4_outfile));
    }
    else if (strcasecmp(argv[i], "-debug") == 0)
    {
      m4_debug = 1;
    }
    else if (strncasecmp(argv[i], "-", 1) == 0)
    {
      /* pass on any other arguments starting with '-' to m4 */
      strlcat(m4_options, argv[i], sizeof(m4_options));
      strlcat(m4_options, " ", sizeof(m4_options));
    }
    else
      filename = argv[i];
  }

  if (filename != NULL)
  {
    for (i = 0; filename[i] != '\0'; i++)
      if ((filename[i] == '\n') || (filename[i] == '\r'))
      {
        filename[i] = 0;
      }
  }

  if (!(dpy = XOpenDisplay(display_name)))
  {
    fprintf(stderr, "FvwmM4: can't open display %s",
            XDisplayName(display_name));
    exit(1);
  }

  tmp_file = m4_defs(dpy, display_name, m4_options, filename);

  snprintf(read_string, sizeof(read_string), "read %s\n", tmp_file);
  SendInfo(fd, read_string, 0);

  /* For a debugging version, we may wish to omit this part. */
  /* I'll let some m4 advocates clean this up */
  if (!m4_debug)
  {
    snprintf(delete_string, sizeof(delete_string), "exec rm %s\n",
             tmp_file);
    SendInfo(fd, delete_string, 0);
  }
  return 0;
}

static char *m4_defs(Display *display, const char *host,
                     char *m4_options, char *config_file)
{
  Screen *screen;
  Visual *visual;
  char client[MAXHOSTNAME], server[MAXHOSTNAME], *colon;
  char ostype[BUFSIZ];
  char options[BUFSIZ];
  static char tmp_name[BUFSIZ];
  struct hostent *hostname;
  char *vc; /* Visual Class */
  FILE *tmpf;
  int fd;
  struct passwd *pwent;
  /* Generate a temporary filename.  Honor the TMPDIR environment variable,
     if set. Hope nobody deletes this file! */

  if (m4_outfile[0] == '\0')
  {
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir != NULL)
    {
      strlcpy(tmp_name, tmpdir, sizeof(tmp_name));
    }
    else
    {
      strlcpy(tmp_name, "/tmp", sizeof(tmp_name));
    }
    strlcat(tmp_name, "/fvwmrcXXXXXXXXXX", sizeof(tmp_name));
    fd = mkstemp(tmp_name);
    if (fd < 0)
    {
      perror("mkstemp failed in m4_defs");
      exit(0377);
    }
    close(fd);
  }
  else
  {
    strlcpy(tmp_name, m4_outfile, sizeof(tmp_name));
    fd = open(tmp_name, O_WRONLY | O_EXCL | O_CREAT, 0600);
    if (fd < 0)
    {
      perror("exclusive open for output file failed in m4_defs");
      exit(0377);
    }
    close(fd);
  }

  /*
   * Create the appropriate command line to run m4, and
   * open a pipe to the command.
   */

  if (m4_prefix)
    snprintf(options, sizeof(options), "%s --prefix-builtins %s > %s\n",
             m4_prog, m4_options, tmp_name);
  else
    snprintf(options, sizeof(options), "%s  %s > %s\n", m4_prog,
             m4_options, tmp_name);
  tmpf = popen(options, "w");
  if (tmpf == NULL)
  {
    perror("Cannot open pipe to m4");
    exit(0377);
  }

  gethostname(client, MAXHOSTNAME);

  getostype(ostype, sizeof ostype);

  /* Change the quoting characters, if specified */

  if (!m4_default_quotes)
  {
    fprintf(tmpf, "%schangequote(%s, %s)%sdnl\n",
            (m4_prefix) ? "m4_" : "", m4_startquote, m4_endquote,
            (m4_prefix) ? "m4_" : "");
  }

  hostname = gethostbyname(client);
  strlcpy(server, XDisplayName(host), sizeof(server));
  colon = strchr(server, ':');
  if (colon != NULL)
    *colon = '\0';
  if ((server[0] == '\0') || (!strcmp(server, "unix")))
    strlcpy(server, client,
            sizeof(server)); /* must be connected to :0 or unix:0 */

  /* TWM_TYPE is fvwm, for completeness */

  fputs(MkDef("TWM_TYPE", "fvwm"), tmpf);

  /* The machine running the X server */
  fputs(MkDef("SERVERHOST", server), tmpf);
  /* The machine running the window manager process */
  fputs(MkDef("CLIENTHOST", client), tmpf);
  if (hostname)
    fputs(MkDef("HOSTNAME", (char *)hostname->h_name), tmpf);
  else
    fputs(MkDef("HOSTNAME", (char *)client), tmpf);

  fputs(MkDef("OSTYPE", ostype), tmpf);

  pwent = getpwuid(geteuid());
  fputs(MkDef("USER", pwent->pw_name), tmpf);

  fputs(MkDef("HOME", getenv("HOME")), tmpf);
  fputs(MkNum("VERSION", ProtocolVersion(display)), tmpf);
  fputs(MkNum("REVISION", ProtocolRevision(display)), tmpf);
  fputs(MkDef("VENDOR", ServerVendor(display)), tmpf);
  fputs(MkNum("RELEASE", VendorRelease(display)), tmpf);
  screen = ScreenOfDisplay(display, Mscreen);
  visual = DefaultVisualOfScreen(screen);
  fputs(MkNum("WIDTH", DisplayWidth(display, Mscreen)), tmpf);
  fputs(MkNum("HEIGHT", DisplayHeight(display, Mscreen)), tmpf);

  fputs(
    MkNum("X_RESOLUTION", Resolution(screen->width, screen->mwidth)),
    tmpf);
  fputs(
    MkNum("Y_RESOLUTION", Resolution(screen->height, screen->mheight)),
    tmpf);
  fputs(MkNum("PLANES", DisplayPlanes(display, Mscreen)), tmpf);

  fputs(MkNum("BITS_PER_RGB", visual->bits_per_rgb), tmpf);
  fputs(MkNum("SCREEN", Mscreen), tmpf);

  switch (visual->class)
  {
  case (StaticGray):
    vc = "StaticGray";
    break;
  case (GrayScale):
    vc = "GrayScale";
    break;
  case (StaticColor):
    vc = "StaticColor";
    break;
  case (PseudoColor):
    vc = "PseudoColor";
    break;
  case (TrueColor):
    vc = "TrueColor";
    break;
  case (DirectColor):
    vc = "DirectColor";
    break;
  default:
    vc = "NonStandard";
    break;
  }

  fputs(MkDef("CLASS", vc), tmpf);
  if (visual->class != StaticGray && visual->class != GrayScale)
    fputs(MkDef("COLOR", "Yes"), tmpf);
  else
    fputs(MkDef("COLOR", "No"), tmpf);
  fputs(MkDef("FVWM_VERSION", VERSION), tmpf);

  /* Add options together */
  options[0] = '\0';
#ifdef SHAPE
  strlcat(options, "SHAPE ", sizeof(options));
#endif
#ifdef XPM
  strlcat(options, "XPM ", sizeof(options));
#endif

  strlcat(options, "M4 ", sizeof(options));

#ifdef NO_SAVEUNDERS
  strlcat(options, "NO_SAVEUNDERS ", sizeof(options));
#endif

  fputs(MkDef("OPTIONS", options), tmpf);

  fputs(MkDef("FVWM_MODULEDIR", FVWM_MODULEDIR), tmpf);
  fputs(MkDef("FVWM_CONFIGDIR", FVWM_CONFIGDIR), tmpf);

  /*
   * At this point, we've sent the definitions to m4.  Just include
   * the fvwmrc file now.
   */

  fprintf(tmpf, "%sinclude(%s%s%s)\n", (m4_prefix) ? "m4_" : "",
          m4_startquote, config_file, m4_endquote);

  pclose(tmpf);
  return (tmp_name);
}

/***********************************************************************
 *
 *  Procedure:
 *	SIGPIPE handler - SIGPIPE means fvwm is dying
 *
 ***********************************************************************/
void DeadPipe(int nonsense) { exit(0); }

static char *MkDef(const char *name, const char *def)
{
  static char *cp = NULL;
  static int maxsize = 0;
  int needed;
  const char *prefix = m4_prefix ? "m4_define" : "define";
  const char *suffix = m4_prefix ? "m4_" : "";

  needed = snprintf(NULL, 0, "%s(%s,%s%s%s%s%s)%sdnl\n", prefix, name,
                    m4_startquote, m4_startquote, def, m4_endquote,
                    m4_endquote, suffix);
  if (needed < 0)
  {
    perror("MkDef failed to calculate length");
    exit(0377);
  }
  needed += 1; /* account for terminating null */
  if (needed > maxsize)
  {
    char *tmp = realloc(cp, needed);
    if (tmp == NULL)
    {
      perror(
        "MkDef can't allocate enough space for a macro definition");
      free(cp);
      exit(0377);
    }
    cp = tmp;
    maxsize = needed;
  }

  if (snprintf(cp, maxsize, "%s(%s,%s%s%s%s%s)%sdnl\n", prefix, name,
               m4_startquote, m4_startquote, def, m4_endquote,
               m4_endquote, suffix) < 0)
  {
    perror("MkDef failed to build macro definition");
    exit(0377);
  }

  return (cp);
}

static char *MkNum(const char *name, int def)
{
  char num[20];

  snprintf(num, sizeof(num), "%d", def);
  return (MkDef(name, num));
}
