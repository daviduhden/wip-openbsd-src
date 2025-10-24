/*
 * *************************************************************************
 * This module is all original code
 * by Rob Nation
 * Copyright 1993, Robert Nation
 *     You may use this code for any purpose, as long as the original
 *     copyright remains in the source code and all documentation
 *
 * Changed 09/24/98 by Dan Espen:
 * - remove logic that processed and saved module configuration commands.
 * Its now in "modconf.c".
 * *************************************************************************
 */
#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/wait.h>

#include "fvwm.h"
#include "menus.h"
#include "misc.h"
#include "parse.h"
#include "screen.h"
#include "module.h"

extern Boolean debugging;

char *fvwm_file = NULL;

int numfilesread = 0;

static int last_read_failed=0;

static const char *read_system_rc_cmd="Read system"FVWMRC;

typedef struct
{
  FILE *stream;
  pid_t pid;
  int fd;
} PipeChild;

#define PIPE_READ_INTERVAL_SEC 1
#define PIPE_READ_MAX_IDLE_LOOPS 10
#define PIPE_REAP_WAIT_USEC 100000
#define PIPE_REAP_ATTEMPTS 20

static int
start_pipe_process(const char *command, PipeChild *child)
{
  int pipe_fd[2];
  pid_t pid;

  if (pipe(pipe_fd) < 0)
    return -1;

  pid = fork();
  if (pid < 0)
    {
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      return -1;
    }

  if (pid == 0)
    {
      close(pipe_fd[0]);
      if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
        _exit(127);
      close(pipe_fd[1]);
      execl("/bin/sh", "sh", "-c", command, (char *)NULL);
      _exit(127);
    }

  close(pipe_fd[1]);
  child->fd = pipe_fd[0];
  child->stream = fdopen(child->fd, "r");
  if (child->stream == NULL)
    {
      close(child->fd);
      kill(pid, SIGTERM);
      (void)waitpid(pid, NULL, 0);
      child->fd = -1;
      return -1;
    }
  fcntl(child->fd, F_SETFD, 1);
  child->pid = pid;
  return 0;
}

static void
stop_pipe_process(PipeChild *child, int timed_out,
                  const char *cmdname, const char *command)
{
  int status;
  int attempt;
  pid_t waited = -1;

  if (child->stream)
    {
      fclose(child->stream);
      child->stream = NULL;
    }

  if (child->fd >= 0)
    child->fd = -1;

  if (child->pid <= 0)
    return;

  if (timed_out)
    {
      fvwm_msg(WARN, cmdname,
               "command '%s' did not close pipe, terminating it", command);
      kill(child->pid, SIGTERM);
    }

  for (attempt = 0; attempt < PIPE_REAP_ATTEMPTS; ++attempt)
    {
      waited = waitpid(child->pid, &status, timed_out ? WNOHANG : 0);
      if (waited == child->pid)
        break;
      if (waited == -1)
        {
          if (errno == EINTR)
            continue;
          if (errno != ECHILD)
            fvwm_msg(ERR, cmdname,
                     "waitpid failed for '%s': %s",
                     command, strerror(errno));
          break;
        }
      if (waited == 0)
        usleep(PIPE_REAP_WAIT_USEC);
    }

  if (timed_out && waited != child->pid)
    {
      kill(child->pid, SIGKILL);
      while ((waited = waitpid(child->pid, &status, 0)) == -1 &&
             errno == EINTR)
        ;
    }

  child->pid = -1;
}


extern void StartupStuff(void);

/*
 * func to do actual read/piperead work
 * Arg 1 is file name to read.
 * Arg 2 (optional) "Quiet" to suppress message on missing file.
 */
static void ReadSubFunc(XEvent *eventp,Window junk,FvwmWindow *tmp_win,
                        unsigned long context, char *action,int* Module,
                        int piperead)
{
  char *filename= NULL,*Home, *home_file, *ofilename = NULL;
  char *option;                         /* optional arg to read */
  char *rest,*tline,line[1024];
  FILE *stream = NULL;
  PipeChild child = { NULL, -1, -1 };
  int thisfileno;
  char missing_quiet;                   /* missing file msg control */
  char *cmdname;
  size_t len;
  int timed_out = 0;
  int idle_loops = 0;

  /* domivogt (30-Dec-1998: I tried using conditional evaluation instead
   * of the cmdname variable ( piperead?"PipeRead":"Read" ), but gcc seems
   * to treat this expression as a pointer to a character pointer, not just
   * as a character pointer, but it doesn't complain either. Or perhaps
   * insure++ gets this wrong? */
  if (piperead)
    cmdname = "PipeRead";
  else
    cmdname = "Read";

  thisfileno = numfilesread;
  numfilesread++;

/*  fvwm_msg(INFO,cmdname,"action == '%s'",action); */

  rest = GetNextToken(action,&ofilename); /* read file name arg */
  if(ofilename == NULL)
  {
    fvwm_msg(ERR, cmdname,"missing parameter");
    last_read_failed = 1;
    return;
  }
  missing_quiet='n';                    /* init */
  rest = GetNextToken(rest,&option);    /* read optional arg */
  if (option != NULL) {                 /* if there is a second arg */
    if (strncasecmp(option,"Quiet",5)==0) { /* is the arg "quiet"? */
      missing_quiet='y';                /* no missing file message wanted */
    } /* end quiet arg */
    free(option);                       /* arg not needed after this */
  } /* end there is a second arg */

  if (piperead)
    {
      child.pid = -1;
      child.fd = -1;
      child.stream = NULL;
      if (start_pipe_process(ofilename, &child) == 0)
        stream = child.stream;
    }
  else
    {
      filename = ofilename;
      if (ofilename[0] != '/')
        {
          Home = getenv("HOME");
          if (Home != NULL)
            {
              len = strlen(Home) + strlen(ofilename) + 3;
              home_file = safemalloc(len);
              strlcpy(home_file,Home,len);
              strlcat(home_file,"/",len);
              strlcat(home_file,ofilename,len);
              filename = home_file;
              stream = fopen(filename,"r");
            }
          else
            {
              stream = NULL;
            }
          if (stream == NULL)
            {
              if((filename != NULL)&&(filename!= ofilename))
                free(filename);
              Home = FVWM_CONFIGDIR;
              len = strlen(Home) + strlen(ofilename) + 3;
              home_file = safemalloc(len);
              strlcpy(home_file,Home,len);
              strlcat(home_file,"/",len);
              strlcat(home_file,ofilename,len);
              filename = home_file;
              stream = fopen(filename,"r");
            }
        }
      else
        {
          stream = fopen(filename,"r");
        }
    }

  if(stream == NULL)
  {
    if (missing_quiet == 'n')
    {
      if (piperead)
        fvwm_msg(ERR, cmdname, "command '%s' not run", ofilename);
      else
        fvwm_msg(ERR, cmdname,
                 "file '%s' not found in $HOME or "FVWM_CONFIGDIR, ofilename);
    }
    if (!piperead && filename && filename != ofilename)
      free(filename);
    if (piperead && child.pid > 0)
      stop_pipe_process(&child, 1, cmdname, ofilename);
    if (piperead || (filename != ofilename))
      free(ofilename);
    last_read_failed = 1;
    return;
  }

  if (!piperead)
  {
    if (filename != ofilename && ofilename != NULL)
      {
        free(ofilename);
        ofilename = NULL;
      }
    fcntl(fileno(stream), F_SETFD, 1);
    if(fvwm_file != NULL)
      free(fvwm_file);
    fvwm_file = filename;
  }

  while(stream && !timed_out)
  {
    if (piperead)
    {
      fd_set readfds;
      struct timeval tv;
      int ready;

      FD_ZERO(&readfds);
      FD_SET(child.fd, &readfds);
      tv.tv_sec = PIPE_READ_INTERVAL_SEC;
      tv.tv_usec = 0;

      ready = select(child.fd + 1, &readfds, NULL, NULL, &tv);
      if (ready < 0)
        {
          if (errno == EINTR)
            continue;
          timed_out = 1;
          break;
        }
      if (ready == 0)
        {
          if (++idle_loops >= PIPE_READ_MAX_IDLE_LOOPS)
            {
              timed_out = 1;
              break;
            }
          continue;
        }
      idle_loops = 0;
    }

    tline = fgets(line,(sizeof line)-1,stream);
    if(tline == NULL)
    {
      if (!piperead || feof(stream))
        break;
      if (ferror(stream))
        {
          clearerr(stream);
          continue;
        }
      break;
    }
    {
      int l;
      while((l = strlen(line)) < sizeof(line) && l >= 2 &&
            line[l-2]=='\\' && line[l-1]=='\n')
        {
          char *cont = fgets(line+l-2,sizeof(line)-l+1,stream);
          if (cont == NULL)
            break;
        }
    }
    tline=line;
    while(isspace(*tline))
      tline++;
    if (debugging)
    {
      fvwm_msg(DBG,"ReadSubFunc","about to exec: '%s'",tline);
    }
    ExecuteFunction(tline,tmp_win,eventp,context,*Module);
  }

  if (piperead)
    stop_pipe_process(&child, timed_out, cmdname, ofilename);
  else
    fclose(stream);
  if (piperead && ofilename)
    free(ofilename);
  last_read_failed = timed_out;
}

void ReadFile(XEvent *eventp,Window junk,FvwmWindow *tmp_win,
              unsigned long context, char *action,int* Module)
{
  int this_read = numfilesread;

  if (debugging)
  {
    fvwm_msg(DBG,"ReadFile","about to attempt '%s'",action);
  }

  ReadSubFunc(eventp,junk,tmp_win,context,action,Module,0);

  if (last_read_failed && this_read == 0)
  {
    fvwm_msg(INFO,"Read","trying to read system rc file");
    ExecuteFunction((char *)read_system_rc_cmd,NULL,&Event,C_ROOT,-1);
  }

  if (this_read == 0)
  {
    if (debugging)
    {
      fvwm_msg(DBG,"ReadFile","about to call startup functions");
    }
    StartupStuff();
  }
}

void PipeRead(XEvent *eventp,Window junk,FvwmWindow *tmp_win,
              unsigned long context, char *action,int* Module)
{
  int this_read = numfilesread;

  if (debugging)
  {
    fvwm_msg(DBG,"PipeRead","about to attempt '%s'",action);
  }

  ReadSubFunc(eventp,junk,tmp_win,context,action,Module,1);

  if (last_read_failed && this_read == 0)
  {
    fvwm_msg(INFO,"PipeRead","trying to read system rc file");
    ExecuteFunction((char *)read_system_rc_cmd,NULL,&Event,C_ROOT,-1);
  }

  if (this_read == 0)
  {
    if (debugging)
    {
      fvwm_msg(DBG,"PipeRead","about to call startup functions");
    }
    StartupStuff();
  }
}

