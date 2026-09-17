/**
 * @file
 * Test code for mutt_save_attachment()
 *
 * @authors
 * Copyright (C) 2026 Ethan Stoner <ethanstoner08@gmail.com>
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "attach/lib.h"

static const char *Contents = "attachment contents\n";

static bool file_has_contents(const char *path)
{
  char buf[256] = { 0 };
  FILE *fp = fopen(path, "r");
  if (!fp)
    return false;
  size_t len = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  return (len == strlen(Contents)) && mutt_str_equal(buf, Contents);
}

void test_mutt_save_attachment(void)
{
  // int mutt_save_attachment(FILE *fp, struct Body *b, const char *path, enum SaveAttach opt, struct Email *e);

  char src[] = "/tmp/neomutt-test-save-src-XXXXXX";
  int fd = mkstemp(src);
  TEST_CHECK(fd >= 0);
  FILE *fp = fdopen(fd, "w");
  TEST_CHECK(fp != NULL);
  fputs(Contents, fp);
  fclose(fp);

  struct Body body = { 0 };
  body.type = TYPE_TEXT;
  body.filename = mutt_str_dup(src);

  {
    TEST_CASE("send mode: copy to a different file");
    char dest[] = "/tmp/neomutt-test-save-dest-XXXXXX";
    TEST_CHECK(mkstemp(dest) >= 0);
    TEST_CHECK(mutt_save_attachment(NULL, &body, dest, MUTT_SAVE_NONE, NULL) == 0);
    TEST_CHECK(file_has_contents(dest));
    unlink(dest);
  }

  {
    TEST_CASE("send mode: overwrite the source file");
    TEST_CHECK(mutt_save_attachment(NULL, &body, src, MUTT_SAVE_NONE, NULL) == -1);
    TEST_CHECK(file_has_contents(src));
  }

  {
    TEST_CASE("send mode: append to the source file");
    TEST_CHECK(mutt_save_attachment(NULL, &body, src, MUTT_SAVE_APPEND, NULL) == -1);
    TEST_CHECK(file_has_contents(src));
  }

  {
    TEST_CASE("send mode: overwrite the source file via a hard link");
    char hardlink[] = "/tmp/neomutt-test-save-link-XXXXXX";
    TEST_CHECK(mkstemp(hardlink) >= 0);
    unlink(hardlink);
    TEST_CHECK(link(src, hardlink) == 0);
    TEST_CHECK(mutt_save_attachment(NULL, &body, hardlink, MUTT_SAVE_NONE, NULL) == -1);
    TEST_CHECK(file_has_contents(src));
    unlink(hardlink);
  }

  FREE(&body.filename);
  unlink(src);
}
