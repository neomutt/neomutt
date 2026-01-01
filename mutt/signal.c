/**
 * @file
 * Signal handling
 *
 * @authors
 * Copyright (C) 2017-2025 Richard Russon <rich@flatcap.org>
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
 * @page mutt_signal Signal handling
 *
 * Signal handling
 */

#include "config.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "signal2.h"

int endwin(void);

/// A set of signals used by mutt_sig_block(), mutt_sig_unblock()
static sigset_t Sigset;
/// A set of signals used by mutt_sig_block_system(), mutt_sig_unblock_system()
static sigset_t SigsetSys;

/// Backup of SIGINT handler, when mutt_sig_block_system() is called
static struct sigaction SysOldInt;
/// Backup of SIGQUIT handler, when mutt_sig_block_system() is called
static struct sigaction SysOldQuit;

/// true when signals are blocked, e.g. SIGTERM, SIGHUP
/// @sa mutt_sig_block(), mutt_sig_unblock()
static bool SignalsBlocked;

/// true when system signals are blocked, e.g. SIGINT, SIGQUIT
/// @sa mutt_sig_block_system(), mutt_sig_unblock_system()
static bool SysSignalsBlocked;

/// Function to handle other signals, e.g. SIGINT (2)
static sig_handler_t SigHandler = mutt_sig_empty_handler;
/// Function to handle SIGTERM (15), SIGHUP (1), SIGQUIT (3) signals
static sig_handler_t ExitHandler = mutt_sig_exit_handler;
/// Function to handle SIGSEGV (11) signals
static sig_handler_t SegvHandler = mutt_sig_exit_handler;

/// Keep the old SEGV handler, it could have been set by ASAN
sig_handler_t OldSegvHandler = NULL;

volatile sig_atomic_t SigInt;   ///< true after SIGINT is received
volatile sig_atomic_t SigWinch; ///< true after SIGWINCH is received

/**
 * exit_print_uint - AS-safe version of printf("%u", n)
 * @param n Number to be printed
 */
static void exit_print_uint(unsigned int n)
{
  char digit;

  if (n > 9)
    exit_print_uint(n / 10);

  digit = '0' + (n % 10);

  if (write(STDOUT_FILENO, &digit, 1) == -1)
  {
    // do nothing
  }
}

/**
 * exit_print_int - AS-safe version of printf("%d", n)
 * @param n Number to be printed
 */
static void exit_print_int(int n)
{
  if (n < 0)
  {
    if (write(STDOUT_FILENO, "-", 1) == -1)
    {
      // do nothing
    }

    n = -n;
  }
  exit_print_uint(n);
}

/**
 * exit_print_string - AS-safe version of printf("%s", str)
 * @param str String to be printed
 */
static void exit_print_string(const char *str)
{
  if (!str)
    return;

  if (write(STDOUT_FILENO, str, strlen(str)) == -1)
  {
    // do nothing
  }
}

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
  exit_print_string("Caught signal ");
  exit_print_int(sig);
  exit_print_string(" ");
#ifdef HAVE_DECL_SYS_SIGLIST
  exit_print_string(sys_siglist[sig]);
#elif (defined(__sun__) && defined(__svr4__))
  exit_print_string(_sys_siglist[sig]);
#elif (defined(__alpha) && defined(__osf__))
  exit_print_string(__sys_siglist[sig]);
#endif
  exit_print_string("...  Exiting\n");
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
    SigHandler = sig_fn;

  if (exit_fn)
    ExitHandler = exit_fn;

  if (segv_fn)
    SegvHandler = segv_fn;

  struct sigaction act = { 0 };
  struct sigaction old_act = { 0 };

  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, NULL);

  act.sa_handler = SegvHandler;
  sigaction(SIGSEGV, &act, &old_act);
  OldSegvHandler = old_act.sa_handler;

  act.sa_handler = ExitHandler;
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGHUP, &act, NULL);
  sigaction(SIGQUIT, &act, NULL);

  /* we want to avoid race conditions */
  sigaddset(&act.sa_mask, SIGTSTP);

  act.sa_handler = SigHandler;

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

  struct sigaction sa = { 0 };

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
    struct sigaction sa = { 0 };

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
  struct sigaction sa = { 0 };

  sa.sa_handler = SigHandler;
#ifdef SA_RESTART
  if (!allow)
    sa.sa_flags |= SA_RESTART;
#endif
  sigaction(SIGINT, &sa, NULL);
}

/**
 * mutt_sig_reset_child_signals - Reset ignored signals back to the default
 *
 * See sigaction(2):
 *   A child created via fork(2) inherits a copy of its parent's
 *   signal dispositions.  During an execve(2), the dispositions of
 *   handled signals are reset to the default; the dispositions of
 *   ignored signals are left unchanged.
 */
void mutt_sig_reset_child_signals(void)
{
  struct sigaction sa = { 0 };

  sa.sa_handler = SIG_DFL;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  /* These signals are set to SIG_IGN and must be reset */
  sigaction(SIGPIPE, &sa, NULL);

  /* These technically don't need to be reset, but the code has been
   * doing so for a long time. */
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGTSTP, &sa, NULL);
  sigaction(SIGCONT, &sa, NULL);
}

/**
 * assertion_dump - Dump some debugging info before we stop the program
 * @param file Source file
 * @param line Line of source
 * @param func Function
 * @param cond Assertion condition
 */
void assertion_dump(const char *file, int line, const char *func, const char *cond)
{
  endwin();
  show_backtrace();
  printf("%s:%d:%s() -- assertion failed (%s)\n", file, line, func, cond);
}
