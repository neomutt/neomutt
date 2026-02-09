/**
 * @file
 * Shared test harness for mailbox backends
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page harness_common Shared test harness for mailbox backends
 *
 * Shared test harness for mailbox backends
 */

#include "config.h"
#include <getopt.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "common.h"
#include "mx.h"

bool StartupComplete = false;

/**
 * harness_init - Initialise NeoMutt for harness use
 * @param modules  Array of Modules to register
 * @param quiet    If true, suppress logging
 * @retval true  Success
 * @retval false Failure
 */
bool harness_init(const struct Module **modules, bool quiet)
{
  if (quiet)
    MuttLogger = log_disp_null;
  else
    MuttLogger = log_disp_terminal;

  NeoMutt = neomutt_new();
  if (!NeoMutt)
    return false;

  char **tmp_env = MUTT_MEM_CALLOC(2, char *);
  neomutt_init(NeoMutt, tmp_env, modules);
  FREE(&tmp_env);
  StartupComplete = true;

  struct passwd *pw = getpwuid(getuid());
  if (pw)
  {
    mutt_str_replace(&NeoMutt->username, pw->pw_name);
    mutt_str_replace(&NeoMutt->home_dir, pw->pw_dir);
  }

  setlocale(LC_ALL, "");

  return true;
}

/**
 * harness_cleanup - Clean up NeoMutt after harness use
 */
void harness_cleanup(void)
{
  if (NeoMutt)
  {
    commands_clear(&NeoMutt->commands);
    neomutt_cleanup(NeoMutt);
    neomutt_free(&NeoMutt);
  }
  buf_pool_cleanup();
}

/**
 * harness_usage - Print usage information
 * @param name  Program name (argv[0])
 */
static void harness_usage(const char *name)
{
  fprintf(stderr,
          "Usage: %s [options] <mailbox-path>\n"
          "\n"
          "Options:\n"
          "  -l, --list          List emails\n"
          "  -r, --read <N>      Read email number N\n"
          "  -c, --check         Check for new mail\n"
          "  -a, --all           Do all: list, check\n"
          "  -n, --repeat <N>    Repeat N times (default: 1)\n"
          "  -q, --quiet         Suppress output\n"
          "  -v, --verbose       Extra debug output\n"
          "  -h, --help          Show this help\n"
          "\n"
          "Network options:\n"
          "  -u, --user <user>   Username\n"
          "  -p, --pass <pass>   Password (or set NEOMUTT_PASS env var)\n",
          name);
}

/**
 * harness_parse_args - Parse command-line arguments
 * @param opts  Options struct to fill
 * @param argc  Argument count
 * @param argv  Argument values
 * @retval  0 Success
 * @retval -1 Error or help requested
 */
int harness_parse_args(struct HarnessOpts *opts, int argc, char *argv[])
{
  memset(opts, 0, sizeof(*opts));
  opts->read_num = -1;
  opts->repeat = 1;

  static const struct option long_options[] = {
    // clang-format off
    { "list",    no_argument,       NULL, 'l' },
    { "read",    required_argument, NULL, 'r' },
    { "check",   no_argument,       NULL, 'c' },
    { "all",     no_argument,       NULL, 'a' },
    { "repeat",  required_argument, NULL, 'n' },
    { "quiet",   no_argument,       NULL, 'q' },
    { "verbose", no_argument,       NULL, 'v' },
    { "help",    no_argument,       NULL, 'h' },
    { "user",    required_argument, NULL, 'u' },
    { "pass",    required_argument, NULL, 'p' },
    { NULL,      0,                 NULL,  0  },
    // clang-format on
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "lr:can:qvhu:p:", long_options, NULL)) != -1)
  {
    switch (opt)
    {
      case 'l':
        opts->list = true;
        break;
      case 'r':
      {
        char *endptr = NULL;
        long val = strtol(optarg, &endptr, 10);
        if ((endptr == optarg) || (*endptr != '\0') || (val < 0))
        {
          fprintf(stderr, "Invalid number for -r: %s\n", optarg);
          return -1;
        }
        opts->read_num = (int) val;
        break;
      }
      case 'c':
        opts->check = true;
        break;
      case 'a':
        opts->list = true;
        opts->check = true;
        break;
      case 'n':
      {
        char *endptr = NULL;
        long val = strtol(optarg, &endptr, 10);
        if ((endptr == optarg) || (*endptr != '\0') || (val < 1))
        {
          fprintf(stderr, "Invalid number for -n: %s\n", optarg);
          return -1;
        }
        opts->repeat = (int) val;
        break;
      }
      case 'q':
        opts->quiet = true;
        break;
      case 'v':
        opts->verbose = true;
        break;
      case 'u':
        opts->user = optarg;
        break;
      case 'p':
        opts->pass = optarg;
        break;
      case 'h':
        harness_usage(argv[0]);
        return -1;
      default:
        harness_usage(argv[0]);
        return -1;
    }
  }

  if (optind >= argc)
  {
    fprintf(stderr, "Error: mailbox path required\n");
    harness_usage(argv[0]);
    return -1;
  }

  opts->path = argv[optind];

  if (!opts->pass)
    opts->pass = mutt_str_getenv("NEOMUTT_PASS");

  return 0;
}

/**
 * harness_apply_credentials - Set network credentials in config
 * @param opts  Parsed options
 *
 * Maps --user and --pass CLI options to the appropriate config variables
 * for each network backend (imap_user/imap_pass, pop_user/pop_pass, etc.).
 */
static void harness_apply_credentials(const struct HarnessOpts *opts)
{
  if (!opts->user && !opts->pass)
    return;

  const char *user_var = NULL;
  const char *pass_var = NULL;

  switch (opts->type)
  {
    case MUTT_IMAP:
      user_var = "imap_user";
      pass_var = "imap_pass";
      break;
    case MUTT_POP:
      user_var = "pop_user";
      pass_var = "pop_pass";
      break;
    case MUTT_NNTP:
      user_var = "nntp_user";
      pass_var = "nntp_pass";
      break;
    default:
      return;
  }

  struct Buffer *err = buf_pool_get();
  if (opts->user && user_var)
    cs_str_string_set(NeoMutt->sub->cs, user_var, opts->user, err);
  if (opts->pass && pass_var)
    cs_str_string_set(NeoMutt->sub->cs, pass_var, opts->pass, err);
  buf_pool_release(&err);
}

/**
 * harness_open - Open a mailbox
 * @param path  Path to the mailbox
 * @param type  Mailbox type (MUTT_UNKNOWN = auto-detect)
 * @param quiet Suppress output
 * @retval ptr  Opened Mailbox
 * @retval NULL Error
 */
static struct Mailbox *harness_open(const char *path, enum MailboxType type, bool quiet)
{
  struct Mailbox *m = mailbox_new();
  buf_strcpy(&m->pathbuf, path);

  if (type != MUTT_UNKNOWN)
  {
    m->type = type;
    m->mx_ops = mx_get_ops(type);
  }
  else
  {
    m->type = mx_path_probe(path);
    m->mx_ops = mx_get_ops(m->type);
  }

  if (!m->mx_ops)
  {
    if (!quiet)
      fprintf(stderr, "Error: unknown mailbox type: %s\n", path);
    mailbox_free(&m);
    return NULL;
  }

  if (!mx_mbox_open(m, MUTT_READONLY | MUTT_PEEK | MUTT_QUIET))
  {
    if (!quiet)
      fprintf(stderr, "Error: failed to open mailbox: %s\n", path);
    mailbox_free(&m);
    return NULL;
  }

  if (!quiet)
    fprintf(stdout, "Opened mailbox: %s (%d messages)\n", path, m->msg_count);

  return m;
}

/**
 * harness_list_emails - List emails in a mailbox
 * @param m     Mailbox
 * @param quiet Suppress output
 */
static void harness_list_emails(struct Mailbox *m, bool quiet)
{
  if (quiet)
    return;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      continue;

    const char *subject = e->env ? e->env->subject : "(no subject)";
    fprintf(stdout, "  %4d: %s\n", i, subject);
  }
}

/**
 * harness_read_email - Read a specific email
 * @param m     Mailbox
 * @param num   Email number
 * @param quiet Suppress output
 * @retval  0 Success
 * @retval -1 Error
 */
static int harness_read_email(struct Mailbox *m, int num, bool quiet)
{
  if ((num < 0) || (num >= m->msg_count))
  {
    if (!quiet)
      fprintf(stderr, "Error: email %d out of range (0-%d)\n", num, m->msg_count - 1);
    return -1;
  }

  struct Email *e = m->emails[num];
  if (!e)
    return -1;

  struct Message *msg = mx_msg_open(m, e);
  if (!msg)
  {
    if (!quiet)
      fprintf(stderr, "Error: failed to open message %d\n", num);
    return -1;
  }

  if (!quiet)
  {
    fprintf(stdout, "Message %d:\n", num);
    if (e->env)
    {
      if (e->env->subject)
        fprintf(stdout, "  Subject: %s\n", e->env->subject);
      if (e->env->date)
        fprintf(stdout, "  Date: %s\n", e->env->date);
    }
    fprintf(stdout, "  Size: %zd\n", e->body ? e->body->length : 0);
  }

  mx_msg_close(m, &msg);
  return 0;
}

/**
 * harness_check_mail - Check for new mail
 * @param m     Mailbox
 * @param quiet Suppress output
 */
static void harness_check_mail(struct Mailbox *m, bool quiet)
{
  enum MxStatus status = mx_mbox_check(m);

  if (!quiet)
  {
    const char *status_str = "unknown";
    switch (status)
    {
      case MX_STATUS_ERROR:
        status_str = "error";
        break;
      case MX_STATUS_OK:
        status_str = "ok (no change)";
        break;
      case MX_STATUS_NEW_MAIL:
        status_str = "new mail";
        break;
      case MX_STATUS_LOCKED:
        status_str = "locked";
        break;
      case MX_STATUS_FLAGS:
        status_str = "flags changed";
        break;
      case MX_STATUS_REOPENED:
        status_str = "reopened";
        break;
    }
    fprintf(stdout, "Check: %s\n", status_str);
  }
}

/**
 * harness_close - Close a mailbox
 * @param m     Mailbox to close
 * @param quiet Suppress output
 */
static void harness_close(struct Mailbox *m, bool quiet)
{
  mx_mbox_close(m);
  mailbox_free(&m);
  if (!quiet)
    fprintf(stdout, "Closed mailbox\n");
}

/**
 * harness_run - Run the harness operations
 * @param opts  Parsed options
 * @retval 0 Success
 * @retval 1 Failure
 */
int harness_run(struct HarnessOpts *opts)
{
  struct timespec ts_start = { 0 };
  struct timespec ts_end = { 0 };

  harness_apply_credentials(opts);

  if (opts->repeat > 1)
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

  for (int i = 0; i < opts->repeat; i++)
  {
    struct Mailbox *m = harness_open(opts->path, opts->type, opts->quiet);
    if (!m)
      return 1;

    if (opts->list)
      harness_list_emails(m, opts->quiet);

    if (opts->read_num >= 0)
    {
      if (harness_read_email(m, opts->read_num, opts->quiet) != 0)
      {
        harness_close(m, opts->quiet);
        return 1;
      }
    }

    if (opts->check)
      harness_check_mail(m, opts->quiet);

    harness_close(m, opts->quiet);
  }

  if (opts->repeat > 1)
  {
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed = (ts_end.tv_sec - ts_start.tv_sec) +
                     (ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;
    fprintf(stderr, "Completed %d iterations in %.3f seconds (%.1f ops/sec)\n",
            opts->repeat, elapsed, opts->repeat / elapsed);
  }

  return 0;
}
