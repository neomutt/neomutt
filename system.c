static char rcsid[]="$Id$";
/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int _mutt_system (const char *cmd, int flags)
{
  int rc = -1;
  struct sigaction act;
  struct sigaction oldcont;
  struct sigaction oldtstp;
  struct sigaction oldint;
  struct sigaction oldquit;
  struct sigaction oldchld;
  sigset_t set;
  pid_t thepid;

  /* must block SIGCHLD and ignore SIGINT and SIGQUIT */

  act.sa_handler = SIG_IGN;
  act.sa_flags = 0;
  sigemptyset (&(act.sa_mask));
  sigaction (SIGINT, &act, &oldint);
  sigaction (SIGQUIT, &act, &oldquit);
  if (flags & M_DETACH_PROCESS)
  {
    act.sa_flags = SA_NOCLDSTOP;
    sigaction (SIGCHLD, &act, &oldchld);
    act.sa_flags = 0;
  }
  else
  {
    sigemptyset (&set);
    sigaddset (&set, SIGCHLD);
    sigprocmask (SIG_BLOCK, &set, NULL);
  }

  act.sa_handler = SIG_DFL;
  sigaction (SIGTSTP, &act, &oldtstp);
  sigaction (SIGCONT, &act, &oldcont);

  if ((thepid = fork ()) == 0)
  {
    /* reset signals for the child */
    sigaction (SIGINT, &act, NULL);
    sigaction (SIGQUIT, &act, NULL);

    if (flags & M_DETACH_PROCESS)
    {
      setsid ();
      switch (fork ())
      {
	case 0:
	  sigaction (SIGCHLD, &act, NULL);
	  break;

	case -1:
	  _exit (127);

	default:
	  _exit (0);
      }
    }
    else
      sigprocmask (SIG_UNBLOCK, &set, NULL);

    execl (EXECSHELL, "sh", "-c", cmd, NULL);
    _exit (127); /* execl error */
  }
  else if (thepid != -1)
  {
    /* wait for the child process to finish */
    waitpid (thepid, &rc, 0);
  }

  /* reset SIGINT, SIGQUIT and SIGCHLD */
  sigaction (SIGINT, &oldint, NULL);
  sigaction (SIGQUIT, &oldquit, NULL);
  if (flags & M_DETACH_PROCESS)
    sigaction (SIGCHLD, &oldchld, NULL);
  else
    sigprocmask (SIG_UNBLOCK, &set, NULL);

  sigaction (SIGCONT, &oldcont, NULL);
  sigaction (SIGTSTP, &oldtstp, NULL);

  rc = WIFEXITED (rc) ? WEXITSTATUS (rc) : -1;

  return (rc);
}
