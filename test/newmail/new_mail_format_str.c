/**
 * @file
 * Test code for new_mail_format_str()
 *
 * @authors
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
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

#include "mutt/string2.h"
#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "core/lib.h"
#include "newmail/lib.h"

void test_new_mail_format_str(void)
{
  // const char *new_mail_format_str(char *buf, size_t buflen, size_t col, int cols,
  //                                 char op, const char *src, const char *prec,
  //                                 const char *if_str, const char *else_str,
  //                                 intptr_t data, MuttFormatFlags flags);

  char buf[64];
  size_t col = 0;
  int cols = 64;
  struct Mailbox *mailbox = mailbox_new();
  mailbox->name = mutt_str_dup("MailBox");
  mailbox->pathbuf = buf_make(16);
  buf_strcpy(&mailbox->pathbuf, "/path");
  intptr_t data = (intptr_t) mailbox;

  new_mail_format_str((char *) buf, 64, col, cols, 'n', NULL, NULL, NULL, NULL, data, 0);
  TEST_CHECK(mutt_str_equal(buf, "MailBox"));
  TEST_MSG("Check failed: %s != MailBox", buf);

  new_mail_format_str((char *) buf, 64, col, cols, 'f', NULL, NULL, NULL, NULL, data, 0);
  TEST_CHECK(mutt_str_equal(buf, "/path"));
  TEST_MSG("Check failed: %s != /path", buf);

  mailbox_free(&mailbox);
}
