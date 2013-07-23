/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"

#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

static sigset_t Sigset;
static sigset_t SigsetSys;
static struct sigaction SysOldInt;
static struct sigaction SysOldQuit;
static int IsEndwin = 0;

/* Attempt to catch "ordinary" signals and shut down gracefully. */
static void exit_handler (int sig)
{
  curs_set (1);
  endwin (); /* just to be safe */
#if SYS_SIGLIST_DECLARED
  printf(_("%s...  Exiting.\n"), sys_siglist[sig]);
#else
#if (__sun__ && __svr4__)
  printf(_("Caught %s...  Exiting.\n"), _sys_siglist[sig]);
#else
#if (__alpha && __osf__)
  printf(_("Caught %s...  Exiting.\n"), __sys_siglist[sig]);
#else
  printf(_("Caught signal %d...  Exiting.\n"), sig);
#endif
#endif
#endif
  exit (0);
}

static void chld_handler (int sig)
{
  /* empty */
}

static void sighandler (int sig)
{
  int save_errno = errno;

  switch (sig)
  {
    case SIGTSTP: /* user requested a suspend */
      if (!option (OPTSUSPEND))
        break;
      IsEndwin = isendwin ();
      curs_set (1);
      if (!IsEndwin)
	endwin ();
      kill (0, SIGSTOP);

    case SIGCONT:
      if (!IsEndwin)
	refresh ();
      mutt_curs_set (-1);
#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
      /* We don't receive SIGWINCH when suspended; however, no harm is done by
       * just assuming we received one, and triggering the 'resize' anyway. */
      SigWinch = 1;
#endif
      break;

#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
    case SIGWINCH:
      SigWinch = 1;
      break;
#endif

    case SIGINT:
      SigInt = 1;
      break;

  }
  errno = save_errno;
}

#ifdef USE_SLANG_CURSES
int mutt_intr_hook (void)
{
  return (-1);
}
#endif /* USE_SLANG_CURSES */

void mutt_signal_init (void)
{
  struct sigaction act;

  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &act, NULL);

  act.sa_handler = exit_handler;
  sigaction (SIGTERM, &act, NULL);
  sigaction (SIGHUP, &act, NULL);
  sigaction (SIGQUIT, &act, NULL);

  /* we want to avoid race conditions */
  sigaddset (&act.sa_mask, SIGTSTP);

  act.sa_handler = sighandler;

  /* we want SIGALRM to abort the current syscall, so we do this before
   * setting the SA_RESTART flag below.  currently this is only used to
   * timeout on a connect() call in a reasonable amount of time.
   */
  sigaction (SIGALRM, &act, NULL);

  /* we also don't want to mess with interrupted system calls */
#ifdef SA_RESTART
  act.sa_flags = SA_RESTART;
#endif

  sigaction (SIGCONT, &act, NULL);
  sigaction (SIGTSTP, &act, NULL);
  sigaction (SIGINT, &act, NULL);
#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
  sigaction (SIGWINCH, &act, NULL);
#endif

  /* POSIX doesn't allow us to ignore SIGCHLD,
   * so we just install a dummy handler for it
   */
  act.sa_handler = chld_handler;
  /* don't need to block any other signals here */
  sigemptyset (&act.sa_mask);
  /* we don't want to mess with stopped children */
  act.sa_flags |= SA_NOCLDSTOP;
  sigaction (SIGCHLD, &act, NULL);

#ifdef USE_SLANG_CURSES
  /* This bit of code is required because of the implementation of
   * SLcurses_wgetch().  If a signal is received (like SIGWINCH) when we
   * are in blocking mode, SLsys_getkey() will not return an error unless
   * a handler function is defined and it returns -1.  This is needed so
   * that if the user resizes the screen while at a prompt, it will just
   * abort and go back to the main-menu.
   */
  SLang_getkey_intr_hook = mutt_intr_hook;
#endif
}

/* signals which are important to block while doing critical ops */
void mutt_block_signals (void)
{
  if (!option (OPTSIGNALSBLOCKED))
  {
    sigemptyset (&Sigset);
    sigaddset (&Sigset, SIGTERM);
    sigaddset (&Sigset, SIGHUP);
    sigaddset (&Sigset, SIGTSTP);
    sigaddset (&Sigset, SIGINT);
#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
    sigaddset (&Sigset, SIGWINCH);
#endif
    sigprocmask (SIG_BLOCK, &Sigset, 0);
    set_option (OPTSIGNALSBLOCKED);
  }
}

/* restore the previous signal mask */
void mutt_unblock_signals (void)
{
  if (option (OPTSIGNALSBLOCKED))
  {
    sigprocmask (SIG_UNBLOCK, &Sigset, 0);
    unset_option (OPTSIGNALSBLOCKED);
  }
}

void mutt_block_signals_system (void)
{
  struct sigaction sa;

  if (! option (OPTSYSSIGNALSBLOCKED))
  {
    /* POSIX: ignore SIGINT and SIGQUIT & block SIGCHLD  before exec */
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);
    sigaction (SIGINT, &sa, &SysOldInt);
    sigaction (SIGQUIT, &sa, &SysOldQuit);

    sigemptyset (&SigsetSys);
    sigaddset (&SigsetSys, SIGCHLD);
    sigprocmask (SIG_BLOCK, &SigsetSys, 0);
    set_option (OPTSYSSIGNALSBLOCKED);
  }
}

void mutt_unblock_signals_system (int catch)
{
  if (option (OPTSYSSIGNALSBLOCKED))
  {
    sigprocmask (SIG_UNBLOCK, &SigsetSys, NULL);
    if (catch)
    {
      sigaction (SIGQUIT, &SysOldQuit, NULL);
      sigaction (SIGINT, &SysOldInt, NULL);
    }
    else
    {
      struct sigaction sa;

      sa.sa_handler = SIG_DFL;
      sigemptyset (&sa.sa_mask);
      sa.sa_flags = 0;
      sigaction (SIGQUIT, &sa, NULL);
      sigaction (SIGINT, &sa, NULL);
    }

    unset_option (OPTSYSSIGNALSBLOCKED);
  }
}

void mutt_allow_interrupt (int disposition)
{
  struct sigaction sa;
  
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = sighandler;
#ifdef SA_RESTART
  if (disposition == 0)
    sa.sa_flags |= SA_RESTART;
#endif
  sigaction (SIGINT, &sa, NULL);
}
