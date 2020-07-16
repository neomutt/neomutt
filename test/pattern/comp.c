/**
 * @file
 * Test code for Patterns
 *
 * @authors
 * Copyright (C) 2018 Naveen Nathan <naveen@lastninja.net>
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
#define MAIN_C 1
#include "config.h"
#include "acutest.h"
#include <assert.h>
#include <string.h>
#include "mutt/buffer.h"
#include "mutt/memory.h"
#include "alias/lib.h"
#include "mutt_globals.h"
#include "pattern/lib.h"

bool ResumeEditedDraftFiles;

/* All tests are limited to patterns that are string_match type only,
 * such as =s, =b, =f, etc.
 *
 * Rationale: (1) there is no way to compare regex types as "equal",
 *            (2) comparing Group is a pain in the arse,
 *            (3) similarly comparing lists (ListHead) is annoying.
 */

/* canonical representation of Pattern "tree",
 * s specifies a caller allocated buffer to write the string,
 * pat specifies the pattern,
 * indent specifies the indentation level (set to 0 if pat is root of tree),
 * returns the number of characters written to s (not including trailing '\0')
 *
 * A pattern tree with patterns a, b, c, d, e, f, g can be represented graphically
 * as follows (where a is obviously the root):
 *
 *        +-c-+
 *        |   |
 *    +-b-+   +-d
 *    |   |
 *  a-+   +-e
 *    |
 *    +-f-+
 *        |
 *        +-g
 *
 *  Let left child represent "next" pattern, and right as "child" pattern.
 *
 *  Then we can convert the above into a textual representation as follows:
 *    {a}
 *      {b}
 *        {c}
 *        {d}
 *      {e}
 *    {f}
 *    {g}
 *
 *  {a} is the root pattern with child pattern {b} (note: 2 space indent)
 *  and next pattern {f} (same indent).
 *  {b} has child {c} and next pattern {e}.
 *  {c} has next pattern {d}.
 *  {f} has next pattern {g}.
 *
 *  In the representation {a} is expanded to all the pattern fields.
 */
static int canonical_pattern(char *s, struct PatternList *pat, int indent)
{
  if (!pat || !s)
    return 0;

  char *p = s;

  for (int i = 0; i < 2 * indent; i++)
    p += sprintf(p, " ");

  struct Pattern *e = NULL;

  // p += sprintf(p, "");
  *p = '\0';

  SLIST_FOREACH(e, pat, entries)
  {
    p += sprintf(p, "{");
    p += sprintf(p, "%d,", e->op);
    p += sprintf(p, "%d,", e->pat_not);
    p += sprintf(p, "%d,", e->all_addr);
    p += sprintf(p, "%d,", e->string_match);
    p += sprintf(p, "%d,", e->group_match);
    p += sprintf(p, "%d,", e->ign_case);
    p += sprintf(p, "%d,", e->is_alias);
    p += sprintf(p, "%d,", e->is_multi);
    p += sprintf(p, "%d,", e->min);
    p += sprintf(p, "%d,", e->max);
    p += sprintf(p, "\"%s\",", e->p.str ? e->p.str : "");
    p += sprintf(p, "%s,", !e->child ? "(null)" : "(list)");
    p += sprintf(p, "%s", SLIST_NEXT(e, entries) ? "(next)" : "(null)");
    p += sprintf(p, "}\n");

    p += canonical_pattern(p, e->child, indent + 1);
  }

  return p - s;
}

/* best-effort pattern tree compare, returns 0 if equal otherwise 1 */
static int cmp_pattern(struct PatternList *p1, struct PatternList *p2)
{
  if (!p1 || !p2)
    return !(!p1 && !p2);

  struct PatternList p1_tmp = *p1;
  struct PatternList p2_tmp = *p2;

  while (!SLIST_EMPTY(&p1_tmp))
  {
    struct Pattern *l = SLIST_FIRST(&p1_tmp);
    struct Pattern *r = SLIST_FIRST(&p2_tmp);

    /* if l is NULL then r must be NULL (and vice-versa) */
    if ((!l || !r) && !(!l && !r))
      return 1;

    SLIST_REMOVE_HEAD(&p1_tmp, entries);
    SLIST_REMOVE_HEAD(&p2_tmp, entries);

    if (l->op != r->op)
      return 1;
    if (l->pat_not != r->pat_not)
      return 1;
    if (l->all_addr != r->all_addr)
      return 1;
    if (l->string_match != r->string_match)
      return 1;
    if (l->group_match != r->group_match)
      return 1;
    if (l->ign_case != r->ign_case)
      return 1;
    if (l->is_alias != r->is_alias)
      return 1;
    if (l->is_multi != r->is_multi)
      return 1;
    if (l->min != r->min)
      return 1;
    if (l->max != r->max)
      return 1;

    if (l->string_match && strcmp(l->p.str, r->p.str))
      return 1;

    if (cmp_pattern(l->child, r->child))
      return 1;
  }

  return 0;
}

void test_mutt_pattern_comp(void)
{
  struct Buffer err = mutt_buffer_make(1024);

  { /* empty */
    char *s = "";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == NULL");
      TEST_MSG("Actual  : pat != NULL");
    }

    char *msg = "empty pattern";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }
  }

  { /* invalid */
    char *s = "x";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == NULL");
      TEST_MSG("Actual  : pat != NULL");
    }

    char *msg = "error in pattern at: x";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }
  }

  { /* missing parameter */
    char *s = "=s";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == NULL");
      TEST_MSG("Actual  : pat != NULL");
    }

    char *msg = "missing parameter";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }
  }

  { /* error in pattern */
    char *s = "| =s foo";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == NULL");
      TEST_MSG("Actual  : pat != NULL");
    }

    char *msg = "error in pattern at: | =s foo";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }
  }

  {
    char *s = "=s foobar";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != NULL");
      TEST_MSG("Actual  : pat == NULL");
    }

    struct PatternList expected;
    SLIST_INIT(&expected);
    struct Pattern e = { .op = MUTT_PAT_SUBJECT,
                         .pat_not = false,
                         .all_addr = false,
                         .string_match = true,
                         .group_match = false,
                         .ign_case = true,
                         .is_alias = false,
                         .is_multi = false,
                         .min = 0,
                         .max = 0,
                         .p.str = "foobar" };
    SLIST_INSERT_HEAD(&expected, &e, entries);

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s2[1024];
      canonical_pattern(s2, &expected, 0);
      TEST_MSG("Expected:\n%s", s2);
      canonical_pattern(s2, pat, 0);
      TEST_MSG("Actual:\n%s", s2);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  {
    char *s = "! =s foobar";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != NULL");
      TEST_MSG("Actual  : pat == NULL");
    }

    struct PatternList expected;
    SLIST_INIT(&expected);
    struct Pattern e = { .op = MUTT_PAT_SUBJECT,
                         .pat_not = true,
                         .all_addr = false,
                         .string_match = true,
                         .group_match = false,
                         .ign_case = true,
                         .is_alias = false,
                         .is_multi = false,
                         .min = 0,
                         .max = 0,
                         .p.str = "foobar" };

    SLIST_INSERT_HEAD(&expected, &e, entries);

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s2[1024];
      canonical_pattern(s2, &expected, 0);
      TEST_MSG("Expected:\n%s", s2);
      canonical_pattern(s2, pat, 0);
      TEST_MSG("Actual:\n%s", s2);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  {
    char *s = "=s foo =s bar";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != NULL");
      TEST_MSG("Actual  : pat == NULL");
    }

    struct PatternList expected;

    struct Pattern e[3] = { /* root */
                            { .op = MUTT_PAT_AND,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = false,
                              .group_match = false,
                              .ign_case = false,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = NULL },
                            /* root->child */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "foo" },
                            /* root->child->next */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "bar" }
    };

    SLIST_INIT(&expected);
    SLIST_INSERT_HEAD(&expected, &e[0], entries);
    struct PatternList child;
    SLIST_INIT(&child);
    e[0].child = &child;
    SLIST_INSERT_HEAD(e[0].child, &e[1], entries);
    SLIST_INSERT_AFTER(&e[1], &e[2], entries);

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s2[1024];
      canonical_pattern(s2, &expected, 0);
      TEST_MSG("Expected:\n%s", s2);
      canonical_pattern(s2, pat, 0);
      TEST_MSG("Actual:\n%s", s2);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  {
    char *s = "(=s foo =s bar)";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != NULL");
      TEST_MSG("Actual  : pat == NULL");
    }

    struct PatternList expected;

    struct Pattern e[3] = { /* root */
                            { .op = MUTT_PAT_AND,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = false,
                              .group_match = false,
                              .ign_case = false,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = NULL },
                            /* root->child */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "foo" },
                            /* root->child->next */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "bar" }
    };

    SLIST_INIT(&expected);
    SLIST_INSERT_HEAD(&expected, &e[0], entries);
    struct PatternList child;
    SLIST_INIT(&child);
    e[0].child = &child;
    SLIST_INSERT_HEAD(e[0].child, &e[1], entries);
    SLIST_INSERT_AFTER(&e[1], &e[2], entries);

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s2[1024];
      canonical_pattern(s2, &expected, 0);
      TEST_MSG("Expected:\n%s", s2);
      canonical_pattern(s2, pat, 0);
      TEST_MSG("Actual:\n%s", s2);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  {
    char *s = "! (=s foo =s bar)";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != NULL");
      TEST_MSG("Actual  : pat == NULL");
    }

    struct PatternList expected;

    struct Pattern e[3] = { /* root */
                            { .op = MUTT_PAT_AND,
                              .pat_not = true,
                              .all_addr = false,
                              .string_match = false,
                              .group_match = false,
                              .ign_case = false,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = NULL },
                            /* root->child */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "foo" },
                            /* root->child->next */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "bar" }
    };

    SLIST_INIT(&expected);
    SLIST_INSERT_HEAD(&expected, &e[0], entries);
    struct PatternList child;
    SLIST_INIT(&child);
    e[0].child = &child;
    SLIST_INSERT_HEAD(e[0].child, &e[1], entries);
    SLIST_INSERT_AFTER(&e[1], &e[2], entries);

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s2[1024];
      canonical_pattern(s2, &expected, 0);
      TEST_MSG("Expected:\n%s", s2);
      canonical_pattern(s2, pat, 0);
      TEST_MSG("Actual:\n%s", s2);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  {
    char *s = "=s foo =s bar =s quux";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != NULL");
      TEST_MSG("Actual  : pat == NULL");
    }

    struct PatternList expected;

    struct Pattern e[4] = { /* root */
                            { .op = MUTT_PAT_AND,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = false,
                              .group_match = false,
                              .ign_case = false,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = NULL },
                            /* root->child */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "foo" },
                            /* root->child->next */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "bar" },
                            /* root->child->next->next */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "quux" }
    };

    SLIST_INIT(&expected);
    SLIST_INSERT_HEAD(&expected, &e[0], entries);
    struct PatternList child;
    e[0].child = &child;
    SLIST_INSERT_HEAD(e[0].child, &e[1], entries);
    SLIST_INSERT_AFTER(&e[1], &e[2], entries);
    SLIST_INSERT_AFTER(&e[2], &e[3], entries);

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s2[1024];
      canonical_pattern(s2, &expected, 0);
      TEST_MSG("Expected:\n%s", s2);
      canonical_pattern(s2, pat, 0);
      TEST_MSG("Actual:\n%s", s2);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  {
    char *s = "!(=s foo|=s bar) =s quux";

    mutt_buffer_reset(&err);
    struct PatternList *pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != NULL");
      TEST_MSG("Actual  : pat == NULL");
    }

    struct PatternList expected;

    struct Pattern e[5] = { /* root */
                            { .op = MUTT_PAT_AND,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = false,
                              .group_match = false,
                              .ign_case = false,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = NULL },
                            /* root->child */
                            { .op = MUTT_PAT_OR,
                              .pat_not = true,
                              .all_addr = false,
                              .string_match = false,
                              .group_match = false,
                              .ign_case = false,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = NULL },
                            /* root->child->child */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "foo" },
                            /* root->child->child->next */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "bar" },
                            /* root->child->next */
                            { .op = MUTT_PAT_SUBJECT,
                              .pat_not = false,
                              .all_addr = false,
                              .string_match = true,
                              .group_match = false,
                              .ign_case = true,
                              .is_alias = false,
                              .is_multi = false,
                              .min = 0,
                              .max = 0,
                              .p.str = "quux" }
    };

    SLIST_INIT(&expected);
    SLIST_INSERT_HEAD(&expected, &e[0], entries);
    struct PatternList child1, child2;
    SLIST_INIT(&child1);
    e[0].child = &child1;
    SLIST_INSERT_HEAD(e[0].child, &e[1], entries);
    SLIST_INIT(&child2);
    e[1].child = &child2;
    SLIST_INSERT_HEAD(e[1].child, &e[2], entries);
    SLIST_INSERT_AFTER(&e[2], &e[3], entries);
    SLIST_INSERT_AFTER(&e[1], &e[4], entries);

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s2[1024];
      canonical_pattern(s2, &expected, 0);
      TEST_MSG("Expected:\n%s", s2);
      canonical_pattern(s2, pat, 0);
      TEST_MSG("Actual:\n%s", s2);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  mutt_buffer_dealloc(&err);
}
