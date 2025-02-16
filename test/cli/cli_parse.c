/**
 * @file
 * Test code for Command Line Parsing
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "cli/lib.h"
#include "test_common.h"

static void serialise_bool(bool b, struct Buffer *res)
{
  if (b)
    buf_addch(res, 'Y');
  else
    buf_addch(res, 'N');
}

static void serialise_buffer(struct Buffer *value, struct Buffer *res)
{
  if (buf_is_empty(value))
    buf_add_printf(res, ":-");
  else
    buf_add_printf(res, ":%s", buf_string(value));
}

static void serialise_array(const struct StringArray *sa, struct Buffer *res)
{
  buf_addstr(res, ":{");

  char **cp = NULL;
  ARRAY_FOREACH(cp, sa)
  {
    buf_addstr(res, *cp);
    if (ARRAY_FOREACH_IDX_cp < (ARRAY_SIZE(sa) - 1))
      buf_addch(res, ',');
  }

  buf_addch(res, '}');
}

static void serialise_help(struct CliHelp *help, struct Buffer *res)
{
  if (!help->is_set)
    return;

  buf_addstr(res, "H(");

  serialise_bool(help->help, res);
  serialise_bool(help->version, res);
  serialise_bool(help->license, res);

  buf_addch(res, '0' + help->mode);

  buf_addstr(res, ")");
}

static void serialise_shared(struct CliShared *shared, struct Buffer *res)
{
  if (!shared->is_set)
    return;

  buf_addstr(res, "X(");

  serialise_array(&shared->user_files, res);
  serialise_bool(shared->disable_system, res);

  serialise_array(&shared->commands, res);
  serialise_buffer(&shared->mbox_type, res);

  serialise_buffer(&shared->log_level, res);
  serialise_buffer(&shared->log_file, res);

  buf_addstr(res, ")");
}

static void serialise_info(struct CliInfo *info, struct Buffer *res)
{
  if (!info->is_set)
    return;

  buf_addstr(res, "I(");

  serialise_bool(info->dump_config, res);
  serialise_bool(info->dump_changed, res);
  serialise_bool(info->show_help, res);
  serialise_bool(info->hide_sensitive, res);

  serialise_array(&info->alias_queries, res);
  serialise_array(&info->queries, res);

  buf_addstr(res, ")");
}

static void serialise_send(struct CliSend *send, struct Buffer *res)
{
  if (!send->is_set)
    return;

  buf_addstr(res, "S(");

  serialise_bool(send->use_crypto, res);
  serialise_bool(send->edit_infile, res);

  serialise_array(&send->attach, res);
  serialise_array(&send->bcc_list, res);
  serialise_array(&send->cc_list, res);
  serialise_array(&send->addresses, res);

  serialise_buffer(&send->draft_file, res);
  serialise_buffer(&send->include_file, res);
  serialise_buffer(&send->subject, res);

  buf_addstr(res, ")");
}

static void serialise_tui(struct CliTui *tui, struct Buffer *res)
{
  if (!tui->is_set)
    return;

  buf_addstr(res, "T(");

  serialise_bool(tui->read_only, res);
  serialise_bool(tui->start_postponed, res);
  serialise_bool(tui->start_browser, res);
  serialise_bool(tui->start_nntp, res);
  serialise_bool(tui->start_new_mail, res);
  serialise_bool(tui->start_any_mail, res);

  serialise_buffer(&tui->folder, res);
  serialise_buffer(&tui->nntp_server, res);

  buf_addstr(res, ")");
}

static void serialise_cli(struct CommandLine *cli, struct Buffer *res)
{
  buf_reset(res);
  serialise_shared(&cli->shared, res);
  serialise_help(&cli->help, res);
  serialise_info(&cli->info, res);
  serialise_send(&cli->send, res);
  serialise_tui(&cli->tui, res);
}

static void args_split(const char *args, struct StringArray *sa)
{
  // Fake entry at argv[0]
  ARRAY_ADD(sa, mutt_str_dup("neomutt"));

  char *src = mutt_str_dup(args);
  if (!src)
    return;

  char *start = src;
  for (char *p = start; *p; p++)
  {
    if (p[0] != ' ')
      continue;

    p[0] = '\0';
    ARRAY_ADD(sa, mutt_str_dup(start));

    start = p + 1;
  }

  ARRAY_ADD(sa, mutt_str_dup(start));

  FREE(&src);
}

static void args_clear(struct StringArray *sa)
{
  char **cp = NULL;
  ARRAY_FOREACH(cp, sa)
  {
    FREE(cp);
  }

  ARRAY_FREE(sa);
}

void test_cli_parse(void)
{
  // bool cli_parse(int argc, char **argv, struct CommandLine *cli);

  MuttLogger = log_disp_null;

  bool rc;

  // Degenerate
  {
    char *args = "apple banana";
    struct CommandLine *cli = command_line_new();
    struct StringArray sa = ARRAY_HEAD_INITIALIZER;
    args_split(args, &sa);

    rc = cli_parse(0, sa.entries, cli);
    TEST_CHECK(rc == false);

    rc = cli_parse(2, NULL, cli);
    TEST_CHECK(rc == false);

    rc = cli_parse(2, sa.entries, NULL);
    TEST_CHECK(rc == false);

    args_clear(&sa);
    command_line_free(&cli);

    command_line_free(NULL);
  }

  // Simple tests
  {
    static const char *Tests[][2] = {
      // clang-format off
      // No args
      { "",                      "" },

      // Help
      { "-h",                    "H(YNN0)" },
      { "-v",                    "H(NYN0)" },
      { "-h -v",                 "H(YYN0)" },
      { "-v -v",                 "H(NYY0)" },
      { "-vv",                   "H(NYY0)" },
      { "-vhv",                  "H(YYY0)" },

      // Shared
      { "-n",                    "X(:{}Y:{}:-:-:-)" },
      { "-F apple",              "X(:{apple}N:{}:-:-:-)" },
      { "-F apple -F banana",    "X(:{apple,banana}N:{}:-:-:-)" },
      { "-nF apple",             "X(:{apple}Y:{}:-:-:-)" },
      { "-F apple -n -F banana", "X(:{apple,banana}Y:{}:-:-:-)" },
      { "-e apple",              "X(:{}N:{apple}:-:-:-)" },
      { "-e apple -e banana",    "X(:{}N:{apple,banana}:-:-:-)" },
      { "-m apple",              "X(:{}N:{}:apple:-:-)" },
      { "-m apple -m banana",    "X(:{}N:{}:banana:-:-)" },
      { "-d 3",                  "X(:{}N:{}:-:3:-)" },
      { "-d3",                   "X(:{}N:{}:-:3:-)" },
      { "-l apple",              "X(:{}N:{}:-:-:apple)" },
      { "-lapple",               "X(:{}N:{}:-:-:apple)" },
      { "-d 3 -l apple",         "X(:{}N:{}:-:3:apple)" },
      { "-d3 -lapple",           "X(:{}N:{}:-:3:apple)" },

      // Info
      { "-D",                    "I(YNNN:{}:{})" },
      { "-D -D",                 "I(YYNN:{}:{})" },
      { "-D -O",                 "I(YNYN:{}:{})" },
      { "-D -S",                 "I(YNNY:{}:{})" },
      { "-DOSD",                 "I(YYYY:{}:{})" },
      { "-A apple",              "I(NNNN:{apple}:{})" },
      { "-A apple -A banana",    "I(NNNN:{apple,banana}:{})" },
      { "-A apple banana",       "I(NNNN:{apple,banana}:{})" },
      { "-Q apple",              "I(NNNN:{}:{apple})" },
      { "-Q apple -Q banana",    "I(NNNN:{}:{apple,banana})" },
      { "-Q apple banana",       "I(NNNN:{}:{apple,banana})" },

      // Send
      { "-C",                    "S(YN:{}:{}:{}:{}:-:-:-)" },
      { "-E",                    "S(NY:{}:{}:{}:{}:-:-:-)" },
      { "-EC",                   "S(YY:{}:{}:{}:{}:-:-:-)" },
      { "-a apple",              "S(NN:{apple}:{}:{}:{}:-:-:-)" },
      { "-a apple -a banana",    "S(NN:{apple,banana}:{}:{}:{}:-:-:-)" },
      { "-a apple banana",       "S(NN:{apple,banana}:{}:{}:{}:-:-:-)" },
      { "-b apple",              "S(NN:{}:{apple}:{}:{}:-:-:-)" },
      { "-b apple -b banana",    "S(NN:{}:{apple,banana}:{}:{}:-:-:-)" },
      { "-c apple",              "S(NN:{}:{}:{apple}:{}:-:-:-)" },
      { "-c apple -c banana",    "S(NN:{}:{}:{apple,banana}:{}:-:-:-)" },
      { "apple",                 "S(NN:{}:{}:{}:{apple}:-:-:-)" },
      { "apple banana",          "S(NN:{}:{}:{}:{apple,banana}:-:-:-)" },
      { "apple banana cherry",   "S(NN:{}:{}:{}:{apple,banana,cherry}:-:-:-)" },
      { "-H apple",              "S(NN:{}:{}:{}:{}:apple:-:-)" },
      { "-H apple -H banana",    "S(NN:{}:{}:{}:{}:banana:-:-)" },
      { "-i apple",              "S(NN:{}:{}:{}:{}:-:apple:-)" },
      { "-i apple -i banana",    "S(NN:{}:{}:{}:{}:-:banana:-)" },
      { "-s apple",              "S(NN:{}:{}:{}:{}:-:-:apple)" },
      { "-s apple -s banana",    "S(NN:{}:{}:{}:{}:-:-:banana)" },

      // TUI
      { "-R",                    "T(YNNNNN:-:-)" },
      { "-p",                    "T(NYNNNN:-:-)" },
      { "-y",                    "T(NNYNNN:-:-)" },
      { "-G",                    "T(NNNYNN:-:-)" },
      { "-Z",                    "T(NNNNYN:-:-)" },
      { "-z",                    "T(NNNNNY:-:-)" },
      { "-R -y -G -Z",           "T(YNYYYN:-:-)" },
      { "-R -p -G -z",           "T(YYNYNY:-:-)" },
      { "-y -p -G -Z",           "T(NYYYYN:-:-)" },
      { "-f apple",              "T(NNNNNN:apple:-)" },
      { "-f apple -f banana",    "T(NNNNNN:banana:-)" },
      { "-g apple",              "T(NNNYNN:-:apple)" },
      { "-g apple -g banana",    "T(NNNYNN:-:banana)" },

      // Complex tests
      { "apple",                            "S(NN:{}:{}:{}:{apple}:-:-:-)" },
      { "apple --",                         "S(NN:{}:{}:{}:{apple}:-:-:-)" },
      { "apple -- banana",                  "S(NN:{}:{}:{}:{apple,banana}:-:-:-)" },
      { "apple -- banana",                  "S(NN:{}:{}:{}:{apple,banana}:-:-:-)" },
      { "-A apple banana -- cherry",        "I(NNNN:{apple,banana}:{})S(NN:{}:{}:{}:{cherry}:-:-:-)" },
      { "-Q apple banana -- cherry damson", "I(NNNN:{}:{apple,banana})S(NN:{}:{}:{}:{cherry,damson}:-:-:-)" },

      // Help modes
      { "-h",                    "H(YNN0)" },
      { "-h shared",             "H(YNN1)" },
      { "-h help",               "H(YNN2)" },
      { "-h info",               "H(YNN3)" },
      { "-h send",               "H(YNN4)" },
      { "-h tui",                "H(YNN5)" },
      { "-h all",                "H(YNN6)" },
      // clang-format on
    };

    struct Buffer *res = buf_pool_get();

    for (size_t i = 0; i < mutt_array_size(Tests); i++)
    {
      struct CommandLine *cli = command_line_new();
      struct StringArray sa = ARRAY_HEAD_INITIALIZER;

      TEST_CASE(Tests[i][0]);
      args_split(Tests[i][0], &sa);

      rc = cli_parse(ARRAY_SIZE(&sa), sa.entries, cli);
      TEST_CHECK(rc == true);

      serialise_cli(cli, res);
      TEST_CHECK_STR_EQ(buf_string(res), Tests[i][1]);

      args_clear(&sa);
      command_line_free(&cli);
    }

    buf_pool_release(&res);
  }

  // Failing tests
  {
    // One bad option and plenty that should take a parameter
    static const char *Tests[] = {
      "-9", "-A", "-a", "-b", "-F", "-f", "-c", "-d",
      "-l", "-e", "-g", "-H", "-i", "-m", "-Q", "-s",
    };

    struct Buffer *res = buf_pool_get();

    opterr = 0; // Disable stderr warnings

    for (size_t i = 0; i < mutt_array_size(Tests); i++)
    {
      struct CommandLine *cli = command_line_new();
      struct StringArray sa = ARRAY_HEAD_INITIALIZER;

      TEST_CASE(Tests[i]);
      args_split(Tests[i], &sa);

      rc = cli_parse(ARRAY_SIZE(&sa), sa.entries, cli);
      TEST_CHECK(rc == false);

      serialise_cli(cli, res);
      TEST_CHECK_STR_EQ(buf_string(res), "H(YNN0)");

      args_clear(&sa);
      command_line_free(&cli);
    }

    buf_pool_release(&res);
  }
}
