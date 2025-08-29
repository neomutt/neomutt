/**
 * @file
 * Test code for the Config helper functions
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

// clang-format off
static struct Mapping MboxTypeMap[] = {
  { "mbox",    MUTT_MBOX,    },
  { "MMDF",    MUTT_MMDF,    },
  { "MH",      MUTT_MH,      },
  { "Maildir", MUTT_MAILDIR, },
  { NULL,      0,            },
};

/**
 * Test Lookup table
 */
static const struct Mapping SortMangoMethods[] = {
  { "date",          EMAIL_SORT_DATE },
  { "date-sent",     EMAIL_SORT_DATE },
  { "date-received", EMAIL_SORT_DATE_RECEIVED },
  { "from",          EMAIL_SORT_FROM },
  { "label",         EMAIL_SORT_LABEL },
  { "unsorted",      EMAIL_SORT_UNSORTED },
  { "mailbox-order", EMAIL_SORT_UNSORTED },
  { "score",         EMAIL_SORT_SCORE },
  { "size",          EMAIL_SORT_SIZE },
  { "spam",          EMAIL_SORT_SPAM },
  { "subject",       EMAIL_SORT_SUBJECT },
  { "threads",       EMAIL_SORT_THREADS },
  { "to",            EMAIL_SORT_TO },
  { NULL,            0 },
};
// clang-format on

static struct EnumDef MboxTypeDef = {
  "mbox_type",
  4,
  (struct Mapping *) &MboxTypeMap,
};

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_BOOL,                           false,                       0,                   NULL, },
  { "Banana",     DT_BOOL,                           true,                        0,                   NULL, },
  { "Cherry",     DT_NUMBER,                         0,                           0,                   NULL, },
  { "Damson",     DT_SYNONYM,                        IP "Cherry",                 0,                   NULL, },
  { "Fig",        DT_STRING|D_STRING_COMMAND|D_NOT_EMPTY, IP "fig",               0,                   NULL, },
  { "Guava",      DT_LONG,                           0,                           0,                   NULL, },
  { "Hawthorn",   DT_ENUM,                           2,                           IP &MboxTypeDef,     NULL, },
  { "Ilama",      DT_MBTABLE,                        IP "abcdef",                 0,                   NULL, },
  { "Jackfruit",  DT_PATH|D_PATH_FILE,               IP "/etc/passwd",            0,                   NULL, },
  { "Kumquat",    DT_QUAD,                           0,                           0,                   NULL, },
  { "Lemon",      DT_REGEX,                          0,                           0,                   NULL, },
  { "Mango",      DT_SORT,                           EMAIL_SORT_DATE,             IP SortMangoMethods, NULL, },
  { "Nectarine",  DT_STRING|D_SENSITIVE,             IP "nectarine",              0,                   NULL, },
  { "Olive",      DT_SLIST,                          IP "olive",                  IP "olive",          NULL, },
  { NULL },
};
// clang-format on

void test_config_helpers(void)
{
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  MuttLogger = log_disp_null;

  struct ConfigSubset *sub = NeoMutt->sub;

  TEST_CHECK(cs_subset_bool(sub, "Apple") == false);
  TEST_CHECK(cs_subset_enum(sub, "Hawthorn") == 2);
  TEST_CHECK(cs_subset_long(sub, "Guava") == 0);
  TEST_CHECK_STR_EQ(cs_subset_mbtable(sub, "Ilama")->orig_str, "abcdef");
  TEST_CHECK(cs_subset_number(sub, "Cherry") == 0);
  TEST_CHECK_STR_EQ(cs_subset_path(sub, "Jackfruit"), "/etc/passwd");
  TEST_CHECK(cs_subset_quad(sub, "Kumquat") == 0);
  TEST_CHECK(cs_subset_regex(sub, "Lemon") == 0);
  TEST_CHECK(cs_subset_slist(sub, "Olive") != NULL);
  TEST_CHECK(cs_subset_sort(sub, "Mango") == EMAIL_SORT_DATE);
  TEST_CHECK_STR_EQ(cs_subset_string(sub, "Nectarine"), "nectarine");

  const char *name = "Apple";
  struct ConfigSet *cs = sub->cs;
  struct HashElem *he = cs_get_elem(cs, name);

  TEST_CHECK(!config_he_set_initial(NULL, NULL, "yes"));
  TEST_CHECK(config_he_set_initial(cs, he, "yes"));

  TEST_CHECK(!config_str_set_initial(NULL, NULL, "no"));
  TEST_CHECK(!config_str_set_initial(cs, "Unknown", "no"));
  TEST_CHECK(config_str_set_initial(cs, "Apple", "no"));
}
