/**
 * @file
 * Test code for parse_mailboxes()
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
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "commands/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command Mailboxes      = { "mailboxes",       CMD_MAILBOXES,       NULL, CMD_NO_DATA };
static const struct Command NamedMailboxes = { "named-mailboxes", CMD_NAMED_MAILBOXES, NULL, CMD_NO_DATA };
// clang-format on

static const struct CommandTest MailboxesTests[] = {
  // clang-format off
  // mailboxes [[ -label <label> ] | -nolabel ] [ -notify | -nonotify ] [ -poll | -nopoll ] <mailbox> [ ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "+" },
  { MUTT_CMD_SUCCESS, "+neo" },
  { MUTT_CMD_SUCCESS, "+neo/devel" },
  { MUTT_CMD_SUCCESS, "+neo/github" },
  { MUTT_CMD_SUCCESS, "-label apple +home/apple" },
  { MUTT_CMD_SUCCESS, "-nolabel     +home/apple" },
  { MUTT_CMD_SUCCESS, "-notify   +home/banana" },
  { MUTT_CMD_SUCCESS, "-nonotify +home/banana" },
  { MUTT_CMD_SUCCESS, "-poll   +home/cherry" },
  { MUTT_CMD_SUCCESS, "-nopoll +home/cherry" },
  { MUTT_CMD_SUCCESS, "+home/damson +home/endive -label f +home/fig" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest NamedMailboxesTests[] = {
  // clang-format off
  // named-mailboxes [ -notify | -nonotify ] [ -poll | -nopoll ] <mailbox> [ ... ]
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "apple +home/apple" },
  { MUTT_CMD_SUCCESS, "banana +home/banana cherry +home/cherry" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

/**
 * test_parse_mailboxes_args_degenerate - Test parse_mailboxes_args with degenerate inputs
 */
static void test_parse_mailboxes_args_degenerate(void)
{
  // bool parse_mailboxes_args(const struct Command *cmd, struct Buffer *line, struct Buffer *err, struct ParseMailboxArray *args)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  bool rc;

  // Test NULL cmd
  TEST_CASE("NULL cmd");
  buf_strcpy(line, "+inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(NULL, line, err, &args);
  TEST_CHECK(rc == false);
  parse_mailbox_array_free(&args);

  // Test NULL line
  TEST_CASE("NULL line");
  ARRAY_INIT(&args);
  rc = parse_mailboxes_args(&Mailboxes, NULL, err, &args);
  TEST_CHECK(rc == false);
  parse_mailbox_array_free(&args);

  // Test NULL err
  TEST_CASE("NULL err");
  ARRAY_INIT(&args);
  buf_strcpy(line, "+inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, NULL, &args);
  TEST_CHECK(rc == false);
  parse_mailbox_array_free(&args);

  // Test NULL args
  TEST_CASE("NULL args");
  buf_strcpy(line, "+inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, NULL);
  TEST_CHECK(rc == false);

  // Test empty line
  TEST_CASE("empty line");
  ARRAY_INIT(&args);
  buf_reset(err);
  buf_strcpy(line, "");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == false);
  TEST_CHECK(!buf_is_empty(err));
  parse_mailbox_array_free(&args);

  buf_pool_release(&err);
  buf_pool_release(&line);
}

/**
 * test_parse_mailboxes_args_simple - Test parse_mailboxes_args with simple mailbox paths
 */
static void test_parse_mailboxes_args_simple(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  bool rc;

  // Test single mailbox
  TEST_CASE("single mailbox");
  buf_strcpy(line, "+inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  struct ParseMailbox *pm = ARRAY_GET(&args, 0);
  TEST_CHECK(pm != NULL);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK(pm->label == NULL);
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_UNSET);
  parse_mailbox_array_free(&args);

  // Test multiple mailboxes
  TEST_CASE("multiple mailboxes");
  ARRAY_INIT(&args);
  buf_strcpy(line, "+inbox +sent +drafts");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 3);
  pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  pm = ARRAY_GET(&args, 1);
  TEST_CHECK_STR_EQ(pm->path, "+sent");
  pm = ARRAY_GET(&args, 2);
  TEST_CHECK_STR_EQ(pm->path, "+drafts");
  parse_mailbox_array_free(&args);

  buf_pool_release(&err);
  buf_pool_release(&line);
}

/**
 * test_parse_mailboxes_args_label - Test parse_mailboxes_args with -label and -nolabel flags
 */
static void test_parse_mailboxes_args_label(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  bool rc;

  // Test -label flag
  TEST_CASE("-label flag");
  buf_strcpy(line, "-label MyInbox +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  struct ParseMailbox *pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK_STR_EQ(pm->label, "MyInbox");
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_UNSET);
  parse_mailbox_array_free(&args);

  // Test -nolabel flag
  TEST_CASE("-nolabel flag");
  ARRAY_INIT(&args);
  buf_strcpy(line, "-nolabel +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK_STR_EQ(pm->label, "");
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_UNSET);
  parse_mailbox_array_free(&args);

  // Test -label without argument (error case)
  TEST_CASE("-label without argument");
  ARRAY_INIT(&args);
  buf_reset(err);
  buf_strcpy(line, "-label");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == false);
  TEST_CHECK(!buf_is_empty(err));
  parse_mailbox_array_free(&args);

  buf_pool_release(&err);
  buf_pool_release(&line);
}

/**
 * test_parse_mailboxes_args_notify - Test parse_mailboxes_args with -notify and -nonotify flags
 */
static void test_parse_mailboxes_args_notify(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  bool rc;

  // Test -notify flag
  TEST_CASE("-notify flag");
  buf_strcpy(line, "-notify +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  struct ParseMailbox *pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK(pm->label == NULL);
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_TRUE);
  parse_mailbox_array_free(&args);

  // Test -nonotify flag
  TEST_CASE("-nonotify flag");
  ARRAY_INIT(&args);
  buf_strcpy(line, "-nonotify +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK(pm->label == NULL);
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_FALSE);
  parse_mailbox_array_free(&args);

  buf_pool_release(&err);
  buf_pool_release(&line);
}

/**
 * test_parse_mailboxes_args_poll - Test parse_mailboxes_args with -poll and -nopoll flags
 */
static void test_parse_mailboxes_args_poll(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  bool rc;

  // Test -poll flag
  TEST_CASE("-poll flag");
  buf_strcpy(line, "-poll +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  struct ParseMailbox *pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK(pm->label == NULL);
  TEST_CHECK(pm->poll == TB_TRUE);
  TEST_CHECK(pm->notify == TB_UNSET);
  parse_mailbox_array_free(&args);

  // Test -nopoll flag
  TEST_CASE("-nopoll flag");
  ARRAY_INIT(&args);
  buf_strcpy(line, "-nopoll +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK(pm->label == NULL);
  TEST_CHECK(pm->poll == TB_FALSE);
  TEST_CHECK(pm->notify == TB_UNSET);
  parse_mailbox_array_free(&args);

  buf_pool_release(&err);
  buf_pool_release(&line);
}

/**
 * test_parse_mailboxes_args_combined - Test parse_mailboxes_args with combined flags
 */
static void test_parse_mailboxes_args_combined(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  bool rc;

  // Test all flags combined
  TEST_CASE("all flags combined");
  buf_strcpy(line, "-label MyInbox -notify -poll +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  struct ParseMailbox *pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK_STR_EQ(pm->label, "MyInbox");
  TEST_CHECK(pm->poll == TB_TRUE);
  TEST_CHECK(pm->notify == TB_TRUE);
  parse_mailbox_array_free(&args);

  // Test multiple mailboxes with mixed flags
  TEST_CASE("multiple mailboxes with mixed flags");
  ARRAY_INIT(&args);
  buf_strcpy(line, "+first -label Second -notify +second -nopoll +third");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&Mailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 3);

  pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+first");
  TEST_CHECK(pm->label == NULL);
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_UNSET);

  pm = ARRAY_GET(&args, 1);
  TEST_CHECK_STR_EQ(pm->path, "+second");
  TEST_CHECK_STR_EQ(pm->label, "Second");
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_TRUE);

  pm = ARRAY_GET(&args, 2);
  TEST_CHECK_STR_EQ(pm->path, "+third");
  TEST_CHECK(pm->label == NULL);
  TEST_CHECK(pm->poll == TB_FALSE);
  TEST_CHECK(pm->notify == TB_UNSET);
  parse_mailbox_array_free(&args);

  buf_pool_release(&err);
  buf_pool_release(&line);
}

/**
 * test_parse_mailboxes_args_named - Test parse_mailboxes_args with named-mailboxes command
 */
static void test_parse_mailboxes_args_named(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  bool rc;

  // Test named-mailboxes with single mailbox
  TEST_CASE("named-mailboxes single");
  buf_strcpy(line, "MyInbox +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&NamedMailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  struct ParseMailbox *pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK_STR_EQ(pm->label, "MyInbox");
  TEST_CHECK(pm->poll == TB_UNSET);
  TEST_CHECK(pm->notify == TB_UNSET);
  parse_mailbox_array_free(&args);

  // Test named-mailboxes with multiple mailboxes
  TEST_CASE("named-mailboxes multiple");
  ARRAY_INIT(&args);
  buf_strcpy(line, "Inbox +inbox Sent +sent");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&NamedMailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 2);

  pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK_STR_EQ(pm->label, "Inbox");

  pm = ARRAY_GET(&args, 1);
  TEST_CHECK_STR_EQ(pm->path, "+sent");
  TEST_CHECK_STR_EQ(pm->label, "Sent");
  parse_mailbox_array_free(&args);

  // Test named-mailboxes with missing mailbox (error case)
  TEST_CASE("named-mailboxes missing mailbox");
  ARRAY_INIT(&args);
  buf_reset(err);
  buf_strcpy(line, "JustALabel");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&NamedMailboxes, line, err, &args);
  TEST_CHECK(rc == false);
  TEST_CHECK(!buf_is_empty(err));
  parse_mailbox_array_free(&args);

  // Test named-mailboxes with -label flag override
  TEST_CASE("named-mailboxes with -label override");
  ARRAY_INIT(&args);
  buf_strcpy(line, "-label Override +inbox");
  buf_seek(line, 0);
  rc = parse_mailboxes_args(&NamedMailboxes, line, err, &args);
  TEST_CHECK(rc == true);
  TEST_CHECK(ARRAY_SIZE(&args) == 1);
  pm = ARRAY_GET(&args, 0);
  TEST_CHECK_STR_EQ(pm->path, "+inbox");
  TEST_CHECK_STR_EQ(pm->label, "Override");
  parse_mailbox_array_free(&args);

  buf_pool_release(&err);
  buf_pool_release(&line);
}

/**
 * test_parse_mailbox_free - Test parse_mailbox_free function
 */
static void test_parse_mailbox_free_func(void)
{
  // Test NULL input
  TEST_CASE("NULL input");
  parse_mailbox_free(NULL); // Should not crash

  // Test valid input
  TEST_CASE("valid input");
  struct ParseMailbox pm = {
    .path = mutt_str_dup("test/path"),
    .label = mutt_str_dup("test label"),
    .poll = TB_TRUE,
    .notify = TB_FALSE,
  };
  parse_mailbox_free(&pm);
  TEST_CHECK(pm.path == NULL);
  TEST_CHECK(pm.label == NULL);
}

/**
 * test_parse_mailbox_array_free - Test parse_mailbox_array_free function
 */
static void test_parse_mailbox_array_free_func(void)
{
  // Test NULL input
  TEST_CASE("NULL input");
  parse_mailbox_array_free(NULL); // Should not crash

  // Test empty array
  TEST_CASE("empty array");
  struct ParseMailboxArray args = ARRAY_HEAD_INITIALIZER;
  parse_mailbox_array_free(&args);
  TEST_CHECK(ARRAY_SIZE(&args) == 0);

  // Test populated array
  TEST_CASE("populated array");
  ARRAY_INIT(&args);
  struct ParseMailbox pm1 = {
    .path = mutt_str_dup("path1"),
    .label = mutt_str_dup("label1"),
    .poll = TB_TRUE,
    .notify = TB_FALSE,
  };
  struct ParseMailbox pm2 = {
    .path = mutt_str_dup("path2"),
    .label = NULL,
    .poll = TB_UNSET,
    .notify = TB_UNSET,
  };
  ARRAY_ADD(&args, pm1);
  ARRAY_ADD(&args, pm2);
  parse_mailbox_array_free(&args);
  TEST_CHECK(ARRAY_SIZE(&args) == 0);
}

static void test_parse_mailboxes2(void)
{
  // enum CommandResult parse_mailboxes(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; MailboxesTests[i].line; i++)
  {
    TEST_CASE(MailboxesTests[i].line);
    buf_reset(err);
    buf_strcpy(line, MailboxesTests[i].line);
    buf_seek(line, 0);
    rc = parse_mailboxes(&Mailboxes, line, NULL, NULL);
    TEST_CHECK_NUM_EQ(rc, MailboxesTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_named_mailboxes(void)
{
  // enum CommandResult parse_mailboxes(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; NamedMailboxesTests[i].line; i++)
  {
    TEST_CASE(NamedMailboxesTests[i].line);
    buf_reset(err);
    buf_strcpy(line, NamedMailboxesTests[i].line);
    buf_seek(line, 0);
    rc = parse_mailboxes(&NamedMailboxes, line, NULL, NULL);
    TEST_CHECK_NUM_EQ(rc, NamedMailboxesTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_mailboxes(void)
{
  // Test parse_mailboxes_args() function
  test_parse_mailboxes_args_degenerate();
  test_parse_mailboxes_args_simple();
  test_parse_mailboxes_args_label();
  test_parse_mailboxes_args_notify();
  test_parse_mailboxes_args_poll();
  test_parse_mailboxes_args_combined();
  test_parse_mailboxes_args_named();

  // Test helper functions
  test_parse_mailbox_free_func();
  test_parse_mailbox_array_free_func();

  // Test full parse_mailboxes() function (existing tests)
  test_parse_mailboxes2();
  test_parse_named_mailboxes();
}
