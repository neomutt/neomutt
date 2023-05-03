/**
 * @file
 * Common code for file tests
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"

struct MuttWindow;
struct PagerView;

bool StartupComplete = true;

char *HomeDir = NULL;
int SigInt = 0;
int SigWinch = 0;
char *ShortHostname = "example";

#define TEST_DIR "NEOMUTT_TEST_DIR"

static struct ConfigDef Vars[] = {
  // clang-format off
  { "assumed_charset", DT_SLIST|SLIST_SEP_COLON|SLIST_ALLOW_EMPTY, 0, 0, NULL, },
  { "charset", DT_STRING|DT_NOT_EMPTY|DT_CHARSET_SINGLE, IP "utf-8", 0, NULL, },
  { "tmp_dir", DT_PATH|DT_PATH_DIR|DT_NOT_EMPTY, IP TMPDIR, 0, NULL, },
  { NULL },
  // clang-format on
};

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

static const char *get_test_dir(void)
{
  return mutt_str_getenv(TEST_DIR);
}

static void init_tmp_dir(struct NeoMutt *n)
{
  char buf[PATH_MAX] = { 0 };

  snprintf(buf, sizeof(buf), "%s/tmp", mutt_str_getenv(TEST_DIR));

  cs_str_initial_set(n->sub->cs, "tmp_dir", buf, NULL);
  cs_str_reset(n->sub->cs, "tmp_dir", NULL);
}

void test_gen_path(char *buf, size_t buflen, const char *fmt)
{
  snprintf(buf, buflen, NONULL(fmt), NONULL(get_test_dir()));
}

bool test_neomutt_create(void)
{
  struct ConfigSet *cs = cs_new(50);
  CONFIG_INIT_TYPE(cs, Address);
  CONFIG_INIT_TYPE(cs, Bool);
  CONFIG_INIT_TYPE(cs, Enum);
  CONFIG_INIT_TYPE(cs, Long);
  CONFIG_INIT_TYPE(cs, Mbtable);
  CONFIG_INIT_TYPE(cs, MyVar);
  CONFIG_INIT_TYPE(cs, Number);
  CONFIG_INIT_TYPE(cs, Path);
  CONFIG_INIT_TYPE(cs, Quad);
  CONFIG_INIT_TYPE(cs, Regex);
  CONFIG_INIT_TYPE(cs, Slist);
  CONFIG_INIT_TYPE(cs, Sort);
  CONFIG_INIT_TYPE(cs, String);

  NeoMutt = neomutt_new(cs);
  TEST_CHECK(NeoMutt != NULL);

  TEST_CHECK(cs_register_variables(cs, Vars, DT_NO_FLAGS));

  init_tmp_dir(NeoMutt);

  return NeoMutt;
}

void test_neomutt_destroy(void)
{
  struct ConfigSet *cs = NeoMutt->sub->cs;
  neomutt_free(&NeoMutt);
  cs_free(&cs);
}

void test_init(void)
{
  const char *path = get_test_dir();
  bool success = false;

  TEST_CASE("Common setup");
  if (!TEST_CHECK(path != NULL))
  {
    TEST_MSG("Environment variable '%s' isn't set\n", TEST_DIR);
    goto done;
  }

  size_t len = strlen(path);
  if (!TEST_CHECK(path[len - 1] != '/'))
  {
    TEST_MSG("Environment variable '%s' mustn't end with a '/'\n", TEST_DIR);
    goto done;
  }

  struct stat st = { 0 };

  if (!TEST_CHECK(stat(path, &st) == 0))
  {
    TEST_MSG("Test dir '%s' doesn't exist\n", path);
    goto done;
  }

  if (!TEST_CHECK(S_ISDIR(st.st_mode) == true))
  {
    TEST_MSG("Test dir '%s' isn't a directory\n", path);
    goto done;
  }

  if (!TEST_CHECK((setlocale(LC_ALL, "C.UTF-8") != NULL) ||
                  (setlocale(LC_ALL, "en_US.UTF-8") != NULL)))
  {
    TEST_MSG("Can't set locale to C.UTF-8 or en_US.UTF-8");
    goto done;
  }

  test_neomutt_create();
  success = true;
done:
  if (!success)
    TEST_MSG("See: https://github.com/neomutt/neomutt-test-files#test-files\n");
}

void test_fini(void)
{
  test_neomutt_destroy();
  buf_pool_free();
}

struct IndexSharedData *index_shared_data_new(void)
{
  return NULL;
}

struct MuttWindow *add_panel_pager(struct MuttWindow *parent, bool status_on_top)
{
  return NULL;
}

enum QuadOption mutt_yesorno(const char *msg, enum QuadOption def)
{
  return MUTT_YES;
}

struct Mailbox *get_current_mailbox(void)
{
  return NULL;
}

struct Menu *get_current_menu(void)
{
  return NULL;
}

int mutt_do_pager(struct PagerView *pview, struct Email *e)
{
  return 0;
}
