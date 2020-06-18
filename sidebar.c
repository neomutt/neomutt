/**
 * @file
 * GUI display the mailboxes in a side panel
 *
 * @authors
 * Copyright (C) 2004 Justin Hibbits <jrh29@po.cwru.edu>
 * Copyright (C) 2004 Thomer M. Gil <mutt@thomer.com>
 * Copyright (C) 2015-2020 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
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
 * @page sidebar GUI display the mailboxes in a side panel
 *
 * GUI display the mailboxes in a side panel
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "sidebar.h"
#include "context.h"
#include "format_flags.h"
#include "mutt_globals.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"

/* These Config Variables are only used in sidebar.c */
short C_SidebarComponentDepth; ///< Config: (sidebar) Strip leading path components from sidebar folders
char *C_SidebarDelimChars; ///< Config: (sidebar) Characters that separate nested folders
char *C_SidebarDividerChar; ///< Config: (sidebar) Character to draw between the sidebar and index
bool C_SidebarFolderIndent; ///< Config: (sidebar) Indent nested folders
char *C_SidebarFormat; ///< Config: (sidebar) printf-like format string for the sidebar panel
char *C_SidebarIndentString; ///< Config: (sidebar) Indent nested folders using this string
bool C_SidebarNewMailOnly; ///< Config: (sidebar) Only show folders with new/flagged mail
bool C_SidebarNonEmptyMailboxOnly; ///< Config: (sidebar) Only show folders with a non-zero number of mail
bool C_SidebarNextNewWrap; ///< Config: (sidebar) Wrap around when searching for the next mailbox with new mail
bool C_SidebarOnRight; ///< Config: (sidebar) Display the sidebar on the right
bool C_SidebarShortPath; ///< Config: (sidebar) Abbreviate the paths using the #C_Folder variable
short C_SidebarSortMethod; ///< Config: (sidebar) Method to sort the sidebar
bool C_SidebarVisible;     ///< Config: (sidebar) Show the sidebar
short C_SidebarWidth;      ///< Config: (sidebar) Width of the sidebar

/* Previous values for some sidebar config */
static short PreviousSort = SORT_ORDER; /* sidebar_sort_method */

/**
 * struct SbEntry - Info about folders in the sidebar
 */
struct SbEntry
{
  char box[256];           ///< Mailbox path (possibly abbreviated)
  int depth;               ///< Indentation depth
  struct Mailbox *mailbox; ///< Mailbox this represents
  bool is_hidden;          ///< Don't show, e.g. $sidebar_new_mail_only
};

static int EntryCount = 0;
static int EntryLen = 0;
static struct SbEntry **Entries = NULL;

static int TopIndex = -1; ///< First mailbox visible in sidebar
static int OpnIndex = -1; ///< Current (open) mailbox
static int HilIndex = -1; ///< Highlighted mailbox
static int BotIndex = -1; ///< Last mailbox visible in sidebar

/**
 * enum DivType - Source of the sidebar divider character
 */
enum DivType
{
  SB_DIV_USER,  ///< User configured using $sidebar_divider_char
  SB_DIV_ASCII, ///< An ASCII vertical bar (pipe)
  SB_DIV_UTF8,  ///< A unicode line-drawing character
};

/**
 * add_indent - Generate the needed indentation
 * @param buf    Output bufer
 * @param buflen Size of output buffer
 * @param sbe    Sidebar entry
 * @retval Number of bytes written
 */
static size_t add_indent(char *buf, size_t buflen, const struct SbEntry *sbe)
{
  size_t res = 0;
  for (int i = 0; i < sbe->depth; i++)
  {
    res += mutt_str_copy(buf + res, C_SidebarIndentString, buflen - res);
  }
  return res;
}

/**
 * sidebar_format_str - Format a string for the sidebar - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%!     | 'n!' Flagged messages
 * | \%B     | Name of the mailbox
 * | \%D     | Description of the mailbox
 * | \%d     | Number of deleted messages
 * | \%F     | Number of Flagged messages in the mailbox
 * | \%L     | Number of messages after limiting
 * | \%n     | 'N' if mailbox has new mail, ' ' (space) otherwise
 * | \%N     | Number of unread messages in the mailbox
 * | \%o     | Number of old unread messages in the mailbox
 * | \%r     | Number of read messages in the mailbox
 * | \%S     | Size of mailbox (total number of messages)
 * | \%t     | Number of tagged messages
 * | \%Z     | Number of new unseen messages in the mailbox
 */
static const char *sidebar_format_str(char *buf, size_t buflen, size_t col, int cols,
                                      char op, const char *src, const char *prec,
                                      const char *if_str, const char *else_str,
                                      intptr_t data, MuttFormatFlags flags)
{
  struct SbEntry *sbe = (struct SbEntry *) data;
  char fmt[256];

  if (!sbe || !buf)
    return src;

  buf[0] = '\0'; /* Just in case there's nothing to do */

  struct Mailbox *m = sbe->mailbox;
  if (!m)
    return src;

  bool c = Context && Context->mailbox &&
           mutt_str_equal(Context->mailbox->realpath, m->realpath);

  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'B':
    case 'D':
    {
      char indented[256];
      size_t off = add_indent(indented, sizeof(indented), sbe);
      snprintf(indented + off, sizeof(indented) - off, "%s",
               ((op == 'D') && sbe->mailbox->name) ? sbe->mailbox->name : sbe->box);

      mutt_format_s(buf, buflen, prec, indented);
      break;
    }

    case 'd':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, c ? Context->mailbox->msg_deleted : 0);
      }
      else if ((c && (Context->mailbox->msg_deleted == 0)) || !c)
        optional = false;
      break;

    case 'F':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, m->msg_flagged);
      }
      else if (m->msg_flagged == 0)
        optional = false;
      break;

    case 'L':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, c ? Context->mailbox->vcount : m->msg_count);
      }
      else if ((c && (Context->mailbox->vcount == m->msg_count)) || !c)
        optional = false;
      break;

    case 'N':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, m->msg_unread);
      }
      else if (m->msg_unread == 0)
        optional = false;
      break;

    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt, m->has_new ? 'N' : ' ');
      }
      else if (m->has_new == false)
        optional = false;
      break;

    case 'o':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, m->msg_unread - m->msg_new);
      }
      else if ((c && (Context->mailbox->msg_unread - Context->mailbox->msg_new) == 0) || !c)
        optional = false;
      break;

    case 'r':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, m->msg_count - m->msg_unread);
      }
      else if ((c && (Context->mailbox->msg_count - Context->mailbox->msg_unread) == 0) || !c)
        optional = false;
      break;

    case 'S':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, m->msg_count);
      }
      else if (m->msg_count == 0)
        optional = false;
      break;

    case 't':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, c ? Context->mailbox->msg_tagged : 0);
      }
      else if ((c && (Context->mailbox->msg_tagged == 0)) || !c)
        optional = false;
      break;

    case 'Z':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, m->msg_new);
      }
      else if ((c && (Context->mailbox->msg_new) == 0) || !c)
        optional = false;
      break;

    case '!':
      if (m->msg_flagged == 0)
        mutt_format_s(buf, buflen, prec, "");
      else if (m->msg_flagged == 1)
        mutt_format_s(buf, buflen, prec, "!");
      else if (m->msg_flagged == 2)
        mutt_format_s(buf, buflen, prec, "!!");
      else
      {
        snprintf(fmt, sizeof(fmt), "%d!", m->msg_flagged);
        mutt_format_s(buf, buflen, prec, fmt);
      }
      break;
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, C_SidebarWidth, if_str,
                        sidebar_format_str, IP sbe, flags);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, C_SidebarWidth, else_str,
                        sidebar_format_str, IP sbe, flags);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * make_sidebar_entry - Turn mailbox data into a sidebar string
 * @param[out] buf     Buffer in which to save string
 * @param[in]  buflen  Buffer length
 * @param[in]  width   Desired width in screen cells
 * @param[in]  sbe     Mailbox object
 *
 * Take all the relevant mailbox data and the desired screen width and then get
 * mutt_expando_format to do the actual work. mutt_expando_format will callback to
 * us using sidebar_format_str() for the sidebar specific formatting characters.
 */
static void make_sidebar_entry(char *buf, size_t buflen, int width, struct SbEntry *sbe)
{
  mutt_expando_format(buf, buflen, 0, width, NONULL(C_SidebarFormat),
                      sidebar_format_str, IP sbe, MUTT_FORMAT_NO_FLAGS);

  /* Force string to be exactly the right width */
  int w = mutt_strwidth(buf);
  int s = mutt_str_len(buf);
  width = MIN(buflen, width);
  if (w < width)
  {
    /* Pad with spaces */
    memset(buf + s, ' ', width - w);
    buf[s + width - w] = '\0';
  }
  else if (w > width)
  {
    /* Truncate to fit */
    size_t len = mutt_wstr_trunc(buf, buflen, width, NULL);
    buf[len] = '\0';
  }
}

/**
 * cb_qsort_sbe - qsort callback to sort SbEntry's
 * @param a First  SbEntry to compare
 * @param b Second SbEntry to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int cb_qsort_sbe(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;
  const struct Mailbox *m1 = sbe1->mailbox;
  const struct Mailbox *m2 = sbe2->mailbox;

  int rc = 0;

  switch ((C_SidebarSortMethod & SORT_MASK))
  {
    case SORT_COUNT:
      if (m2->msg_count == m1->msg_count)
        rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));
      else
        rc = (m2->msg_count - m1->msg_count);
      break;
    case SORT_UNREAD:
      if (m2->msg_unread == m1->msg_unread)
        rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));
      else
        rc = (m2->msg_unread - m1->msg_unread);
      break;
    case SORT_DESC:
      rc = mutt_str_cmp(m1->name, m2->name);
      break;
    case SORT_FLAGGED:
      if (m2->msg_flagged == m1->msg_flagged)
        rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));
      else
        rc = (m2->msg_flagged - m1->msg_flagged);
      break;
    case SORT_PATH:
    {
      rc = mutt_inbox_cmp(mailbox_path(m1), mailbox_path(m2));
      if (rc == 0)
        rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));
      break;
    }
  }

  if (C_SidebarSortMethod & SORT_REVERSE)
    rc = -rc;

  return rc;
}

/**
 * update_entries_visibility - Should a SbEntry be displayed in the sidebar?
 *
 * For each SbEntry in the Entries array, check whether we should display it.
 * This is determined by several criteria.  If the Mailbox:
 * * is the currently open mailbox
 * * is the currently highlighted mailbox
 * * has unread messages
 * * has flagged messages
 * * is whitelisted
 */
static void update_entries_visibility(void)
{
  /* Aliases for readability */
  const bool new_only = C_SidebarNewMailOnly;
  const bool non_empty_only = C_SidebarNonEmptyMailboxOnly;
  struct SbEntry *sbe = NULL;

  /* Take the fast path if there is no need to test visibilities */
  if (!new_only && !non_empty_only)
  {
    for (int i = 0; i < EntryCount; i++)
    {
      Entries[i]->is_hidden = false;
    }
    return;
  }

  for (int i = 0; i < EntryCount; i++)
  {
    sbe = Entries[i];

    sbe->is_hidden = false;

    if (Context && mutt_str_equal(sbe->mailbox->realpath, Context->mailbox->realpath))
    {
      /* Spool directories are always visible */
      continue;
    }

    if (mutt_list_find(&SidebarWhitelist, mailbox_path(sbe->mailbox)) ||
        mutt_list_find(&SidebarWhitelist, sbe->mailbox->name))
    {
      /* Explicitly asked to be visible */
      continue;
    }

    if (non_empty_only && (i != OpnIndex) && (sbe->mailbox->msg_count == 0))
    {
      sbe->is_hidden = true;
    }

    if (new_only && (i != OpnIndex) && (sbe->mailbox->msg_unread == 0) &&
        (sbe->mailbox->msg_flagged == 0) && !sbe->mailbox->has_new)
    {
      sbe->is_hidden = true;
    }
  }
}

/**
 * unsort_entries - Restore Entries array order to match Mailbox list order
 */
static void unsort_entries(void)
{
  int i = 0;

  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if (i >= EntryCount)
      break;

    int j = i;
    while ((j < EntryCount) && (Entries[j]->mailbox != np->mailbox))
      j++;
    if (j < EntryCount)
    {
      if (j != i)
      {
        struct SbEntry *tmp = Entries[i];
        Entries[i] = Entries[j];
        Entries[j] = tmp;
      }
      i++;
    }
  }
  neomutt_mailboxlist_clear(&ml);
}

/**
 * sort_entries - Sort Entries array
 *
 * Sort the Entries array according to the current sort config
 * option "sidebar_sort_method". This calls qsort to do the work which calls our
 * callback function "cb_qsort_sbe".
 *
 * Once sorted, the prev/next links will be reconstructed.
 */
static void sort_entries(void)
{
  enum SortType ssm = (C_SidebarSortMethod & SORT_MASK);

  /* These are the only sort methods we understand */
  if ((ssm == SORT_COUNT) || (ssm == SORT_UNREAD) || (ssm == SORT_FLAGGED) || (ssm == SORT_PATH))
    qsort(Entries, EntryCount, sizeof(*Entries), cb_qsort_sbe);
  else if ((ssm == SORT_ORDER) && (C_SidebarSortMethod != PreviousSort))
    unsort_entries();
}

/**
 * select_next - Selects the next unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_next(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry++;
    if (entry == EntryCount)
      return false;
  } while (Entries[entry]->is_hidden);

  HilIndex = entry;
  return true;
}

/**
 * select_next_new - Selects the next new mailbox
 * @retval true  Success
 * @retval false Failure
 *
 * Search down the list of mail folders for one containing new mail.
 */
static bool select_next_new(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry++;
    if (entry == EntryCount)
    {
      if (C_SidebarNextNewWrap)
        entry = 0;
      else
        return false;
    }
    if (entry == HilIndex)
      return false;
  } while (!Entries[entry]->mailbox->has_new && (Entries[entry]->mailbox->msg_unread == 0));

  HilIndex = entry;
  return true;
}

/**
 * select_prev - Selects the previous unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_prev(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry--;
    if (entry < 0)
      return false;
  } while (Entries[entry]->is_hidden);

  HilIndex = entry;
  return true;
}

/**
 * select_prev_new - Selects the previous new mailbox
 * @retval true  Success
 * @retval false Failure
 *
 * Search up the list of mail folders for one containing new mail.
 */
static bool select_prev_new(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry--;
    if (entry < 0)
    {
      if (C_SidebarNextNewWrap)
        entry = EntryCount - 1;
      else
        return false;
    }
    if (entry == HilIndex)
      return false;
  } while (!Entries[entry]->mailbox->has_new && (Entries[entry]->mailbox->msg_unread == 0));

  HilIndex = entry;
  return true;
}

/**
 * select_page_down - Selects the first entry in the next page of mailboxes
 * @retval true  Success
 * @retval false Failure
 */
static bool select_page_down(void)
{
  if ((EntryCount == 0) || (BotIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = BotIndex;
  select_next();
  /* If the rest of the entries are hidden, go up to the last unhidden one */
  if (Entries[HilIndex]->is_hidden)
    select_prev();

  return (orig_hil_index != HilIndex);
}

/**
 * select_page_up - Selects the last entry in the previous page of mailboxes
 * @retval true  Success
 * @retval false Failure
 */
static bool select_page_up(void)
{
  if ((EntryCount == 0) || (TopIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = TopIndex;
  select_prev();
  /* If the rest of the entries are hidden, go down to the last unhidden one */
  if (Entries[HilIndex]->is_hidden)
    select_next();

  return (orig_hil_index != HilIndex);
}

/**
 * select_first - Selects the first unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_first(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = 0;
  if (Entries[HilIndex]->is_hidden)
    if (!select_next())
      HilIndex = orig_hil_index;

  return (orig_hil_index != HilIndex);
}

/**
 * select_last - Selects the last unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_last(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = EntryCount;
  if (!select_prev())
    HilIndex = orig_hil_index;

  return (orig_hil_index != HilIndex);
}

/**
 * prepare_sidebar - Prepare the list of SbEntry's for the sidebar display
 * @param page_size  The number of lines on a page
 * @retval false No, don't draw the sidebar
 * @retval true  Yes, draw the sidebar
 *
 * Before painting the sidebar, we determine which are visible, sort
 * them and set up our page pointers.
 *
 * This is a lot of work to do each refresh, but there are many things that
 * can change outside of the sidebar that we don't hear about.
 */
static bool prepare_sidebar(int page_size)
{
  if ((EntryCount == 0) || (page_size <= 0))
    return false;

  const struct SbEntry *opn_entry = (OpnIndex >= 0) ? Entries[OpnIndex] : NULL;
  const struct SbEntry *hil_entry = (HilIndex >= 0) ? Entries[HilIndex] : NULL;

  update_entries_visibility();
  sort_entries();

  for (int i = 0; i < EntryCount; i++)
  {
    if (opn_entry == Entries[i])
      OpnIndex = i;
    if (hil_entry == Entries[i])
      HilIndex = i;
  }

  if ((HilIndex < 0) || Entries[HilIndex]->is_hidden || (C_SidebarSortMethod != PreviousSort))
  {
    if (OpnIndex >= 0)
      HilIndex = OpnIndex;
    else
    {
      HilIndex = 0;
      if (Entries[HilIndex]->is_hidden)
        select_next();
    }
  }

  /* Set the Top and Bottom to frame the HilIndex in groups of page_size */

  /* If C_SidebarNewMailOnly or C_SidebarNonEmptyMailboxOnly is set, some entries
   * may be hidden so we need to scan for the framing interval */
  if (C_SidebarNewMailOnly || C_SidebarNonEmptyMailboxOnly)
  {
    TopIndex = -1;
    BotIndex = -1;
    while (BotIndex < HilIndex)
    {
      TopIndex = BotIndex + 1;
      int page_entries = 0;
      while (page_entries < page_size)
      {
        BotIndex++;
        if (BotIndex >= EntryCount)
          break;
        if (!Entries[BotIndex]->is_hidden)
          page_entries++;
      }
    }
  }
  /* Otherwise we can just calculate the interval */
  else
  {
    TopIndex = (HilIndex / page_size) * page_size;
    BotIndex = TopIndex + page_size - 1;
  }

  if (BotIndex > (EntryCount - 1))
    BotIndex = EntryCount - 1;

  PreviousSort = C_SidebarSortMethod;
  return true;
}

/**
 * draw_divider - Draw a line between the sidebar and the rest of neomutt
 * @param win        Window to draw on
 * @param num_rows   Height of the Sidebar
 * @param num_cols   Width of the Sidebar
 * @retval 0   Empty string
 * @retval num Character occupies n screen columns
 *
 * Draw a divider using characters from the config option "sidebar_divider_char".
 * This can be an ASCII or Unicode character.
 * We calculate these characters' width in screen columns.
 *
 * If the user hasn't set $sidebar_divider_char we pick a character for them,
 * respecting the value of $ascii_chars.
 */
static int draw_divider(struct MuttWindow *win, int num_rows, int num_cols)
{
  if ((num_rows < 1) || (num_cols < 1))
    return 0;

  int delim_len;
  enum DivType altchar = SB_DIV_UTF8;

  /* Calculate the width of the delimiter in screen cells */
  delim_len = mutt_strwidth(C_SidebarDividerChar);
  if (delim_len < 0)
  {
    delim_len = 1; /* Bad character */
  }
  else if (delim_len == 0)
  {
    if (C_SidebarDividerChar)
      return 0; /* User has set empty string */

    delim_len = 1; /* Unset variable */
  }
  else
  {
    altchar = SB_DIV_USER; /* User config */
  }

  if (C_AsciiChars && (altchar != SB_DIV_ASCII))
  {
    /* $ascii_chars overrides Unicode divider chars */
    if (altchar == SB_DIV_UTF8)
    {
      altchar = SB_DIV_ASCII;
    }
    else if (C_SidebarDividerChar)
    {
      for (int i = 0; i < delim_len; i++)
      {
        if (C_SidebarDividerChar[i] & ~0x7F) /* high-bit is set */
        {
          altchar = SB_DIV_ASCII;
          delim_len = 1;
          break;
        }
      }
    }
  }

  if (delim_len > num_cols)
    return 0;

  mutt_curses_set_color(MT_COLOR_SIDEBAR_DIVIDER);

  int col = C_SidebarOnRight ? 0 : (C_SidebarWidth - delim_len);

  for (int i = 0; i < num_rows; i++)
  {
    mutt_window_move(win, col, i);

    switch (altchar)
    {
      case SB_DIV_USER:
        mutt_window_addstr(NONULL(C_SidebarDividerChar));
        break;
      case SB_DIV_ASCII:
        mutt_window_addch('|');
        break;
      case SB_DIV_UTF8:
        mutt_window_addch(ACS_VLINE);
        break;
    }
  }

  return delim_len;
}

/**
 * fill_empty_space - Wipe the remaining Sidebar space
 * @param win        Window to draw on
 * @param first_row  Window line to start (0-based)
 * @param num_rows   Number of rows to fill
 * @param div_width  Width in screen characters taken by the divider
 * @param num_cols   Number of columns to fill
 *
 * Write spaces over the area the sidebar isn't using.
 */
static void fill_empty_space(struct MuttWindow *win, int first_row,
                             int num_rows, int div_width, int num_cols)
{
  /* Fill the remaining rows with blank space */
  mutt_curses_set_color(MT_COLOR_NORMAL);

  if (!C_SidebarOnRight)
    div_width = 0;
  for (int r = 0; r < num_rows; r++)
  {
    mutt_window_move(win, div_width, first_row + r);

    for (int i = 0; i < num_cols; i++)
      mutt_window_addch(' ');
  }
}

/**
 * imap_is_prefix - Check if folder matches the beginning of mbox
 * @param folder Folder
 * @param mbox   Mailbox path
 * @retval num Length of the prefix
 */
static int imap_is_prefix(const char *folder, const char *mbox)
{
  int plen = 0;

  struct Url *url_m = url_parse(mbox);
  struct Url *url_f = url_parse(folder);
  if (!url_m || !url_f)
    goto done;

  if (!mutt_istr_equal(url_m->host, url_f->host))
    goto done;

  if (url_m->user && url_f->user && !mutt_istr_equal(url_m->user, url_f->user))
    goto done;

  size_t mlen = mutt_str_len(url_m->path);
  size_t flen = mutt_str_len(url_f->path);
  if (flen > mlen)
    goto done;

  if (!mutt_strn_equal(url_m->path, url_f->path, flen))
    goto done;

  plen = strlen(mbox) - mlen + flen;

done:
  url_free(&url_m);
  url_free(&url_f);

  return plen;
}

/**
 * abbrev_folder - Abbreviate a Mailbox path using a folder
 * @param mbox   Mailbox path to shorten
 * @param folder Folder path to use
 * @param type   Mailbox type
 * @retval ptr Pointer into the mbox param
 */
static const char *abbrev_folder(const char *mbox, const char *folder, enum MailboxType type)
{
  if (!mbox || !folder)
    return NULL;

  if (type == MUTT_IMAP)
  {
    int prefix = imap_is_prefix(folder, mbox);
    if (prefix == 0)
      return NULL;
    return mbox + prefix;
  }

  if (!C_SidebarDelimChars)
    return NULL;

  size_t flen = mutt_str_len(folder);
  if (flen == 0)
    return NULL;
  if (strchr(C_SidebarDelimChars, folder[flen - 1])) // folder ends with a delimiter
    flen--;

  size_t mlen = mutt_str_len(mbox);
  if (mlen <= flen)
    return NULL;

  if (!mutt_strn_equal(folder, mbox, flen))
    return NULL;

  // After the match, check that mbox has a delimiter
  if (!strchr(C_SidebarDelimChars, mbox[flen]))
    return NULL;

  return mbox + flen + 1;
}

/**
 * abbrev_url - Abbreviate a url-style Mailbox path
 * @param mbox Mailbox path to shorten
 * @param type Mailbox type
 *
 * Use heuristics to shorten a non-local Mailbox path.
 * Strip the host part (or database part for Notmuch).
 *
 * e.g.
 * - `imap://user@host.com/apple/banana` becomes `apple/banana`
 * - `notmuch:///home/user/db?query=hello` becomes `query=hello`
 */
static const char *abbrev_url(const char *mbox, enum MailboxType type)
{
  /* This is large enough to skip `notmuch://`,
   * but not so large that it will go past the host part. */
  const int scheme_len = 10;

  size_t len = mutt_str_len(mbox);
  if ((len < scheme_len) || ((type != MUTT_NNTP) && (type != MUTT_IMAP) &&
                             (type != MUTT_NOTMUCH) && (type != MUTT_POP)))
  {
    return mbox;
  }

  const char split = (type == MUTT_NOTMUCH) ? '?' : '/';

  // Skip over the scheme, e.g. `imaps://`, `notmuch://`
  const char *last = strchr(mbox + scheme_len, split);
  if (last)
    mbox = last + 1;
  return mbox;
}

/**
 * calc_path_depth - Calculate the depth of a Mailbox path
 * @param[in]  mbox      Mailbox path to examine
 * @param[in]  delims    Delimiter characters
 * @param[out] last_part Last path component
 */
static int calc_path_depth(const char *mbox, const char *delims, const char **last_part)
{
  if (!mbox || !delims || !last_part)
    return 0;

  int depth = 0;
  const char *match = NULL;
  while ((match = strpbrk(mbox, delims)))
  {
    depth++;
    mbox = match + 1;
  }

  *last_part = mbox;
  return depth;
}

/**
 * draw_sidebar - Write out a list of mailboxes, in a panel
 * @param win        Window to draw on
 * @param num_rows   Height of the Sidebar
 * @param num_cols   Width of the Sidebar
 * @param div_width  Width in screen characters taken by the divider
 *
 * Display a list of mailboxes in a panel on the left.  What's displayed will
 * depend on our index markers: TopMailbox, OpnMailbox, HilMailbox, BotMailbox.
 * On the first run they'll be NULL, so we display the top of NeoMutt's list.
 *
 * * TopMailbox - first visible mailbox
 * * BotMailbox - last  visible mailbox
 * * OpnMailbox - mailbox shown in NeoMutt's Index Panel
 * * HilMailbox - Unselected mailbox (the paging follows this)
 *
 * The entries are formatted using "sidebar_format" and may be abbreviated:
 * "sidebar_short_path", indented: "sidebar_folder_indent",
 * "sidebar_indent_string" and sorted: "sidebar_sort_method".  Finally, they're
 * trimmed to fit the available space.
 */
static void draw_sidebar(struct MuttWindow *win, int num_rows, int num_cols, int div_width)
{
  struct SbEntry *entry = NULL;
  struct Mailbox *m = NULL;
  if (TopIndex < 0)
    return;

  int w = MIN(num_cols, (C_SidebarWidth - div_width));
  int row = 0;
  for (int entryidx = TopIndex; (entryidx < EntryCount) && (row < num_rows); entryidx++)
  {
    entry = Entries[entryidx];
    if (entry->is_hidden)
      continue;
    m = entry->mailbox;

    if (entryidx == OpnIndex)
    {
      if ((Colors->defs[MT_COLOR_SIDEBAR_INDICATOR] != 0))
        mutt_curses_set_color(MT_COLOR_SIDEBAR_INDICATOR);
      else
        mutt_curses_set_color(MT_COLOR_INDICATOR);
    }
    else if (entryidx == HilIndex)
      mutt_curses_set_color(MT_COLOR_SIDEBAR_HIGHLIGHT);
    else if (m->has_new)
      mutt_curses_set_color(MT_COLOR_SIDEBAR_NEW);
    else if (m->msg_unread > 0)
      mutt_curses_set_color(MT_COLOR_SIDEBAR_UNREAD);
    else if (m->msg_flagged > 0)
      mutt_curses_set_color(MT_COLOR_SIDEBAR_FLAGGED);
    else if ((Colors->defs[MT_COLOR_SIDEBAR_SPOOLFILE] != 0) &&
             mutt_str_equal(mailbox_path(m), C_Spoolfile))
    {
      mutt_curses_set_color(MT_COLOR_SIDEBAR_SPOOLFILE);
    }
    else
    {
      if (Colors->defs[MT_COLOR_SIDEBAR_ORDINARY] != 0)
        mutt_curses_set_color(MT_COLOR_SIDEBAR_ORDINARY);
      else
        mutt_curses_set_color(MT_COLOR_NORMAL);
    }

    int col = 0;
    if (C_SidebarOnRight)
      col = div_width;

    mutt_window_move(win, col, row);
    if (Context && Context->mailbox && (Context->mailbox->realpath[0] != '\0') &&
        mutt_str_equal(m->realpath, Context->mailbox->realpath))
    {
      m->msg_unread = Context->mailbox->msg_unread;
      m->msg_count = Context->mailbox->msg_count;
      m->msg_flagged = Context->mailbox->msg_flagged;
    }

    const char *path = mailbox_path(m);

    // Try to abbreviate the full path
    const char *abbr = abbrev_folder(path, C_Folder, m->type);
    if (!abbr)
      abbr = abbrev_url(path, m->type);
    const char *short_path = abbr ? abbr : path;

    /* Compute the depth */
    const char *last_part = abbr;
    entry->depth = calc_path_depth(abbr, C_SidebarDelimChars, &last_part);

    const bool short_path_is_abbr = (short_path == abbr);
    if (C_SidebarShortPath)
    {
      short_path = last_part;
    }

    // Don't indent if we were unable to create an abbreviation.
    // Otherwise, the full path will be indent, and it looks unusual.
    if (C_SidebarFolderIndent && short_path_is_abbr)
    {
      if (C_SidebarComponentDepth > 0)
        entry->depth -= C_SidebarComponentDepth;
    }

    mutt_str_copy(entry->box, short_path, sizeof(entry->box));
    char str[256];
    make_sidebar_entry(str, sizeof(str), w, entry);
    mutt_window_printf("%s", str);
    row++;
  }

  fill_empty_space(win, row, num_rows - row, div_width, w);
}

/**
 * sb_draw - Completely redraw the sidebar
 * @param win Window to draw on
 *
 * Completely refresh the sidebar region.  First draw the divider; then, for
 * each Mailbox, call make_sidebar_entry; finally blank out any remaining space.
 */
void sb_draw(struct MuttWindow *win)
{
  if (!C_SidebarVisible || !win)
    return;

  if (!mutt_window_is_visible(win))
    return;

  int col = 0, row = 0;
  mutt_window_get_coords(win, &col, &row);

  int num_rows = win->state.rows;
  int num_cols = win->state.cols;

  int div_width = draw_divider(win, num_rows, num_cols);

  if (!Entries)
  {
    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
    struct MailboxNode *np = NULL;
    STAILQ_FOREACH(np, &ml, entries)
    {
      sb_notify_mailbox(np->mailbox, SBN_CREATED);
    }
    neomutt_mailboxlist_clear(&ml);
  }

  if (!prepare_sidebar(num_rows))
  {
    fill_empty_space(win, 0, num_rows, div_width, num_cols - div_width);
    return;
  }

  draw_sidebar(win, num_rows, num_cols, div_width);
  mutt_window_move(win, col, row);
}

/**
 * sb_change_mailbox - Change the selected mailbox
 * @param op Operation code
 *
 * Change the selected mailbox, e.g. "Next mailbox", "Previous Mailbox
 * with new mail". The operations are listed in opcodes.h.
 *
 * If the operation is successful, HilMailbox will be set to the new mailbox.
 * This function only *selects* the mailbox, doesn't *open* it.
 *
 * Allowed values are: OP_SIDEBAR_FIRST, OP_SIDEBAR_LAST,
 * OP_SIDEBAR_NEXT, OP_SIDEBAR_NEXT_NEW,
 * OP_SIDEBAR_PAGE_DOWN, OP_SIDEBAR_PAGE_UP, OP_SIDEBAR_PREV,
 * OP_SIDEBAR_PREV_NEW.
 */
void sb_change_mailbox(int op)
{
  if (!C_SidebarVisible)
    return;

  if (HilIndex < 0) /* It'll get reset on the next draw */
    return;

  switch (op)
  {
    case OP_SIDEBAR_FIRST:
      if (!select_first())
        return;
      break;
    case OP_SIDEBAR_LAST:
      if (!select_last())
        return;
      break;
    case OP_SIDEBAR_NEXT:
      if (!select_next())
        return;
      break;
    case OP_SIDEBAR_NEXT_NEW:
      if (!select_next_new())
        return;
      break;
    case OP_SIDEBAR_PAGE_DOWN:
      if (!select_page_down())
        return;
      break;
    case OP_SIDEBAR_PAGE_UP:
      if (!select_page_up())
        return;
      break;
    case OP_SIDEBAR_PREV:
      if (!select_prev())
        return;
      break;
    case OP_SIDEBAR_PREV_NEW:
      if (!select_prev_new())
        return;
      break;
    default:
      return;
  }
  mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
}

/**
 * sb_get_highlight - Get the Mailbox that's highlighted in the sidebar
 * @retval ptr Mailbox
 */
struct Mailbox *sb_get_highlight(void)
{
  if (!C_SidebarVisible)
    return NULL;

  if ((EntryCount == 0) || (HilIndex < 0))
    return NULL;

  return Entries[HilIndex]->mailbox;
}

/**
 * sb_set_open_mailbox - Set the 'open' Mailbox
 * @param m Mailbox
 *
 * Search through the list of mailboxes.
 * If a Mailbox has a matching path, set OpnMailbox to it.
 */
void sb_set_open_mailbox(struct Mailbox *m)
{
  OpnIndex = -1;

  if (!m)
    return;

  for (int entry = 0; entry < EntryCount; entry++)
  {
    if (mutt_str_equal(Entries[entry]->mailbox->realpath, m->realpath))
    {
      OpnIndex = entry;
      HilIndex = entry;
      break;
    }
  }
}

/**
 * sb_notify_mailbox - The state of a Mailbox is about to change
 * @param m   Folder
 * @param sbn What happened to the mailbox
 *
 * We receive a notification:
 * - After a new Mailbox has been created
 * - Before a Mailbox is deleted
 * - After an existing Mailbox is renamed
 *
 * Before a deletion, check that our pointers won't be invalidated.
 */
void sb_notify_mailbox(struct Mailbox *m, enum SidebarNotification sbn)
{
  if (!m)
    return;

  /* Any new/deleted mailboxes will cause a refresh.  As long as
   * they're valid, our pointers will be updated in prepare_sidebar() */

  if (sbn == SBN_CREATED)
  {
    if (EntryCount >= EntryLen)
    {
      EntryLen += 10;
      mutt_mem_realloc(&Entries, EntryLen * sizeof(struct SbEntry *));
    }
    Entries[EntryCount] = mutt_mem_calloc(1, sizeof(struct SbEntry));
    Entries[EntryCount]->mailbox = m;

    if (TopIndex < 0)
      TopIndex = EntryCount;
    if (BotIndex < 0)
      BotIndex = EntryCount;
    if ((OpnIndex < 0) && Context &&
        mutt_str_equal(m->realpath, Context->mailbox->realpath))
    {
      OpnIndex = EntryCount;
    }

    EntryCount++;
  }
  else if (sbn == SBN_DELETED)
  {
    int del_index;
    for (del_index = 0; del_index < EntryCount; del_index++)
      if (Entries[del_index]->mailbox == m)
        break;
    if (del_index == EntryCount)
      return;
    FREE(&Entries[del_index]);
    EntryCount--;

    if ((TopIndex > del_index) || (TopIndex == EntryCount))
      TopIndex--;
    if (OpnIndex == del_index)
      OpnIndex = -1;
    else if (OpnIndex > del_index)
      OpnIndex--;
    if ((HilIndex > del_index) || (HilIndex == EntryCount))
      HilIndex--;
    if ((BotIndex > del_index) || (BotIndex == EntryCount))
      BotIndex--;

    for (; del_index < EntryCount; del_index++)
      Entries[del_index] = Entries[del_index + 1];
  }

  // otherwise, we just need to redraw

  mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
}

/**
 * sb_observer - Listen for config changes affecting the sidebar - Implements ::observer_t
 * @param nc Notification data
 * @retval bool True, if successful
 */
int sb_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct MuttWindow *win = nc->global_data;
  struct EventConfig *ec = nc->event_data;

  if (!mutt_strn_equal(ec->name, "sidebar_", 8))
    return 0;

  bool repaint = false;

  if (win->state.visible != C_SidebarVisible)
  {
    window_set_visible(win, C_SidebarVisible);
    repaint = true;
  }

  if (win->req_cols != C_SidebarWidth)
  {
    win->req_cols = C_SidebarWidth;
    repaint = true;
  }

  struct MuttWindow *parent = win->parent;
  struct MuttWindow *first = TAILQ_FIRST(&parent->children);

  if ((C_SidebarOnRight && (first == win)) || (!C_SidebarOnRight && (first != win)))
  {
    // Swap the Sidebar and the Container of the Index/Pager
    TAILQ_REMOVE(&parent->children, first, entries);
    TAILQ_INSERT_TAIL(&parent->children, first, entries);
    repaint = true;
  }

  if (repaint)
  {
    mutt_debug(LL_NOTIFY, "repaint sidebar\n");
    mutt_window_reflow(MuttDialogWindow);
    mutt_menu_set_current_redraw_full();
  }

  return 0;
}
