#define TEST_NO_MAIN
#define MAIN_C 1
#include "acutest.h"

#include <assert.h>
#include <string.h>
#include "mutt/buffer.h"
#include "mutt/memory.h"
#include "pattern.h"

#include "alias.h"
#include "globals.h"
bool ResumeEditedDraftFiles;

/* All tests are limited to patterns that are stringmatch type only,
 * such as =s, =b, =f, etc.
 *
 * Rationale: (1) there is no way to compare regex types as "equal",
 *            (2) comparing Group is a pain in the arse,
 *            (3) similarly comparing lists (ListHead) is annoying.
 */

/* canoncial representation of Pattern "tree",
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
static int canonical_pattern(char *s, struct Pattern *pat, int indent)
{
  char *p = s;

  for (int i = 0; i < 2*indent; i++) {
    p += sprintf(p, " ");
  }
  p += sprintf(p, "{");
  p += sprintf(p, "%d,", pat->op);
  p += sprintf(p, "%d,", pat->not);
  p += sprintf(p, "%d,", pat->alladdr);
  p += sprintf(p, "%d,", pat->stringmatch);
  p += sprintf(p, "%d,", pat->groupmatch);
  p += sprintf(p, "%d,", pat->ign_case);
  p += sprintf(p, "%d,", pat->isalias);
  p += sprintf(p, "%d,", pat->ismulti);
  p += sprintf(p, "%d,", pat->min);
  p += sprintf(p, "%d,", pat->max);
  p += sprintf(p, "\"%s\",", pat->p.str ? pat->p.str : "");
  p += sprintf(p, "%s,", pat->child ? "(ptr)" : "(null)");
  p += sprintf(p, "%s", pat->next ? "(ptr)" : "(null)");
  p += sprintf(p, "}\n");

  if (pat->child)
  {
    p += canonical_pattern(p, pat->child, indent + 1);
  }

  if (pat->next)
    p += canonical_pattern(p, pat->next, indent);

  return p - s;
}

/* best-effort pattern tree compare, returns 0 if equal otherwise 1 */
static int cmp_pattern(struct Pattern *p1, struct Pattern *p2)
{
  if (!p1 || !p2)
    return p1 || p2;

  if (p1->op != p2->op)
    return 1;
  if (p1->not != p2->not)
    return 1;
  if (p1->alladdr != p2->alladdr)
    return 1;
  if (p1->stringmatch != p2->stringmatch)
    return 1;
  if (p1->groupmatch != p2->groupmatch)
    return 1;
  if (p1->ign_case != p2->ign_case)
    return 1;
  if (p1->isalias != p2->isalias)
    return 1;
  if (p1->ismulti != p2->ismulti)
    return 1;
  if (p1->min != p2->min)
    return 1;
  if (p1->max != p2->max)
    return 1;

  if (p1->stringmatch && strcmp(p1->p.str, p2->p.str))
    return 1;

  if (cmp_pattern(p1->child, p2->child))
    return 1;

  return cmp_pattern(p1->next, p2->next);
}

void test_mutt_pattern_comp(void)
{
  struct Buffer err;
  struct Pattern *pat;

  err.dsize = 1024;
  err.data = mutt_mem_malloc(err.dsize);

  { /* empty */
    char *s = "";

    mutt_buffer_reset(&err);
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == 0");
      TEST_MSG("Actual  : pat == %p", pat);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == 0");
      TEST_MSG("Actual  : pat == %p", pat);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == 0");
      TEST_MSG("Actual  : pat == %p", pat);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(!pat))
    {
      TEST_MSG("Expected: pat == 0");
      TEST_MSG("Actual  : pat == %p", pat);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != 0");
      TEST_MSG("Actual  : pat == %p", pat);
    }

    struct Pattern expected = { .op = 30 /* MUTT_SUBJECT */,
                                .not = 0,
                                .alladdr = 0,
                                .stringmatch = 1,
                                .groupmatch = 0,
                                .ign_case = 1,
                                .isalias = 0,
                                .ismulti = 0,
                                .min = 0,
                                .max = 0,
                                .next = NULL,
                                .child = NULL,
                                .p.str = "foobar" };

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s[1024];
      canonical_pattern(s, &expected, 0);
      TEST_MSG("Expected:\n%s", s);
      canonical_pattern(s, pat, 0);
      TEST_MSG("Actual:\n%s", s);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != 0");
      TEST_MSG("Actual  : pat == %p", pat);
    }

    struct Pattern expected = { .op = 30 /* MUTT_SUBJECT */,
                                .not = 1,
                                .alladdr = 0,
                                .stringmatch = 1,
                                .groupmatch = 0,
                                .ign_case = 1,
                                .isalias = 0,
                                .ismulti = 0,
                                .min = 0,
                                .max = 0,
                                .next = NULL,
                                .child = NULL,
                                .p.str = "foobar" };

    if (!TEST_CHECK(!cmp_pattern(pat, &expected)))
    {
      char s[1024];
      canonical_pattern(s, &expected, 0);
      TEST_MSG("Expected:\n%s", s);
      canonical_pattern(s, pat, 0);
      TEST_MSG("Actual:\n%s", s);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != 0");
      TEST_MSG("Actual  : pat == %p", pat);
    }

    struct Pattern expected[3] = { /* root */
                                   { .op = 22 /* MUTT_AND */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 0,
                                     .groupmatch = 0,
                                     .ign_case = 0,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = NULL },
                                   /* root->child */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "foo" },
                                   /* root->child->next */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "bar" }
    };

    expected[0].child = &expected[1];
    expected[1].next = &expected[2];

    if (!TEST_CHECK(!cmp_pattern(pat, &expected[0])))
    {
      char s[1024];
      canonical_pattern(s, &expected[0], 0);
      TEST_MSG("Expected:\n%s", s);
      canonical_pattern(s, pat, 0);
      TEST_MSG("Actual:\n%s", s);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != 0");
      TEST_MSG("Actual  : pat == %p", pat);
    }

    struct Pattern expected[3] = { /* root */
                                   { .op = 22 /* MUTT_AND */,
                                     .not = 1,
                                     .alladdr = 0,
                                     .stringmatch = 0,
                                     .groupmatch = 0,
                                     .ign_case = 0,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = NULL },
                                   /* root->child */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "foo" },
                                   /* root->child->next */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "bar" }
    };

    expected[0].child = &expected[1];
    expected[1].next = &expected[2];

    if (!TEST_CHECK(!cmp_pattern(pat, &expected[0])))
    {
      char s[1024];
      canonical_pattern(s, &expected[0], 0);
      TEST_MSG("Expected:\n%s", s);
      canonical_pattern(s, pat, 0);
      TEST_MSG("Actual:\n%s", s);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != 0");
      TEST_MSG("Actual  : pat == %p", pat);
    }

    struct Pattern expected[4] = { /* root */
                                   { .op = 22 /* MUTT_AND */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 0,
                                     .groupmatch = 0,
                                     .ign_case = 0,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = NULL },
                                   /* root->child */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "foo" },
                                   /* root->child->next */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "bar" },
                                   /* root->child->next->next */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "quux" }
    };

    expected[0].child = &expected[1];
    expected[1].next = &expected[2];
    expected[2].next = &expected[3];

    if (!TEST_CHECK(!cmp_pattern(pat, &expected[0])))
    {
      char s[1024];
      canonical_pattern(s, &expected[0], 0);
      TEST_MSG("Expected:\n%s", s);
      canonical_pattern(s, pat, 0);
      TEST_MSG("Actual:\n%s", s);
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
    pat = mutt_pattern_comp(s, 0, &err);

    if (!TEST_CHECK(pat != NULL))
    {
      TEST_MSG("Expected: pat != 0");
      TEST_MSG("Actual  : pat == %p", pat);
    }

    struct Pattern expected[5] = { /* root */
                                   { .op = 22 /* MUTT_AND */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 0,
                                     .groupmatch = 0,
                                     .ign_case = 0,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = NULL },
                                   /* root->child */
                                   { .op = 23 /* MUTT_OR */,
                                     .not = 1,
                                     .alladdr = 0,
                                     .stringmatch = 0,
                                     .groupmatch = 0,
                                     .ign_case = 0,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = NULL },
                                   /* root->child->child */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "foo" },
                                   /* root->child->child->next */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "bar" },
                                   /* root->child->next */
                                   { .op = 30 /* MUTT_SUBJECT */,
                                     .not = 0,
                                     .alladdr = 0,
                                     .stringmatch = 1,
                                     .groupmatch = 0,
                                     .ign_case = 1,
                                     .isalias = 0,
                                     .ismulti = 0,
                                     .min = 0,
                                     .max = 0,
                                     .next = NULL,
                                     .child = NULL,
                                     .p.str = "quux" }
    };

    expected[0].child = &expected[1];
    expected[1].child = &expected[2];
    expected[2].next = &expected[3];
    expected[1].next = &expected[4];

    if (!TEST_CHECK(!cmp_pattern(pat, &expected[0])))
    {
      char s[1024];
      canonical_pattern(s, &expected[0], 0);
      TEST_MSG("Expected:\n%s", s);
      canonical_pattern(s, pat, 0);
      TEST_MSG("Actual:\n%s", s);
    }

    char *msg = "";
    if (!TEST_CHECK(!strcmp(err.data, msg)))
    {
      TEST_MSG("Expected: %s", msg);
      TEST_MSG("Actual  : %s", err.data);
    }

    mutt_pattern_free(&pat);
  }

  FREE(&err.data);
}
