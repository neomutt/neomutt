/**
 * Copyright (C) 2004 Justin Hibbits <jrh29@po.cwru.edu>
 * Copyright (C) 2004 Thomer M. Gil <mutt@thomer.com>
 * Copyright (C) 2015-2016 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016 Kevin J. McCarthy <kevin@8t8.us>
 *
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

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt.h"
#include "sidebar.h"
#include "buffy.h"
#include "context.h"
#include "format_flags.h"
#include "globals.h"
#include "keymap_defs.h"
#include "lib.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

/* Previous values for some sidebar config */
static short PreviousSort = SORT_ORDER; /* sidebar_sort_method */

/**
 * struct SbEntry - Info about folders in the sidebar
 */
struct SbEntry
{
  char box[STRING]; /* formatted mailbox name */
  struct Buffy *buffy;
  short is_hidden;
};

static int EntryCount = 0;
static int EntryLen = 0;
static struct SbEntry **Entries = NULL;

static int TopIndex = -1; /* First mailbox visible in sidebar */
static int OpnIndex = -1; /* Current (open) mailbox */
static int HilIndex = -1; /* Highlighted mailbox */
static int BotIndex = -1; /* Last mailbox visible in sidebar */

/* The source of the sidebar divider character. */
enum div_type
{
  SB_DIV_USER,
  SB_DIV_ASCII,
  SB_DIV_UTF8
};

enum
{
  SB_SRC_NONE = 0,
  SB_SRC_VIRT,
  SB_SRC_INCOMING
};
static int sidebar_source = SB_SRC_NONE;

static struct Buffy *get_incoming(void)
{
  switch (sidebar_source)
  {
    case SB_SRC_NONE:
      sidebar_source = SB_SRC_INCOMING;

#ifdef USE_NOTMUCH
      if (option(OPTVIRTSPOOLFILE) && VirtIncoming)
      {
        sidebar_source = SB_SRC_VIRT;
        return VirtIncoming;
      }
      break;
    case SB_SRC_VIRT:
      if (VirtIncoming)
      {
        return VirtIncoming;
      }
      break;
#endif
    case SB_SRC_INCOMING:
      break;
  }

  return Incoming; /* default */
}

/**
 * cb_format_str - Create the string to show in the sidebar
 * @dest:        Buffer in which to save string
 * @destlen:     Buffer length
 * @col:         Starting column, UNUSED
 * @op:          printf-like operator, e.g. 'B'
 * @src:         printf-like format string
 * @prefix:      Field formatting string, UNUSED
 * @ifstring:    If condition is met, display this string
 * @elsestring:  Otherwise, display this string
 * @data:        Pointer to our sidebar_entry
 * @flags:       Format flags, e.g. MUTT_FORMAT_OPTIONAL
 *
 * cb_format_str is a callback function for mutt_FormatString.  It understands
 * six operators. '%B' : Mailbox name, '%F' : Number of flagged messages,
 * '%N' : Number of new messages, '%S' : Size (total number of messages),
 * '%!' : Icon denoting number of flagged messages.
 * '%n' : N if folder has new mail, blank otherwise.
 *
 * Returns: src (unchanged)
 */
static const char *cb_format_str(char *dest, size_t destlen, size_t col, int cols,
                                 char op, const char *src, const char *prefix,
                                 const char *ifstring, const char *elsestring,
                                 unsigned long data, format_flag flags)
{
  struct SbEntry *sbe = (struct SbEntry *) data;
  unsigned int optional;
  char fmt[STRING];

  if (!sbe || !dest)
    return src;

  dest[0] = 0; /* Just in case there's nothing to do */

  struct Buffy *b = sbe->buffy;
  if (!b)
    return src;

  int c = Context && (mutt_strcmp(Context->realpath, b->realpath) == 0);

  optional = flags & MUTT_FORMAT_OPTIONAL;

  switch (op)
  {
    case 'B':
      mutt_format_s(dest, destlen, prefix, sbe->box);
      break;

    case 'd':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, c ? Context->deleted : 0);
      }
      else if ((c && Context->deleted == 0) || !c)
        optional = 0;
      break;

    case 'F':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, b->msg_flagged);
      }
      else if (b->msg_flagged == 0)
        optional = 0;
      break;

    case 'L':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, c ? Context->vcount : b->msg_count);
      }
      else if ((c && Context->vcount == b->msg_count) || !c)
        optional = 0;
      break;

    case 'N':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, b->msg_unread);
      }
      else if (b->msg_unread == 0)
        optional = 0;
      break;

    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prefix);
        snprintf(dest, destlen, fmt, b->new ? 'N' : ' ');
      }
      else if (b->new == false)
        optional = 0;
      break;

    case 'S':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, b->msg_count);
      }
      else if (b->msg_count == 0)
        optional = 0;
      break;

    case 't':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, c ? Context->tagged : 0);
      }
      else if ((c && Context->tagged == 0) || !c)
        optional = 0;
      break;

    case '!':
      if (b->msg_flagged == 0)
        mutt_format_s(dest, destlen, prefix, "");
      else if (b->msg_flagged == 1)
        mutt_format_s(dest, destlen, prefix, "!");
      else if (b->msg_flagged == 2)
        mutt_format_s(dest, destlen, prefix, "!!");
      else
      {
        snprintf(fmt, sizeof(fmt), "%d!", b->msg_flagged);
        mutt_format_s(dest, destlen, prefix, fmt);
      }
      break;
  }

  if (optional)
    mutt_FormatString(dest, destlen, col, SidebarWidth, ifstring, cb_format_str,
                      (unsigned long) sbe, flags);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_FormatString(dest, destlen, col, SidebarWidth, elsestring,
                      cb_format_str, (unsigned long) sbe, flags);

  /* We return the format string, unchanged */
  return src;
}

/**
 * make_sidebar_entry - Turn mailbox data into a sidebar string
 * @buf:     Buffer in which to save string
 * @buflen:  Buffer length
 * @width:   Desired width in screen cells
 * @box:     Mailbox name
 * @sbe:     Mailbox object
 *
 * Take all the relevant mailbox data and the desired screen width and then get
 * mutt_FormatString to do the actual work. mutt_FormatString will callback to
 * us using cb_format_str() for the sidebar specific formatting characters.
 */
static void make_sidebar_entry(char *buf, unsigned int buflen, int width,
                               char *box, struct SbEntry *sbe)
{
  if (!buf || !box || !sbe)
    return;

  strfcpy(sbe->box, box, sizeof(sbe->box));

  mutt_FormatString(buf, buflen, 0, width, NONULL(SidebarFormat), cb_format_str,
                    (unsigned long) sbe, 0);

  /* Force string to be exactly the right width */
  int w = mutt_strwidth(buf);
  int s = mutt_strlen(buf);
  width = MIN(buflen, width);
  if (w < width)
  {
    /* Pad with spaces */
    memset(buf + s, ' ', width - w);
    buf[s + width - w] = 0;
  }
  else if (w > width)
  {
    /* Truncate to fit */
    int len = mutt_wstr_trunc(buf, buflen, width, NULL);
    buf[len] = 0;
  }
}

/**
 * cb_qsort_sbe - qsort callback to sort SBENTRYs
 * @a: First  SbEntry to compare
 * @b: Second SbEntry to compare
 *
 * Returns:
 *      -1: a precedes b
 *       0: a and b are identical
 *       1: b precedes a
 */
static int cb_qsort_sbe(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(const struct SbEntry **) a;
  const struct SbEntry *sbe2 = *(const struct SbEntry **) b;
  struct Buffy *b1 = sbe1->buffy;
  struct Buffy *b2 = sbe2->buffy;

  int result = 0;

  switch ((SidebarSortMethod & SORT_MASK))
  {
    case SORT_COUNT:
      result = (b2->msg_count - b1->msg_count);
      break;
    case SORT_UNREAD:
      result = (b2->msg_unread - b1->msg_unread);
      break;
    case SORT_DESC:
      result = mutt_strcmp(b1->desc, b2->desc);
      break;
    case SORT_FLAGGED:
      result = (b2->msg_flagged - b1->msg_flagged);
      break;
    case SORT_PATH:
    {
      result = mutt_inbox_cmp(b1->path, b2->path);
      if (result == 0)
        result = mutt_strcoll(b1->path, b2->path);
      break;
    }
  }

  if (SidebarSortMethod & SORT_REVERSE)
    result = -result;

  return result;
}

/**
 * update_entries_visibility - Should a sidebar_entry be displayed in the sidebar
 *
 * For each SbEntry in the Entries array, check whether we should display it.
 * This is determined by several criteria.  If the Buffy:
 *      is the currently open mailbox
 *      is the currently highlighted mailbox
 *      has unread messages
 *      has flagged messages
 *      is whitelisted
 */
static void update_entries_visibility(void)
{
  short new_only = option(OPTSIDEBARNEWMAILONLY);
  struct SbEntry *sbe = NULL;
  for (int i = 0; i < EntryCount; i++)
  {
    sbe = Entries[i];

    sbe->is_hidden = 0;

    if (!new_only)
      continue;

    if ((i == OpnIndex) || (sbe->buffy->msg_unread > 0) || sbe->buffy->new ||
        (sbe->buffy->msg_flagged > 0))
      continue;

    if (Context && (mutt_strcmp(sbe->buffy->realpath, Context->realpath) == 0))
      /* Spool directory */
      continue;

    if (mutt_find_list(SidebarWhitelist, sbe->buffy->path) ||
        mutt_find_list(SidebarWhitelist, sbe->buffy->desc))
      /* Explicitly asked to be visible */
      continue;

    sbe->is_hidden = 1;
  }
}

/**
 * unsort_entries - Restore Entries array order to match Buffy list order
 */
static void unsort_entries(void)
{
  struct Buffy *cur = get_incoming();
  int i = 0, j;
  struct SbEntry *tmp = NULL;

  while (cur && (i < EntryCount))
  {
    j = i;
    while ((j < EntryCount) && (Entries[j]->buffy != cur))
      j++;
    if (j < EntryCount)
    {
      if (j != i)
      {
        tmp = Entries[i];
        Entries[i] = Entries[j];
        Entries[j] = tmp;
      }
      i++;
    }
    cur = cur->next;
  }
}

/**
 * sort_entries - Sort Entries array.
 *
 * Sort the Entries array according to the current sort config
 * option "sidebar_sort_method". This calls qsort to do the work which calls our
 * callback function "cb_qsort_sbe".
 *
 * Once sorted, the prev/next links will be reconstructed.
 */
static void sort_entries(void)
{
  short ssm = (SidebarSortMethod & SORT_MASK);

  /* These are the only sort methods we understand */
  if ((ssm == SORT_COUNT) || (ssm == SORT_UNREAD) || (ssm == SORT_FLAGGED) || (ssm == SORT_PATH))
    qsort(Entries, EntryCount, sizeof(*Entries), cb_qsort_sbe);
  else if ((ssm == SORT_ORDER) && (SidebarSortMethod != PreviousSort))
    unsort_entries();
}

/**
 * select_next - Selects the next unhidden mailbox
 *
 * Returns:
 *      true: Success
 *      false: Failure
 */
static bool select_next(void)
{
  int entry = HilIndex;

  if (!EntryCount || HilIndex < 0)
    return false;

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
 *
 * Search down the list of mail folders for one containing new mail.
 *
 * Returns:
 *      true: Success
 *      false: Failure
 */
static int select_next_new(void)
{
  int entry = HilIndex;

  if (!EntryCount || HilIndex < 0)
    return false;

  do
  {
    entry++;
    if (entry == EntryCount)
    {
      if (option(OPTSIDEBARNEXTNEWWRAP))
        entry = 0;
      else
        return false;
    }
    if (entry == HilIndex)
      return false;
  } while (!Entries[entry]->buffy->new && !Entries[entry]->buffy->msg_unread);

  HilIndex = entry;
  return true;
}

/**
 * select_prev - Selects the previous unhidden mailbox
 *
 * Returns:
 *      true: Success
 *      false: Failure
 */
static bool select_prev(void)
{
  int entry = HilIndex;

  if (!EntryCount || HilIndex < 0)
    return false;

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
 *
 * Search up the list of mail folders for one containing new mail.
 *
 * Returns:
 *      true: Success
 *      false: Failure
 */
static bool select_prev_new(void)
{
  int entry = HilIndex;

  if (!EntryCount || HilIndex < 0)
    return false;

  do
  {
    entry--;
    if (entry < 0)
    {
      if (option(OPTSIDEBARNEXTNEWWRAP))
        entry = EntryCount - 1;
      else
        return false;
    }
    if (entry == HilIndex)
      return false;
  } while (!Entries[entry]->buffy->new && !Entries[entry]->buffy->msg_unread);

  HilIndex = entry;
  return true;
}

/**
 * select_page_down - Selects the first entry in the next page of mailboxes
 *
 * Returns:
 *      1: Success
 *      0: Failure
 */
static int select_page_down(void)
{
  int orig_hil_index = HilIndex;

  if (!EntryCount || BotIndex < 0)
    return 0;

  HilIndex = BotIndex;
  select_next();
  /* If the rest of the entries are hidden, go up to the last unhidden one */
  if (Entries[HilIndex]->is_hidden)
    select_prev();

  return (orig_hil_index != HilIndex);
}

/**
 * select_page_up - Selects the last entry in the previous page of mailboxes
 *
 * Returns:
 *      1: Success
 *      0: Failure
 */
static int select_page_up(void)
{
  int orig_hil_index = HilIndex;

  if (!EntryCount || TopIndex < 0)
    return 0;

  HilIndex = TopIndex;
  select_prev();
  /* If the rest of the entries are hidden, go down to the last unhidden one */
  if (Entries[HilIndex]->is_hidden)
    select_next();

  return (orig_hil_index != HilIndex);
}

/**
 * prepare_sidebar - Prepare the list of SBENTRYs for the sidebar display
 * @page_size:  The number of lines on a page
 *
 * Before painting the sidebar, we determine which are visible, sort
 * them and set up our page pointers.
 *
 * This is a lot of work to do each refresh, but there are many things that
 * can change outside of the sidebar that we don't hear about.
 *
 * Returns:
 *      false: No, don't draw the sidebar
 *      true: Yes, draw the sidebar
 */
static bool prepare_sidebar(int page_size)
{
  struct SbEntry *opn_entry = NULL, *hil_entry = NULL;
  int page_entries;

  if (!EntryCount || (page_size <= 0))
    return false;

  if (OpnIndex >= 0)
    opn_entry = Entries[OpnIndex];
  if (HilIndex >= 0)
    hil_entry = Entries[HilIndex];

  update_entries_visibility();
  sort_entries();

  for (int i = 0; i < EntryCount; i++)
  {
    if (opn_entry == Entries[i])
      OpnIndex = i;
    if (hil_entry == Entries[i])
      HilIndex = i;
  }

  if ((HilIndex < 0) || Entries[HilIndex]->is_hidden || (SidebarSortMethod != PreviousSort))
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

  /* If OPTSIDEBARNEMAILONLY is set, some entries may be hidden so we
   * need to scan for the framing interval */
  if (option(OPTSIDEBARNEWMAILONLY))
  {
    TopIndex = BotIndex = -1;
    while (BotIndex < HilIndex)
    {
      TopIndex = BotIndex + 1;
      page_entries = 0;
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

  PreviousSort = SidebarSortMethod;
  return true;
}

/**
 * draw_divider - Draw a line between the sidebar and the rest of mutt
 * @num_rows:   Height of the Sidebar
 * @num_cols:   Width of the Sidebar
 *
 * Draw a divider using characters from the config option "sidebar_divider_char".
 * This can be an ASCII or Unicode character.
 * We calculate these characters' width in screen columns.
 *
 * If the user hasn't set $sidebar_divider_char we pick a character for them,
 * respecting the value of $ascii_chars.
 *
 * @return:
 * *    0:  Empty string
 * *    n:  Character occupies n screen columns
 */
static int draw_divider(int num_rows, int num_cols)
{
  if ((num_rows < 1) || (num_cols < 1))
    return 0;

  int i;
  int delim_len;
  enum div_type altchar = SB_DIV_UTF8;

  /* Calculate the width of the delimiter in screen cells */
  delim_len = mutt_strwidth(SidebarDividerChar);
  if (delim_len < 0)
  {
    delim_len = 1; /* Bad character */
  }
  else if (delim_len == 0)
  {
    if (SidebarDividerChar)
      return 0; /* User has set empty string */

    delim_len = 1; /* Unset variable */
  }
  else
  {
    altchar = SB_DIV_USER; /* User config */
  }

  if (option(OPTASCIICHARS) && (altchar != SB_DIV_ASCII))
  {
    /* $ascii_chars overrides Unicode divider chars */
    if (altchar == SB_DIV_UTF8)
    {
      altchar = SB_DIV_ASCII;
    }
    else if (SidebarDividerChar)
    {
      for (i = 0; i < delim_len; i++)
      {
        if (SidebarDividerChar[i] & ~0x7F) /* high-bit is set */
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

  SETCOLOR(MT_COLOR_DIVIDER);

  int col = option(OPTSIDEBARONRIGHT) ? 0 : (SidebarWidth - delim_len);

  for (i = 0; i < num_rows; i++)
  {
    mutt_window_move(MuttSidebarWindow, i, col);

    switch (altchar)
    {
      case SB_DIV_USER:
        addstr(NONULL(SidebarDividerChar));
        break;
      case SB_DIV_ASCII:
        addch('|');
        break;
      case SB_DIV_UTF8:
        addch(ACS_VLINE);
        break;
    }
  }

  return delim_len;
}

/**
 * fill_empty_space - Wipe the remaining Sidebar space
 * @first_row:  Window line to start (0-based)
 * @num_rows:   Number of rows to fill
 * @div_width:  Width in screen characters taken by the divider
 * @num_cols:   Number of columns to fill
 *
 * Write spaces over the area the sidebar isn't using.
 */
static void fill_empty_space(int first_row, int num_rows, int div_width, int num_cols)
{
  /* Fill the remaining rows with blank space */
  SETCOLOR(MT_COLOR_NORMAL);

  if (!option(OPTSIDEBARONRIGHT))
    div_width = 0;
  for (int r = 0; r < num_rows; r++)
  {
    mutt_window_move(MuttSidebarWindow, first_row + r, div_width);

    int i;
    for (i = 0; i < num_cols; i++)
      addch(' ');
  }
}

/**
 * draw_sidebar - Write out a list of mailboxes, on the left
 * @num_rows:   Height of the Sidebar
 * @num_cols:   Width of the Sidebar
 * @div_width:  Width in screen characters taken by the divider
 *
 * Display a list of mailboxes in a panel on the left.  What's displayed will
 * depend on our index markers: TopBuffy, OpnBuffy, HilBuffy, BotBuffy.
 * On the first run they'll be NULL, so we display the top of Mutt's list
 * (Incoming).
 *
 * TopBuffy - first visible mailbox
 * BotBuffy - last  visible mailbox
 * OpnBuffy - mailbox shown in Mutt's Index Panel
 * HilBuffy - Unselected mailbox (the paging follows this)
 *
 * The entries are formatted using "sidebar_format" and may be abbreviated:
 * "sidebar_short_path", indented: "sidebar_folder_indent",
 * "sidebar_indent_string" and sorted: "sidebar_sort_method".  Finally, they're
 * trimmed to fit the available space.
 */
static void draw_sidebar(int num_rows, int num_cols, int div_width)
{
  struct SbEntry *entry = NULL;
  struct Buffy *b = NULL;
  if (TopIndex < 0)
    return;

  int w = MIN(num_cols, (SidebarWidth - div_width));
  int row = 0;
  for (int entryidx = TopIndex; (entryidx < EntryCount) && (row < num_rows); entryidx++)
  {
    entry = Entries[entryidx];
    if (entry->is_hidden)
      continue;
    b = entry->buffy;

    if (entryidx == OpnIndex)
    {
      if ((ColorDefs[MT_COLOR_SB_INDICATOR] != 0))
        SETCOLOR(MT_COLOR_SB_INDICATOR);
      else
        SETCOLOR(MT_COLOR_INDICATOR);
    }
    else if (entryidx == HilIndex)
      SETCOLOR(MT_COLOR_HIGHLIGHT);
    else if ((b->msg_unread > 0) || (b->new))
      SETCOLOR(MT_COLOR_NEW);
    else if (b->msg_flagged > 0)
      SETCOLOR(MT_COLOR_FLAGGED);
    else if ((ColorDefs[MT_COLOR_SB_SPOOLFILE] != 0) &&
             (mutt_strcmp(b->path, Spoolfile) == 0))
      SETCOLOR(MT_COLOR_SB_SPOOLFILE);
    else
    {
      if (ColorDefs[MT_COLOR_ORDINARY] != 0)
        SETCOLOR(MT_COLOR_ORDINARY);
      else
        SETCOLOR(MT_COLOR_NORMAL);
    }

    int col = 0;
    if (option(OPTSIDEBARONRIGHT))
      col = div_width;

    mutt_window_move(MuttSidebarWindow, row, col);
    if (Context && Context->realpath && (mutt_strcmp(b->realpath, Context->realpath) == 0))
    {
#ifdef USE_NOTMUCH
      if (b->magic == MUTT_NOTMUCH)
        nm_nonctx_get_count(b->realpath, &b->msg_count, &b->msg_unread);
      else
#endif
      {
        b->msg_unread = Context->unread;
        b->msg_count = Context->msgcount;
      }
      b->msg_flagged = Context->flagged;
    }

    /* compute length of Maildir without trailing separator */
    size_t maildirlen = mutt_strlen(Maildir);
    if (maildirlen && SidebarDelimChars &&
        strchr(SidebarDelimChars, Maildir[maildirlen - 1]))
      maildirlen--;

    /* check whether Maildir is a prefix of the current folder's path */
    bool maildir_is_prefix = false;
    if ((mutt_strlen(b->path) > maildirlen) &&
        (mutt_strncmp(Maildir, b->path, maildirlen) == 0) &&
        SidebarDelimChars && strchr(SidebarDelimChars, b->path[maildirlen]))
      maildir_is_prefix = true;

    /* calculate depth of current folder and generate its display name with indented spaces */
    int sidebar_folder_depth = 0;
    char *sidebar_folder_name = NULL;
    int i;
    if (option(OPTSIDEBARSHORTPATH))
    {
      /* disregard a trailing separator, so strlen() - 2 */
      sidebar_folder_name = b->path;
      for (i = mutt_strlen(sidebar_folder_name) - 2; i >= 0; i--)
      {
        if (SidebarDelimChars && strchr(SidebarDelimChars, sidebar_folder_name[i]))
        {
          sidebar_folder_name += (i + 1);
          break;
        }
      }
    }
    else
      sidebar_folder_name = b->path + maildir_is_prefix * (maildirlen + 1);

    if (maildir_is_prefix && option(OPTSIDEBARFOLDERINDENT))
    {
      const char *tmp_folder_name = NULL;
      int lastsep = 0;
      tmp_folder_name = b->path + maildirlen + 1;
      int tmplen = (int) mutt_strlen(tmp_folder_name) - 1;
      for (i = 0; i < tmplen; i++)
      {
        if (SidebarDelimChars && strchr(SidebarDelimChars, tmp_folder_name[i]))
        {
          sidebar_folder_depth++;
          lastsep = i + 1;
        }
      }
      if (sidebar_folder_depth > 0)
      {
        if (option(OPTSIDEBARSHORTPATH))
          tmp_folder_name += lastsep; /* basename */
        int sfn_len = mutt_strlen(tmp_folder_name) +
                      sidebar_folder_depth * mutt_strlen(SidebarIndentString) + 1;
        sidebar_folder_name = safe_malloc(sfn_len);
        sidebar_folder_name[0] = 0;
        for (i = 0; i < sidebar_folder_depth; i++)
          safe_strcat(sidebar_folder_name, sfn_len, NONULL(SidebarIndentString));
        safe_strcat(sidebar_folder_name, sfn_len, tmp_folder_name);
      }
    }
#ifdef USE_NOTMUCH
    else if (b->magic == MUTT_NOTMUCH)
    {
      sidebar_folder_name = b->desc;
    }
#endif
    char str[STRING];
    make_sidebar_entry(str, sizeof(str), w, sidebar_folder_name, entry);
    printw("%s", str);
    if (sidebar_folder_depth > 0)
      FREE(&sidebar_folder_name);
    row++;
  }

  fill_empty_space(row, num_rows - row, div_width, w);
}


/**
 * mutt_sb_draw - Completely redraw the sidebar
 *
 * Completely refresh the sidebar region.  First draw the divider; then, for
 * each Buffy, call make_sidebar_entry; finally blank out any remaining space.
 */
void mutt_sb_draw(void)
{
  if (!option(OPTSIDEBAR))
    return;

#ifdef USE_SLANG_CURSES
  int x = SLsmg_get_column();
  int y = SLsmg_get_row();
#else
  int x = getcurx(stdscr);
  int y = getcury(stdscr);
#endif

  int num_rows = MuttSidebarWindow->rows;
  int num_cols = MuttSidebarWindow->cols;

  int div_width = draw_divider(num_rows, num_cols);

  struct Buffy *b = NULL;
  if (!Entries)
    for (b = get_incoming(); b; b = b->next)
      mutt_sb_notify_mailbox(b, 1);

  if (!get_incoming())
  {
    fill_empty_space(0, num_rows, div_width, num_cols - div_width);
    return;
  }

  if (!prepare_sidebar(num_rows))
    return;

  draw_sidebar(num_rows, num_cols, div_width);
  move(y, x);
}

/**
 * mutt_sb_change_mailbox - Change the selected mailbox
 * @op: Operation code
 *
 * Change the selected mailbox, e.g. "Next mailbox", "Previous Mailbox
 * with new mail". The operations are listed OPS.SIDEBAR which is built
 * into an enum in keymap_defs.h.
 *
 * If the operation is successful, HilBuffy will be set to the new mailbox.
 * This function only *selects* the mailbox, doesn't *open* it.
 *
 * Allowed values are: OP_SIDEBAR_NEXT, OP_SIDEBAR_NEXT_NEW,
 * OP_SIDEBAR_PAGE_DOWN, OP_SIDEBAR_PAGE_UP, OP_SIDEBAR_PREV,
 * OP_SIDEBAR_PREV_NEW.
 */
void mutt_sb_change_mailbox(int op)
{
  if (!option(OPTSIDEBAR))
    return;

  if (HilIndex < 0) /* It'll get reset on the next draw */
    return;

  switch (op)
  {
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
  mutt_set_current_menu_redraw(REDRAW_SIDEBAR);
}

/**
 * mutt_sb_set_buffystats - Update the Buffy's message counts from the Context
 * @ctx:  A mailbox Context
 *
 * Given a mailbox Context, find a matching mailbox Buffy and copy the message
 * counts into it.
 */
void mutt_sb_set_buffystats(const struct Context *ctx)
{
  /* Even if the sidebar's hidden,
   * we should take note of the new data. */
  struct Buffy *b = get_incoming();
  if (!ctx || !b)
    return;

  for (; b; b = b->next)
  {
    if (mutt_strcmp(b->realpath, ctx->realpath) == 0)
    {
      b->msg_unread = ctx->unread;
      b->msg_count = ctx->msgcount;
      b->msg_flagged = ctx->flagged;
      break;
    }
  }
}

/**
 * mutt_sb_get_highlight - Get the Buffy that's highlighted in the sidebar
 *
 * Get the path of the mailbox that's highlighted in the sidebar.
 *
 * Returns:
 *      Mailbox path
 */
const char *mutt_sb_get_highlight(void)
{
  if (!option(OPTSIDEBAR))
    return NULL;

  if (!EntryCount || HilIndex < 0)
    return NULL;

  return Entries[HilIndex]->buffy->path;
}

/**
 * mutt_sb_set_open_buffy - Set the OpnBuffy based on the global Context
 *
 * Search through the list of mailboxes.  If a Buffy has a matching path, set
 * OpnBuffy to it.
 */
void mutt_sb_set_open_buffy(void)
{
  OpnIndex = -1;

  if (!Context)
    return;

  for (int entry = 0; entry < EntryCount; entry++)
  {
    if (mutt_strcmp(Entries[entry]->buffy->realpath, Context->realpath) == 0)
    {
      OpnIndex = entry;
      HilIndex = entry;
      break;
    }
  }
}

/**
 * mutt_sb_notify_mailbox - The state of a Buffy is about to change
 *
 * We receive a notification:
 *      After a new Buffy has been created
 *      Before a Buffy is deleted
 *
 * Before a deletion, check that our pointers won't be invalidated.
 */
void mutt_sb_notify_mailbox(struct Buffy *b, int created)
{
  int del_index;

  if (!b)
    return;

  if (sidebar_source == SB_SRC_NONE)
    return;

  /* Any new/deleted mailboxes will cause a refresh.  As long as
   * they're valid, our pointers will be updated in prepare_sidebar() */

  if (created)
  {
    if (EntryCount >= EntryLen)
    {
      EntryLen += 10;
      safe_realloc(&Entries, EntryLen * sizeof(struct SbEntry *));
    }
    Entries[EntryCount] = safe_calloc(1, sizeof(struct SbEntry));
    Entries[EntryCount]->buffy = b;

    if (TopIndex < 0)
      TopIndex = EntryCount;
    if (BotIndex < 0)
      BotIndex = EntryCount;
    if ((OpnIndex < 0) && Context && (mutt_strcmp(b->realpath, Context->realpath) == 0))
      OpnIndex = EntryCount;

    EntryCount++;
  }
  else
  {
    for (del_index = 0; del_index < EntryCount; del_index++)
      if (Entries[del_index]->buffy == b)
        break;
    if (del_index == EntryCount)
      return;
    FREE(&Entries[del_index]);
    EntryCount--;

    if (TopIndex > del_index || TopIndex == EntryCount)
      TopIndex--;
    if (OpnIndex == del_index)
      OpnIndex = -1;
    else if (OpnIndex > del_index)
      OpnIndex--;
    if (HilIndex > del_index || HilIndex == EntryCount)
      HilIndex--;
    if (BotIndex > del_index || BotIndex == EntryCount)
      BotIndex--;

    for (; del_index < EntryCount; del_index++)
      Entries[del_index] = Entries[del_index + 1];
  }

  mutt_set_current_menu_redraw(REDRAW_SIDEBAR);
}

/**
 * mutt_sb_toggle_virtual - Switch between regular and virtual folders
 */
void mutt_sb_toggle_virtual(void)
{
  if (sidebar_source == -1)
    get_incoming();

#ifdef USE_NOTMUCH
  if ((sidebar_source == SB_SRC_INCOMING) && VirtIncoming)
    sidebar_source = SB_SRC_VIRT;
  else
#endif
    sidebar_source = SB_SRC_INCOMING;

  TopIndex = -1;
  OpnIndex = -1;
  HilIndex = -1;
  BotIndex = -1;

  struct Buffy *b = NULL;

  EntryCount = 0;
  FREE(&Entries);
  EntryLen = 0;
  for (b = get_incoming(); b; b = b->next)
    mutt_sb_notify_mailbox(b, 1);

  mutt_set_current_menu_redraw(REDRAW_SIDEBAR);
}
