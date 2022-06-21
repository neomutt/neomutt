/**
 * @file
 * Auto-completion helpers
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page complete_helpers Auto-completion helpers
 *
 * Auto-completion helpers
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h" // IWYU pragma: keep
#include "index/lib.h"
#include "notmuch/lib.h"
#include "functions.h"
#include "keymap.h"
#include "mutt_commands.h"
#include "menu/lib.h"
#include "myvar.h"
#include "sort.h"

// Initial string that starts completion. No telling how much the user has
// typed so far. Allocate 1024 just to be sure!
static char UserTyped[1024] = { 0 };

static int NumMatched = 0;          /* Number of matches for completion */
static char Completed[256] = { 0 }; /* completed string (command or variable) */
static const char **Matches;
/* this is a lie until mutt_init runs: */
static int MatchesListsize = 512; // Enough space for all of the config items

#ifdef USE_NOTMUCH
/* List of tags found in last call to mutt_nm_query_complete(). */
static char **nm_tags;
#endif

/**
 * complete_init - Setup auto-completion
 */
void complete_init(void)
{
  Matches = mutt_mem_calloc(MatchesListsize, sizeof(char *));
}

/**
 * matches_ensure_morespace - Allocate more space for auto-completion
 * @param current Current allocation
 */
static void matches_ensure_morespace(int current)
{
  if (current <= (MatchesListsize - 2))
    return;

  int base_space = 512; // Enough space for all of the config items
  int extra_space = MatchesListsize - base_space;
  extra_space *= 2;
  const int space = base_space + extra_space;
  mutt_mem_realloc(&Matches, space * sizeof(char *));
  memset(&Matches[current + 1], 0, space - current);
  MatchesListsize = space;
}

/**
 * candidate - Helper function for completion
 * @param user User entered data for completion
 * @param src  Candidate for completion
 * @param dest Completion result gets here
 * @param dlen Length of dest buffer
 *
 * Changes the dest buffer if necessary/possible to aid completion.
 */
static void candidate(char *user, const char *src, char *dest, size_t dlen)
{
  if (!dest || !user || !src)
    return;

  if (strstr(src, user) != src)
    return;

  matches_ensure_morespace(NumMatched);
  Matches[NumMatched++] = src;
  if (dest[0] == '\0')
    mutt_str_copy(dest, src, dlen);
  else
  {
    int l;
    for (l = 0; src[l] && src[l] == dest[l]; l++)
      ; // do nothing

    dest[l] = '\0';
  }
}

#ifdef USE_NOTMUCH
/**
 * complete_all_nm_tags - Pass a list of Notmuch tags to the completion code
 * @param pt List of all Notmuch tags
 * @retval  0 Success
 * @retval -1 Error
 */
static int complete_all_nm_tags(const char *pt)
{
  struct Mailbox *m_cur = get_current_mailbox();
  int tag_count_1 = 0;
  int tag_count_2 = 0;

  NumMatched = 0;
  mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
  memset(Matches, 0, MatchesListsize);
  memset(Completed, 0, sizeof(Completed));

  nm_db_longrun_init(m_cur, false);

  /* Work out how many tags there are. */
  if (nm_get_all_tags(m_cur, NULL, &tag_count_1) || (tag_count_1 == 0))
    goto done;

  /* Free the old list, if any. */
  if (nm_tags)
  {
    for (int i = 0; nm_tags[i]; i++)
      FREE(&nm_tags[i]);
    FREE(&nm_tags);
  }
  /* Allocate a new list, with sentinel. */
  nm_tags = mutt_mem_malloc((tag_count_1 + 1) * sizeof(char *));
  nm_tags[tag_count_1] = NULL;

  /* Get all the tags. */
  if (nm_get_all_tags(m_cur, nm_tags, &tag_count_2) || (tag_count_1 != tag_count_2))
  {
    FREE(&nm_tags);
    nm_tags = NULL;
    nm_db_longrun_done(m_cur);
    return -1;
  }

  /* Put them into the completion machinery. */
  for (int num = 0; num < tag_count_1; num++)
  {
    candidate(UserTyped, nm_tags[num], Completed, sizeof(Completed));
  }

  matches_ensure_morespace(NumMatched);
  Matches[NumMatched++] = UserTyped;

done:
  nm_db_longrun_done(m_cur);
  return 0;
}
#endif

/**
 * mutt_command_complete - Complete a command name
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_command_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  char *pt = buf;
  int spaces; /* keep track of the number of leading spaces on the line */
  struct MyVar *myv = NULL;

  SKIPWS(buf);
  spaces = buf - pt;

  pt = buf + pos - spaces;
  while ((pt > buf) && !isspace((unsigned char) *pt))
    pt--;

  if (pt == buf) /* complete cmd */
  {
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));

      struct Command *c = NULL;
      for (size_t num = 0, size = mutt_commands_array(&c); num < size; num++)
        candidate(UserTyped, c[num].name, Completed, sizeof(Completed));
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (UserTyped[0] == '\0')
        return 1;
    }

    if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    /* return the completed command */
    strncpy(buf, Completed, buflen - spaces);
  }
  else if (mutt_str_startswith(buf, "set") || mutt_str_startswith(buf, "unset") ||
           mutt_str_startswith(buf, "reset") || mutt_str_startswith(buf, "toggle"))
  { /* complete variables */
    static const char *const prefixes[] = { "no", "inv", "?", "&", 0 };

    pt++;
    /* loop through all the possible prefixes (no, inv, ...) */
    if (mutt_str_startswith(buf, "set"))
    {
      for (int num = 0; prefixes[num]; num++)
      {
        if (mutt_str_startswith(pt, prefixes[num]))
        {
          pt += mutt_str_len(prefixes[num]);
          break;
        }
      }
    }

    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));

      struct HashElem *he = NULL;
      struct HashElem **list = get_elem_list(NeoMutt->sub->cs);
      for (size_t i = 0; list[i]; i++)
      {
        he = list[i];
        const int type = DTYPE(he->type);

        if ((type == DT_SYNONYM) || (type & DT_DEPRECATED))
          continue;

        candidate(UserTyped, he->key.strkey, Completed, sizeof(Completed));
      }
      FREE(&list);

      TAILQ_FOREACH(myv, &MyVars, entries)
      {
        candidate(UserTyped, myv->name, Completed, sizeof(Completed));
      }
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (UserTyped[0] == '\0')
        return 1;
    }

    if ((Completed[0] == 0) && UserTyped[0])
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else if (mutt_str_startswith(buf, "exec"))
  {
    const enum MenuType mtype = menu_get_current_type();
    const struct MenuFuncOp *funcs = km_get_table(mtype);
    if (!funcs && (mtype != MENU_PAGER))
      funcs = OpGeneric;

    pt++;
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_copy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));
      for (int num = 0; funcs[num].name; num++)
        candidate(UserTyped, funcs[num].name, Completed, sizeof(Completed));
      /* try the generic menu */
      if ((mtype != MENU_PAGER) && (mtype != MENU_GENERIC))
      {
        funcs = OpGeneric;
        for (int num = 0; funcs[num].name; num++)
          candidate(UserTyped, funcs[num].name, Completed, sizeof(Completed));
      }
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
      if (UserTyped[0] == '\0')
        return 1;
    }

    if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else
    return 0;

  return 1;
}

/**
 * mutt_label_complete - Complete a label name
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_label_complete(char *buf, size_t buflen, int numtabs)
{
  char *pt = buf;
  int spaces; /* keep track of the number of leading spaces on the line */

  struct Mailbox *m_cur = get_current_mailbox();
  if (!m_cur || !m_cur->label_hash)
    return 0;

  SKIPWS(buf);
  spaces = buf - pt;

  /* first TAB. Collect all the matches */
  if (numtabs == 1)
  {
    struct HashElem *entry = NULL;
    struct HashWalkState state = { 0 };

    NumMatched = 0;
    mutt_str_copy(UserTyped, buf, sizeof(UserTyped));
    memset(Matches, 0, MatchesListsize);
    memset(Completed, 0, sizeof(Completed));
    while ((entry = mutt_hash_walk(m_cur->label_hash, &state)))
      candidate(UserTyped, entry->key.strkey, Completed, sizeof(Completed));
    matches_ensure_morespace(NumMatched);
    qsort(Matches, NumMatched, sizeof(char *), (sort_t) mutt_istr_cmp);
    Matches[NumMatched++] = UserTyped;

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buf'. Fake successful return this time */
    if (UserTyped[0] == '\0')
      return 1;
  }

  if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
    return 0;

  /* NumMatched will _always_ be at least 1 since the initial
   * user-typed string is always stored */
  if ((numtabs == 1) && (NumMatched == 2))
    snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
  else if ((numtabs > 1) && (NumMatched > 2))
  {
    /* cycle through all the matches */
    snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
  }

  /* return the completed label */
  strncpy(buf, Completed, buflen - spaces);

  return 1;
}

#ifdef USE_NOTMUCH
/**
 * mutt_nm_query_complete - Complete to the nearest notmuch tag
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "tag:"-prefixed string previous to pos.
 */
bool mutt_nm_query_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  char *pt = buf;
  int spaces;

  SKIPWS(buf);
  spaces = buf - pt;

  pt = (char *) mutt_strn_rfind((char *) buf, pos, "tag:");
  if (pt)
  {
    pt += 4;
    if (numtabs == 1)
    {
      /* First TAB. Collect all the matches */
      complete_all_nm_tags(pt);

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time.  */
      if (UserTyped[0] == '\0')
        return true;
    }

    if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
      return false;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if ((numtabs == 1) && (NumMatched == 2))
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if ((numtabs > 1) && (NumMatched > 2))
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    /* return the completed query */
    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else
    return false;

  return true;
}
#endif

#ifdef USE_NOTMUCH
/**
 * mutt_nm_tag_complete - Complete to the nearest notmuch tag
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "+" or "-" -prefixed string previous to pos.
 */
bool mutt_nm_tag_complete(char *buf, size_t buflen, int numtabs)
{
  if (!buf)
    return false;

  char *pt = buf;

  /* Only examine the last token */
  char *last_space = strrchr(buf, ' ');
  if (last_space)
    pt = (last_space + 1);

  /* Skip the +/- */
  if ((pt[0] == '+') || (pt[0] == '-'))
    pt++;

  if (numtabs == 1)
  {
    /* First TAB. Collect all the matches */
    complete_all_nm_tags(pt);

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buf'. Fake successful return this time.  */
    if (UserTyped[0] == '\0')
      return true;
  }

  if ((Completed[0] == '\0') && (UserTyped[0] != '\0'))
    return false;

  /* NumMatched will _always_ be at least 1 since the initial
   * user-typed string is always stored */
  if ((numtabs == 1) && (NumMatched == 2))
    snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
  else if ((numtabs > 1) && (NumMatched > 2))
  {
    /* cycle through all the matches */
    snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
  }

  /* return the completed query */
  strncpy(pt, Completed, buf + buflen - pt);

  return true;
}
#endif

/**
 * mutt_var_value_complete - Complete a variable/value
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param pos    Cursor position in the buffer
 * @retval 1 Success
 * @retval 0 Failure
 */
int mutt_var_value_complete(char *buf, size_t buflen, int pos)
{
  char *pt = buf;

  if (buf[0] == '\0')
    return 0;

  SKIPWS(buf);
  const int spaces = buf - pt;

  pt = buf + pos - spaces;
  while ((pt > buf) && !isspace((unsigned char) *pt))
    pt--;
  pt++;           /* move past the space */
  if (*pt == '=') /* abort if no var before the '=' */
    return 0;

  if (mutt_str_startswith(buf, "set"))
  {
    const char *myvarval = NULL;
    char var[256];
    mutt_str_copy(var, pt, sizeof(var));
    /* ignore the trailing '=' when comparing */
    int vlen = mutt_str_len(var);
    if (vlen == 0)
      return 0;

    var[vlen - 1] = '\0';

    struct HashElem *he = cs_subset_lookup(NeoMutt->sub, var);
    if (!he)
    {
      myvarval = myvar_get(var);
      if (myvarval)
      {
        struct Buffer pretty = mutt_buffer_make(256);
        pretty_var(myvarval, &pretty);
        snprintf(pt, buflen - (pt - buf), "%s=%s", var, pretty.data);
        mutt_buffer_dealloc(&pretty);
        return 1;
      }
      return 0; /* no such variable. */
    }
    else
    {
      struct Buffer value = mutt_buffer_make(256);
      struct Buffer pretty = mutt_buffer_make(256);
      int rc = cs_subset_he_string_get(NeoMutt->sub, he, &value);
      if (CSR_RESULT(rc) == CSR_SUCCESS)
      {
        pretty_var(value.data, &pretty);
        snprintf(pt, buflen - (pt - buf), "%s=%s", var, pretty.data);
        mutt_buffer_dealloc(&value);
        mutt_buffer_dealloc(&pretty);
        return 0;
      }
      mutt_buffer_dealloc(&value);
      mutt_buffer_dealloc(&pretty);
      return 1;
    }
  }
  return 0;
}
