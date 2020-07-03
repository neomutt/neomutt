/**
 * @file
 * Send email using sendmail
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page send_sendmail Send email using sendmail
 *
 * Send email using sendmail
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "context.h"
#include "format_flags.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "options.h"
#include "pager.h"
#ifdef USE_NNTP
#include "nntp/lib.h"
#endif
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_OK 0
#endif

struct ConfigSubset;

SIG_ATOMIC_VOLATILE_T SigAlrm; ///< true after SIGALRM is received

/**
 * alarm_handler - Async notification of an alarm signal
 * @param sig Signal, (SIGALRM)
 */
static void alarm_handler(int sig)
{
  SigAlrm = 1;
}

/**
 * send_msg - invoke sendmail in a subshell
 * @param[in]  path     Path to program to execute
 * @param[in]  args     Arguments to pass to program
 * @param[in]  msg      Temp file containing message to send
 * @param[out] tempfile If sendmail is put in the background, this points
 *                      to the temporary file containing the stdout of the
 *                      child process. If it is NULL, stderr and stdout
 *                      are not redirected.
 * @retval  0 Success
 * @retval >0 Failure, return code from sendmail
 */
static int send_msg(const char *path, char **args, const char *msg, char **tempfile)
{
  sigset_t set;
  int st;

  mutt_sig_block_system();

  sigemptyset(&set);
  /* we also don't want to be stopped right now */
  sigaddset(&set, SIGTSTP);
  sigprocmask(SIG_BLOCK, &set, NULL);

  if ((C_SendmailWait >= 0) && tempfile)
  {
    struct Buffer *tmp = mutt_buffer_pool_get();
    mutt_buffer_mktemp(tmp);
    *tempfile = mutt_buffer_strdup(tmp);
    mutt_buffer_pool_release(&tmp);
  }

  pid_t pid = fork();
  if (pid == 0)
  {
    struct sigaction act, oldalrm;

    /* save parent's ID before setsid() */
    pid_t ppid = getppid();

    /* we want the delivery to continue even after the main process dies,
     * so we put ourselves into another session right away */
    setsid();

    /* next we close all open files */
    close(0);
#ifdef OPEN_MAX
    for (int fd = tempfile ? 1 : 3; fd < OPEN_MAX; fd++)
      close(fd);
#elif defined(_POSIX_OPEN_MAX)
    for (int fd = tempfile ? 1 : 3; fd < _POSIX_OPEN_MAX; fd++)
      close(fd);
#else
    if (tempfile)
    {
      close(1);
      close(2);
    }
#endif

    /* now the second fork() */
    pid = fork();
    if (pid == 0)
    {
      /* "msg" will be opened as stdin */
      if (open(msg, O_RDONLY, 0) < 0)
      {
        unlink(msg);
        _exit(S_ERR);
      }
      unlink(msg);

      if ((C_SendmailWait >= 0) && tempfile && *tempfile)
      {
        /* *tempfile will be opened as stdout */
        if (open(*tempfile, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0600) < 0)
          _exit(S_ERR);
        /* redirect stderr to *tempfile too */
        if (dup(1) < 0)
          _exit(S_ERR);
      }
      else if (tempfile)
      {
        if (open("/dev/null", O_WRONLY | O_APPEND) < 0) /* stdout */
          _exit(S_ERR);
        if (open("/dev/null", O_RDWR | O_APPEND) < 0) /* stderr */
          _exit(S_ERR);
      }

      /* execvpe is a glibc extension */
      /* execvpe (path, args, mutt_envlist_getlist()); */
      execvp(path, args);
      _exit(S_ERR);
    }
    else if (pid == -1)
    {
      unlink(msg);
      FREE(tempfile);
      _exit(S_ERR);
    }

    /* C_SendmailWait > 0: interrupt waitpid() after C_SendmailWait seconds
     * C_SendmailWait = 0: wait forever
     * C_SendmailWait < 0: don't wait */
    if (C_SendmailWait > 0)
    {
      SigAlrm = 0;
      act.sa_handler = alarm_handler;
#ifdef SA_INTERRUPT
      /* need to make sure waitpid() is interrupted on SIGALRM */
      act.sa_flags = SA_INTERRUPT;
#else
      act.sa_flags = 0;
#endif
      sigemptyset(&act.sa_mask);
      sigaction(SIGALRM, &act, &oldalrm);
      alarm(C_SendmailWait);
    }
    else if (C_SendmailWait < 0)
      _exit(0xff & EX_OK);

    if (waitpid(pid, &st, 0) > 0)
    {
      st = WIFEXITED(st) ? WEXITSTATUS(st) : S_ERR;
      if (C_SendmailWait && (st == (0xff & EX_OK)) && tempfile && *tempfile)
      {
        unlink(*tempfile); /* no longer needed */
        FREE(tempfile);
      }
    }
    else
    {
      st = ((C_SendmailWait > 0) && (errno == EINTR) && SigAlrm) ? S_BKG : S_ERR;
      if ((C_SendmailWait > 0) && tempfile && *tempfile)
      {
        unlink(*tempfile);
        FREE(tempfile);
      }
    }

    if (C_SendmailWait > 0)
    {
      /* reset alarm; not really needed, but... */
      alarm(0);
      sigaction(SIGALRM, &oldalrm, NULL);
    }

    if ((kill(ppid, 0) == -1) && (errno == ESRCH) && tempfile && *tempfile)
    {
      /* the parent is already dead */
      unlink(*tempfile);
      FREE(tempfile);
    }

    _exit(st);
  }

  sigprocmask(SIG_UNBLOCK, &set, NULL);

  if ((pid != -1) && (waitpid(pid, &st, 0) > 0))
    st = WIFEXITED(st) ? WEXITSTATUS(st) : S_ERR; /* return child status */
  else
    st = S_ERR; /* error */

  mutt_sig_unblock_system(true);

  return st;
}

/**
 * add_args_one - Add an Address to a dynamic array
 * @param[out] args    Array to add to
 * @param[out] argslen Number of entries in array
 * @param[out] argsmax Allocated size of the array
 * @param[in]  addr    Address to add
 * @retval ptr Updated array
 */
static char **add_args_one(char **args, size_t *argslen, size_t *argsmax, struct Address *addr)
{
  /* weed out group mailboxes, since those are for display only */
  if (addr->mailbox && !addr->group)
  {
    if (*argslen == *argsmax)
      mutt_mem_realloc(&args, (*argsmax += 5) * sizeof(char *));
    args[(*argslen)++] = addr->mailbox;
  }
  return args;
}

/**
 * add_args - Add a list of Addresses to a dynamic array
 * @param[out] args    Array to add to
 * @param[out] argslen Number of entries in array
 * @param[out] argsmax Allocated size of the array
 * @param[in]  al      Addresses to add
 * @retval ptr Updated array
 */
static char **add_args(char **args, size_t *argslen, size_t *argsmax, struct AddressList *al)
{
  if (!al)
    return args;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    args = add_args_one(args, argslen, argsmax, a);
  }
  return args;
}

/**
 * add_option - Add a string to a dynamic array
 * @param[out] args    Array to add to
 * @param[out] argslen Number of entries in array
 * @param[out] argsmax Allocated size of the array
 * @param[in]  s       string to add
 * @retval ptr Updated array
 *
 * @note The array may be realloc()'d
 */
static char **add_option(char **args, size_t *argslen, size_t *argsmax, char *s)
{
  if (*argslen == *argsmax)
    mutt_mem_realloc(&args, (*argsmax += 5) * sizeof(char *));
  args[(*argslen)++] = s;
  return args;
}

/**
 * mutt_invoke_sendmail - Run sendmail
 * @param from     The sender
 * @param to       Recipients
 * @param cc       Recipients
 * @param bcc      Recipients
 * @param msg      File containing message
 * @param eightbit Message contains 8bit chars
 * @param sub      Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_invoke_sendmail(struct AddressList *from, struct AddressList *to,
                         struct AddressList *cc, struct AddressList *bcc,
                         const char *msg, bool eightbit, struct ConfigSubset *sub)
{
  char *ps = NULL, *path = NULL, *s = NULL, *childout = NULL;
  char **args = NULL;
  size_t argslen = 0, argsmax = 0;
  char **extra_args = NULL;
  int i;

#ifdef USE_NNTP
  if (OptNewsSend)
  {
    char cmd[1024];

    mutt_expando_format(cmd, sizeof(cmd), 0, sizeof(cmd), NONULL(C_Inews),
                        nntp_format_str, 0, MUTT_FORMAT_NO_FLAGS);
    if (*cmd == '\0')
    {
      i = nntp_post(Context->mailbox, msg);
      unlink(msg);
      return i;
    }

    s = mutt_str_dup(cmd);
  }
  else
#endif
    s = mutt_str_dup(C_Sendmail);

  /* ensure that $sendmail is set to avoid a crash. http://dev.mutt.org/trac/ticket/3548 */
  if (!s)
  {
    mutt_error(_("$sendmail must be set in order to send mail"));
    return -1;
  }

  ps = s;
  i = 0;
  while ((ps = strtok(ps, " ")))
  {
    if (argslen == argsmax)
      mutt_mem_realloc(&args, sizeof(char *) * (argsmax += 5));

    if (i)
    {
      if (mutt_str_equal(ps, "--"))
        break;
      args[argslen++] = ps;
    }
    else
    {
      path = mutt_str_dup(ps);
      ps = strrchr(ps, '/');
      if (ps)
        ps++;
      else
        ps = path;
      args[argslen++] = ps;
    }
    ps = NULL;
    i++;
  }

#ifdef USE_NNTP
  if (!OptNewsSend)
  {
#endif
    size_t extra_argslen = 0;
    /* If C_Sendmail contained a "--", we save the recipients to append to
   * args after other possible options added below. */
    if (ps)
    {
      ps = NULL;
      size_t extra_argsmax = 0;
      while ((ps = strtok(ps, " ")))
      {
        if (extra_argslen == extra_argsmax)
          mutt_mem_realloc(&extra_args, sizeof(char *) * (extra_argsmax += 5));

        extra_args[extra_argslen++] = ps;
        ps = NULL;
      }
    }

    if (eightbit && C_Use8bitmime)
      args = add_option(args, &argslen, &argsmax, "-B8BITMIME");

    if (C_UseEnvelopeFrom)
    {
      if (C_EnvelopeFromAddress)
      {
        args = add_option(args, &argslen, &argsmax, "-f");
        args = add_args_one(args, &argslen, &argsmax, C_EnvelopeFromAddress);
      }
      else if (!TAILQ_EMPTY(from) && !TAILQ_NEXT(TAILQ_FIRST(from), entries))
      {
        args = add_option(args, &argslen, &argsmax, "-f");
        args = add_args(args, &argslen, &argsmax, from);
      }
    }

    if (C_DsnNotify)
    {
      args = add_option(args, &argslen, &argsmax, "-N");
      args = add_option(args, &argslen, &argsmax, C_DsnNotify);
    }
    if (C_DsnReturn)
    {
      args = add_option(args, &argslen, &argsmax, "-R");
      args = add_option(args, &argslen, &argsmax, C_DsnReturn);
    }
    args = add_option(args, &argslen, &argsmax, "--");
    for (i = 0; i < extra_argslen; i++)
      args = add_option(args, &argslen, &argsmax, extra_args[i]);
    args = add_args(args, &argslen, &argsmax, to);
    args = add_args(args, &argslen, &argsmax, cc);
    args = add_args(args, &argslen, &argsmax, bcc);
#ifdef USE_NNTP
  }
#endif

  if (argslen == argsmax)
    mutt_mem_realloc(&args, sizeof(char *) * (++argsmax));

  args[argslen++] = NULL;

  /* Some user's $sendmail command uses gpg for password decryption,
   * and is set up to prompt using ncurses pinentry.  If we
   * mutt_endwin() it leaves other users staring at a blank screen.
   * So instead, just force a hard redraw on the next refresh. */
  if (!OptNoCurses)
    mutt_need_hard_redraw();

  i = send_msg(path, args, msg, OptNoCurses ? NULL : &childout);
  if (i != (EX_OK & 0xff))
  {
    if (i != S_BKG)
    {
      const char *e = mutt_str_sysexit(i);
      mutt_error(_("Error sending message, child exited %d (%s)"), i, NONULL(e));
      if (childout)
      {
        struct stat st;

        if ((stat(childout, &st) == 0) && (st.st_size > 0))
        {
          mutt_do_pager(_("Output of the delivery process"), childout,
                        MUTT_PAGER_NO_FLAGS, NULL);
        }
      }
    }
  }
  else if (childout)
    unlink(childout);

  FREE(&childout);
  FREE(&path);
  FREE(&s);
  FREE(&args);
  FREE(&extra_args);

  if (i == (EX_OK & 0xff))
    i = 0;
  else if (i == S_BKG)
    i = 1;
  else
    i = -1;
  return i;
}
