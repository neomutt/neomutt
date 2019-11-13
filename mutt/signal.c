/**
 * @file
 * Signal handling
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page signal Signal handling
 *
 * Signal handling
 */

#include "config.h"
#include <stddef.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "message.h"
#include "signal2.h"

static sigset_t Sigset;
static sigset_t SigsetSys;
static struct sigaction SysOldInt;
static struct sigaction SysOldQuit;
static bool SignalsBlocked;
static bool SysSignalsBlocked;

static sig_handler_t sig_handler = mutt_sig_empty_handler;
static sig_handler_t exit_handler = mutt_sig_exit_handler;
static sig_handler_t segv_handler = mutt_sig_exit_handler;

/**
 * mutt_sig_empty_handler - Dummy signal handler
 * @param sig Signal number, e.g. SIGINT
 *
 * Useful for signals that we can't ignore,
 * or don't want to do anything with.
 */
void mutt_sig_empty_handler(int sig)
{
}

/**
 * mutt_sig_exit_handler - Notify the user and shutdown gracefully
 * @param sig Signal number, e.g. SIGINT
 */
void mutt_sig_exit_handler(int sig)
{
#ifdef HAVE_DECL_SYS_SIGLIST
  printf(_("Caught signal %d (%s) ...  Exiting.\n"), sig, sys_siglist[sig]);
#elif (defined(__sun__) && defined(__svr4__))
  printf(_("Caught signal %d (%s) ...  Exiting.\n"), sig, _sys_siglist[sig]);
#elif (defined(__alpha) && defined(__osf__))
  printf(_("Caught signal %d (%s) ...  Exiting.\n"), sig, __sys_siglist[sig]);
#else
  printf(_("Caught signal %d ...  Exiting.\n"), sig);
#endif
  exit(0);
}

/**
 * mutt_sig_init - Initialise the signal handling
 * @param sig_fn  Function to handle signals
 * @param exit_fn Function to call on uncaught signals
 * @param segv_fn Function to call on a segfault (Segmentation Violation)
 *
 * Set up handlers to ignore or catch signals of interest.
 * We use three handlers for the signals we want to catch, ignore, or exit.
 */
void mutt_sig_init(sig_handler_t sig_fn, sig_handler_t exit_fn, sig_handler_t segv_fn)
{
  if (sig_fn)
    sig_handler = sig_fn;

  if (exit_fn)
    exit_handler = exit_fn;

  if (segv_fn)
    segv_handler = segv_fn;

  struct sigaction act;

  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, NULL);

  act.sa_handler = segv_handler;
  sigaction(SIGSEGV, &act, NULL);

  act.sa_handler = exit_handler;
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGHUP, &act, NULL);
  sigaction(SIGQUIT, &act, NULL);

  /* we want to avoid race conditions */
  sigaddset(&act.sa_mask, SIGTSTP);

  act.sa_handler = sig_handler;

  /* we want SIGALRM to abort the current syscall, so we do this before
   * setting the SA_RESTART flag below.  currently this is only used to
   * timeout on a connect() call in a reasonable amount of time. */
  sigaction(SIGALRM, &act, NULL);

/* we also don't want to mess with interrupted system calls */
#ifdef SA_RESTART
  act.sa_flags = SA_RESTART;
#endif

  sigaction(SIGCONT, &act, NULL);
  sigaction(SIGTSTP, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGWINCH, &act, NULL);

  /* POSIX doesn't allow us to ignore SIGCHLD,
   * so we just install a dummy handler for it */
  act.sa_handler = mutt_sig_empty_handler;
  /* don't need to block any other signals here */
  sigemptyset(&act.sa_mask);
  /* we don't want to mess with stopped children */
  act.sa_flags |= SA_NOCLDSTOP;
  sigaction(SIGCHLD, &act, NULL);
}

/**
 * mutt_sig_block - Block signals during critical operations
 *
 * It's important that certain signals don't interfere with critical operations.
 * Call mutt_sig_unblock() to restore the signals' behaviour.
 */
void mutt_sig_block(void)
{
  if (SignalsBlocked)
    return;

  sigemptyset(&Sigset);
  sigaddset(&Sigset, SIGTERM);
  sigaddset(&Sigset, SIGHUP);
  sigaddset(&Sigset, SIGTSTP);
  sigaddset(&Sigset, SIGINT);
  sigaddset(&Sigset, SIGWINCH);
  sigprocmask(SIG_BLOCK, &Sigset, 0);
  SignalsBlocked = true;
}

/**
 * mutt_sig_unblock - Restore previously blocked signals
 */
void mutt_sig_unblock(void)
{
  if (!SignalsBlocked)
    return;

  sigprocmask(SIG_UNBLOCK, &Sigset, 0);
  SignalsBlocked = false;
}

/**
 * mutt_sig_block_system - Block signals before calling exec()
 *
 * It's important that certain signals don't interfere with the child process.
 * Call mutt_sig_unblock_system() to restore the signals' behaviour.
 */
void mutt_sig_block_system(void)
{
  if (SysSignalsBlocked)
    return;

  struct sigaction sa;

  /* POSIX: ignore SIGINT and SIGQUIT & block SIGCHLD before exec */
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, &SysOldInt);
  sigaction(SIGQUIT, &sa, &SysOldQuit);

  sigemptyset(&SigsetSys);
  sigaddset(&SigsetSys, SIGCHLD);
  sigprocmask(SIG_BLOCK, &SigsetSys, 0);
  SysSignalsBlocked = true;
}

/**
 * mutt_sig_unblock_system - Restore previously blocked signals
 * @param restore If true, restore previous SIGINT, SIGQUIT behaviour
 */
void mutt_sig_unblock_system(bool restore)
{
  if (!SysSignalsBlocked)
    return;

  sigprocmask(SIG_UNBLOCK, &SigsetSys, NULL);
  if (restore)
  {
    sigaction(SIGQUIT, &SysOldQuit, NULL);
    sigaction(SIGINT, &SysOldInt, NULL);
  }
  else
  {
    struct sigaction sa;

    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
  }

  SysSignalsBlocked = false;
}

/**
 * mutt_sig_allow_interrupt - Allow/disallow Ctrl-C (SIGINT)
 * @param allow True to allow Ctrl-C to interrupt signals
 *
 * Allow the user to interrupt some long operations.
 */
void mutt_sig_allow_interrupt(bool allow)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_handler;
#ifdef SA_RESTART
  if (!allow)
    sa.sa_flags |= SA_RESTART;
#endif
  sigaction(SIGINT, &sa, NULL);
}
