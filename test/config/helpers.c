/**
 * @file
 * Test code for the Config helper functions
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
#include "mutt/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

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
  { "date",          SORT_DATE },
  { "date-sent",     SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "unsorted",      SORT_ORDER },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "threads",       SORT_THREADS },
  { "to",            SORT_TO },
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
  { "Elderberry", DT_ADDRESS,                        IP "elderberry@example.com", 0,                   NULL, },
  { "Fig",        DT_STRING|DT_COMMAND|DT_NOT_EMPTY, IP "fig",                    0,                   NULL, },
  { "Guava",      DT_LONG,                           0,                           0,                   NULL, },
  { "Hawthorn",   DT_ENUM,                           2,                           IP &MboxTypeDef,     NULL, },
  { "Ilama",      DT_MBTABLE,                        IP "abcdef",                 0,                   NULL, },
  { "Jackfruit",  DT_PATH|DT_PATH_FILE,              IP "/etc/passwd",            0,                   NULL, },
  { "Kumquat",    DT_QUAD,                           0,                           0,                   NULL, },
  { "Lemon",      DT_REGEX,                          0,                           0,                   NULL, },
  { "Mango",      DT_SORT,                           1,                           IP SortMangoMethods, NULL, },
  { "Nectarine",  DT_STRING|DT_SENSITIVE,            IP "nectarine",              0,                   NULL, },
  { "Olive",      DT_SLIST,                          IP "olive",                  IP "olive",          NULL, },
  { NULL },
};
// clang-format on

static struct ConfigSet *create_sample_data(void)
{
  struct ConfigSet *cs = cs_new(30);
  if (!cs)
    return NULL;

  cs_register_type(cs, &CstAddress);
  cs_register_type(cs, &CstBool);
  cs_register_type(cs, &CstEnum);
  cs_register_type(cs, &CstLong);
  cs_register_type(cs, &CstMbtable);
  cs_register_type(cs, &CstNumber);
  cs_register_type(cs, &CstPath);
  cs_register_type(cs, &CstQuad);
  cs_register_type(cs, &CstPath);
  cs_register_type(cs, &CstRegex);
  cs_register_type(cs, &CstSlist);
  cs_register_type(cs, &CstSort);
  cs_register_type(cs, &CstString);

  if (!cs_register_variables(cs, Vars, 0))
    return NULL;

  return cs;
}

void test_config_helpers(void)
{
  struct ConfigSet *cs = create_sample_data();
  if (!cs)
    return;

  NeoMutt = neomutt_new(cs);
  if (!NeoMutt)
    return;

  struct ConfigSubset *sub = cs_subset_new(NULL, NULL, NULL);
  if (!sub)
    return;
  sub->cs = cs;

  TEST_CHECK(cs_subset_address(sub, "Elderberry") != NULL);
  TEST_CHECK(cs_subset_bool(sub, "Apple") == false);
  TEST_CHECK(cs_subset_enum(sub, "Hawthorn") == 2);
  TEST_CHECK(cs_subset_long(sub, "Guava") == 0);
  TEST_CHECK(
      mutt_str_equal(cs_subset_mbtable(sub, "Ilama")->orig_str, "abcdef"));
  TEST_CHECK(cs_subset_number(sub, "Cherry") == 0);
  TEST_CHECK(mutt_str_equal(cs_subset_path(sub, "Jackfruit"), "/etc/passwd"));
  TEST_CHECK(cs_subset_quad(sub, "Kumquat") == 0);
  TEST_CHECK(cs_subset_regex(sub, "Lemon") == 0);
  TEST_CHECK(cs_subset_slist(sub, "Olive") != NULL);
  TEST_CHECK(cs_subset_sort(sub, "Mango") == 1);
  TEST_CHECK(mutt_str_equal(cs_subset_string(sub, "Nectarine"), "nectarine"));

  neomutt_free(&NeoMutt);
  cs_subset_free(&sub);
  cs_free(&cs);
}
