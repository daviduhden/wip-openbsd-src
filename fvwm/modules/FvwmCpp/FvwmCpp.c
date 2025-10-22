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
#define FALSE  0

#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#include "../../fvwm/module.h"

#include "FvwmCpp.h"
#include "../../libs/fvwmlib.h"
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#define Resolution(pixels, mm)  ((((pixels) * 100000 / (mm)) + 50) / 100)

char *MyName;
int fd[2];

struct list *list_root = NULL;

int ScreenWidth, ScreenHeight;
int Mscreen;

long Vx, Vy;
static char *MkDef(char *name, char *def);
static char *MkNum(char *name,int def);
static int cpp_process(Display *display, const char *host, char *options,
  const char *config_file, int keep_output);
static int is_cpp_linemarker(const char *line);
static void *xrealloc(void *ptr, size_t size);
#define MAXHOSTNAME 255
#define EXTRA 20

char *cpp_prog = FVWM_CPP;          /* Name of the cpp program */

char cpp_options[BUFSIZ];
char cpp_outfile[BUFSIZ]="";

/***********************************************************************
 *
 *  Procedure:

 *	main - start of module
 *
 ***********************************************************************/
int main(int argc, char **argv)
{
  Display *dpy;			/* which display are we talking to */
  char *temp, *s;
  char *display_name = NULL;
  char *filename = NULL;
  int i, cpp_debug = 0;

  strcpy(cpp_options,"");

  /* Record the program name for error messages */
  temp = argv[0];

  s=strrchr(argv[0], '/');
  if (s != NULL)
    temp = s + 1;

  MyName = safemalloc(strlen(temp)+2);
  strcpy(MyName,"*");
  strcat(MyName, temp);

  if(argc < 6)
    {
      fprintf(stderr,"%s Version %s should only be executed by fvwm!\n",MyName,
	      VERSION);
      exit(1);
    }

  /* Open the X display */
  if (!(dpy = XOpenDisplay(display_name)))
    {
      fprintf(stderr,"%s: can't open display %s", MyName,
	      XDisplayName(display_name));
      exit (1);
    }


  Mscreen= DefaultScreen(dpy);
  ScreenHeight = DisplayHeight(dpy,Mscreen);
  ScreenWidth = DisplayWidth(dpy,Mscreen);

  /* We should exit if our fvwm pipes die */
  signal (SIGPIPE, DeadPipe);

  fd[0] = atoi(argv[1]);
  fd[1] = atoi(argv[2]);

  for(i=6;i<argc;i++)
    {
      /* leaving this in just in case-- any option starting with '-'
         will get passed on to cpp anyway */
      if(strcasecmp(argv[i],"-cppopt") == 0)
	{
	  strlcat(cpp_options, argv[++i], sizeof(cpp_options));
	  strlcat(cpp_options, " ", sizeof(cpp_options));
	}
      else if (strcasecmp(argv[i], "-cppprog") == 0)
	{
	  cpp_prog = argv[++i];
	}
      else if(strcasecmp(argv[i], "-outfile") == 0)
      {
        strlcpy(cpp_outfile,argv[++i],sizeof(cpp_outfile));
      }
      else if(strcasecmp(argv[i], "-debug") == 0)
	{
	  cpp_debug = 1;
	}
      else if (strncasecmp(argv[i],"-",1) == 0)
      {
        /* pass on any other arguments starting with '-' to cpp */
        strlcat(cpp_options, argv[i],sizeof(cpp_options));
        strlcat(cpp_options, " ",sizeof(cpp_options));
      }
      else
	filename = argv[i];
    }

  /* If we don't have a cpp, stop right now.  */
  if (!cpp_prog || cpp_prog[0] == '\0') {
    fprintf(stderr, "%s: no C preprocessor program specified\n", MyName);
    exit(1);
  }

  for(i=0;i<strlen(filename);i++)
    if((filename[i] == '\n')||(filename[i] == '\r'))
      {
	filename[i] = 0;
      }

  if (filename == NULL)
    {
      fprintf(stderr, "%s: no configuration file specified\n", MyName);
      exit(1);
    }

  if (cpp_process(dpy, display_name, cpp_options, filename, cpp_debug) != 0)
    exit(1);

  return 0;
}



static int
cpp_process(Display *display, const char *host, char *cpp_opts,
    const char *config_file, int keep_output)
{
  Screen *screen;
  Visual *visual;
  char client[MAXHOSTNAME], server[MAXHOSTNAME], *colon;
  char ostype[BUFSIZ];
  char feature_opts[BUFSIZ];
  struct hostent *hostname;
  char *vc;
  struct passwd *pwent;
  FILE *cpp_in = NULL;
  FILE *cpp_out = NULL;
  FILE *mirror = NULL;
  int to_child[2];
  int from_child[2];
  pid_t pid;
  int status;
  char command[2 * BUFSIZ];
  char tmp_name[BUFSIZ];
  int created_temp = 0;
  char kept_path[BUFSIZ];
  size_t line_cap = 1024;
  char *linebuf = NULL;
  size_t line_len = 0;
  unsigned char chunk[BUFSIZ];
  size_t nread;
  int appended_newline = 0;

  kept_path[0] = '\0';

  if (cpp_outfile[0] != '\0') {
    mirror = fopen(cpp_outfile, "w");
    if (mirror == NULL)
      fprintf(stderr, "%s: unable to open %s for writing\n", MyName,
          cpp_outfile);
  } else if (keep_output) {
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL)
      tmpdir = "/tmp";
    strlcpy(tmp_name, tmpdir, sizeof(tmp_name));
    strlcat(tmp_name, "/fvwmcppXXXXXXXXXX", sizeof(tmp_name));
    {
      int fd_tmp = mkstemp(tmp_name);
      if (fd_tmp >= 0) {
        mirror = fdopen(fd_tmp, "w");
        if (mirror != NULL) {
          strlcpy(kept_path, tmp_name, sizeof(kept_path));
          created_temp = 1;
        } else {
          close(fd_tmp);
        }
      } else {
        perror("mkstemp failed in cpp_process");
      }
    }
  }

  if (pipe(to_child) == -1 || pipe(from_child) == -1) {
    perror("pipe in cpp_process");
    if (mirror)
      fclose(mirror);
    return -1;
  }

  static int
  is_cpp_linemarker(const char *line)
  {
    const unsigned char *p;

    if (line == NULL)
      return 0;

    p = (const unsigned char *)line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p != '#')
      return 0;
    p++;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '\0' || *p == '\n')
      return 1;
    if (isdigit(*p)) {
      while (isdigit(*p))
        p++;
      while (*p == ' ' || *p == '\t')
        p++;
      if (*p == '\0' || *p == '\n' || *p == '"')
        return 1;
    }
    if (strncmp((const char *)p, "line", 4) == 0)
      return 1;
    return 0;
  }

  static void *
  xrealloc(void *ptr, size_t size)
  {
    void *tmp = realloc(ptr, size);

    if (tmp == NULL) {
      fprintf(stderr, "%s: unable to allocate %zu bytes\n", MyName, size);
      exit(1);
    }
    return tmp;
  }

  snprintf(command, sizeof(command), "%s %s", cpp_prog,
      (cpp_opts != NULL) ? cpp_opts : "");

  pid = fork();
  if (pid == -1) {
    perror("fork in cpp_process");
    close(to_child[0]);
    close(to_child[1]);
    close(from_child[0]);
    close(from_child[1]);
    if (mirror)
      fclose(mirror);
    return -1;
  }

  if (pid == 0) {
    dup2(to_child[0], STDIN_FILENO);
    dup2(from_child[1], STDOUT_FILENO);
    close(to_child[0]);
    close(to_child[1]);
    close(from_child[0]);
    close(from_child[1]);
    execl("/bin/sh", "sh", "-c", command, (char *)NULL);
    _exit(127);
  }

  close(to_child[0]);
  close(from_child[1]);

  cpp_in = fdopen(to_child[1], "w");
  cpp_out = fdopen(from_child[0], "r");
  if (cpp_in == NULL || cpp_out == NULL) {
    perror("fdopen in cpp_process");
    if (cpp_in)
      fclose(cpp_in);
    else
      close(to_child[1]);
    if (cpp_out)
      fclose(cpp_out);
    else
      close(from_child[0]);
    if (mirror)
      fclose(mirror);
    waitpid(pid, NULL, 0);
    return -1;
  }

#define WRITE_DEF(name, value)                              \
  do {                                                      \
    char *tmp__ = MkDef((name), (value));                   \
    fputs(tmp__, cpp_in);                                  \
    free(tmp__);                                           \
  } while (0)
#define WRITE_NUM(name, value)                              \
  do {                                                      \
    char *tmp__ = MkNum((name), (value));                   \
    fputs(tmp__, cpp_in);                                  \
    free(tmp__);                                           \
  } while (0)

  gethostname(client, MAXHOSTNAME);
  getostype(ostype, sizeof ostype);

  hostname = gethostbyname(client);
  strlcpy(server, XDisplayName(host), sizeof(server));
  colon = strchr(server, ':');
  if (colon != NULL)
    *colon = '\0';
  if ((server[0] == '\0') || (!strcmp(server, "unix")))
    strlcpy(server, client, sizeof(server));

  WRITE_DEF("TWM_TYPE", "fvwm");
  WRITE_DEF("SERVERHOST", server);
  WRITE_DEF("CLIENTHOST", client);
  if (hostname)
    WRITE_DEF("HOSTNAME", (char *)hostname->h_name);
  else
    WRITE_DEF("HOSTNAME", client);
  WRITE_DEF("OSTYPE", ostype);

  pwent = getpwuid(geteuid());
  if (pwent && pwent->pw_name)
    WRITE_DEF("USER", pwent->pw_name);
  else
    WRITE_DEF("USER", "");

  {
    const char *home = getenv("HOME");
    WRITE_DEF("HOME", (home != NULL) ? home : "");
  }

  WRITE_NUM("VERSION", ProtocolVersion(display));
  WRITE_NUM("REVISION", ProtocolRevision(display));
  WRITE_DEF("VENDOR", ServerVendor(display));
  WRITE_NUM("RELEASE", VendorRelease(display));

  screen = ScreenOfDisplay(display, Mscreen);
  visual = DefaultVisualOfScreen(screen);
  WRITE_NUM("WIDTH", DisplayWidth(display, Mscreen));
  WRITE_NUM("HEIGHT", DisplayHeight(display, Mscreen));
  WRITE_NUM("X_RESOLUTION", Resolution(screen->width, screen->mwidth));
  WRITE_NUM("Y_RESOLUTION", Resolution(screen->height, screen->mheight));
  WRITE_NUM("PLANES", DisplayPlanes(display, Mscreen));
  WRITE_NUM("BITS_PER_RGB", visual->bits_per_rgb);
  WRITE_NUM("SCREEN", Mscreen);

  switch (visual->class) {
  case StaticGray:
    vc = "StaticGray";
    break;
  case GrayScale:
    vc = "GrayScale";
    break;
  case StaticColor:
    vc = "StaticColor";
    break;
  case PseudoColor:
    vc = "PseudoColor";
    break;
  case TrueColor:
    vc = "TrueColor";
    break;
  case DirectColor:
    vc = "DirectColor";
    break;
  default:
    vc = "NonStandard";
    break;
  }

  WRITE_DEF("CLASS", vc);
  if (visual->class != StaticGray && visual->class != GrayScale)
    WRITE_DEF("COLOR", "Yes");
  else
    WRITE_DEF("COLOR", "No");
  WRITE_DEF("FVWM_VERSION", VERSION);

  feature_opts[0] = '\0';
#ifdef SHAPE
  strlcat(feature_opts, "SHAPE ", sizeof(feature_opts));
#endif
#ifdef XPM
  strlcat(feature_opts, "XPM ", sizeof(feature_opts));
#endif
  strlcat(feature_opts, "Cpp ", sizeof(feature_opts));
#ifdef NO_SAVEUNDERS
  strlcat(feature_opts, "NO_SAVEUNDERS ", sizeof(feature_opts));
#endif
  WRITE_DEF("OPTIONS", feature_opts);
  WRITE_DEF("FVWM_MODULEDIR", FVWM_MODULEDIR);
  WRITE_DEF("FVWM_CONFIGDIR", FVWM_CONFIGDIR);

  fprintf(cpp_in, "#include \"%s\"\n", config_file);
  fflush(cpp_in);
  fclose(cpp_in);

#undef WRITE_DEF
#undef WRITE_NUM

  linebuf = safemalloc(line_cap);
  while ((nread = fread(chunk, 1, sizeof(chunk), cpp_out)) > 0) {
    if (mirror)
      fwrite(chunk, 1, nread, mirror);
    for (size_t i = 0; i < nread; i++) {
      if (line_len + 1 >= line_cap) {
        line_cap *= 2;
        linebuf = xrealloc(linebuf, line_cap);
      }
      linebuf[line_len++] = chunk[i];
      if (chunk[i] == '\n') {
        if (line_len >= 2 && linebuf[line_len - 2] == '\\') {
          /* Preserve fvwm's "\" line continuation semantics. */
          line_len -= 2;
          continue;
        }
        linebuf[line_len] = '\0';
        if (!is_cpp_linemarker(linebuf))
          SendInfo(fd, linebuf, 0);
        line_len = 0;
      }
    }
  }

  if (ferror(cpp_out))
    perror("cpp output read");

  if (line_len > 0) {
    if (line_len + 2 >= line_cap) {
      line_cap *= 2;
      linebuf = xrealloc(linebuf, line_cap);
    }
    if (linebuf[line_len - 1] != '\n') {
      linebuf[line_len++] = '\n';
      appended_newline = 1;
    }
    linebuf[line_len] = '\0';
    if (mirror && appended_newline)
      fputc('\n', mirror);
    if (!is_cpp_linemarker(linebuf))
      SendInfo(fd, linebuf, 0);
  }

  free(linebuf);
  fclose(cpp_out);

  if (mirror)
    fclose(mirror);

  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid for cpp");
    status = 1;
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "%s: cpp exited with status %d\n", MyName,
        WEXITSTATUS(status));
    return -1;
  }

  if (created_temp && kept_path[0] != '\0') {
    char msg[BUFSIZ];
    snprintf(msg, sizeof(msg), "Echo %s: preprocessor output kept in %s\n",
        MyName, kept_path);
    SendInfo(fd, msg, 0);
  }

  return 0;
}





/***********************************************************************
 *
 *  Procedure:
 *	SIGPIPE handler - SIGPIPE means fvwm is dying
 *
 ***********************************************************************/
void DeadPipe(int nonsense)
{
  exit(0);
}

static char *MkDef(char *name, char *def)
{
  char *cp = NULL;
  int n;

  /* Get space to hold everything, if needed */

  n = EXTRA + strlen(name) + strlen(def);
  cp = safemalloc(n);

  sprintf(cp, "#define %s %s\n",name,def);

  return(cp);
}

static char *MkNum(char *name,int def)
{
  char num[20];

  sprintf(num, "%d", def);

  return(MkDef(name, num));
}
