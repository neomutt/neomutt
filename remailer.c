/**
 * @file
 * Support of Mixmaster anonymous remailer
 *
 * @authors
 * Copyright (C) 1999-2001 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page remailer Support of Mixmaster anonymous remailer
 *
 * Support of Mixmaster anonymous remailer
 */

#include "config.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "recvattach.h"
#include "sendlib.h"
#ifdef MIXMASTER
#include "remailer.h"
#endif

/* These Config Variables are only used in remailer.c */
char *C_MixEntryFormat; ///< Config: (mixmaster) printf-like format string for the mixmaster chain
char *C_Mixmaster; ///< Config: (mixmaster) External command to route a mixmaster message

#define MIX_HOFFSET 2
#define MIX_VOFFSET (win->state.rows - 4)
#define MIX_MAXROW (win->state.rows - 1)

/**
 * struct Coord - Screen coordinates
 */
struct Coord
{
  short r; ///< row
  short c; ///< column
};

static const struct Mapping RemailerHelp[] = {
  { N_("Append"), OP_MIX_APPEND }, { N_("Insert"), OP_MIX_INSERT },
  { N_("Delete"), OP_MIX_DELETE }, { N_("Abort"), OP_EXIT },
  { N_("OK"), OP_MIX_USE },        { NULL, 0 },
};

/**
 * mix_get_caps - Get Mixmaster Capabilities
 * @param capstr Capability string to parse
 * @retval num Capabilities, see #MixCapFlags
 */
static MixCapFlags mix_get_caps(const char *capstr)
{
  MixCapFlags caps = MIX_CAP_NO_FLAGS;

  while (*capstr)
  {
    switch (*capstr)
    {
      case 'C':
        caps |= MIX_CAP_COMPRESS;
        break;

      case 'M':
        caps |= MIX_CAP_MIDDLEMAN;
        break;

      case 'N':
      {
        switch (*++capstr)
        {
          case 'm':
            caps |= MIX_CAP_NEWSMAIL;
            break;

          case 'p':
            caps |= MIX_CAP_NEWSPOST;
            break;
        }
      }
    }

    if (*capstr)
      capstr++;
  }

  return caps;
}

/**
 * mix_add_entry - Add an entry to the Remailer list
 * @param[out] type2_list Remailer list to add to
 * @param[in]  entry      Remailer to add
 * @param[out] slots      Total number of slots
 * @param[out] used       Number of slots used
 */
static void mix_add_entry(struct Remailer ***type2_list, struct Remailer *entry,
                          size_t *slots, size_t *used)
{
  if (*used == *slots)
  {
    *slots += 5;
    mutt_mem_realloc(type2_list, sizeof(struct Remailer *) * (*slots));
  }

  (*type2_list)[(*used)++] = entry;
  if (entry)
    entry->num = *used;
}

/**
 * remailer_new - Create a new Remailer
 * @retval ptr Newly allocated Remailer
 */
static struct Remailer *remailer_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Remailer));
}

/**
 * remailer_free - Free a Remailer
 * @param[out] ptr Remailer to free
 */
static void remailer_free(struct Remailer **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Remailer *r = *ptr;

  FREE(&r->shortname);
  FREE(&r->addr);
  FREE(&r->ver);
  FREE(ptr);
}

/**
 * mix_type2_list - parse the type2.list as given by mixmaster -T
 * @param[out] l Length of list
 * @retval ptr type2.list
 */
static struct Remailer **mix_type2_list(size_t *l)
{
  if (!l)
    return NULL;

  FILE *fp = NULL;
  char line[8192];
  char *t = NULL;

  struct Remailer **type2_list = NULL;
  struct Remailer *p = NULL;
  size_t slots = 0, used = 0;

  int fd_null = open("/dev/null", O_RDWR);
  if (fd_null == -1)
    return NULL;

  struct Buffer *cmd = mutt_buffer_pool_get();
  mutt_buffer_printf(cmd, "%s -T", C_Mixmaster);

  pid_t mm_pid =
      mutt_create_filter_fd(mutt_b2s(cmd), NULL, &fp, NULL, fd_null, -1, fd_null);
  if (mm_pid == -1)
  {
    mutt_buffer_pool_release(&cmd);
    close(fd_null);
    return NULL;
  }

  mutt_buffer_pool_release(&cmd);

  /* first, generate the "random" remailer */

  p = remailer_new();
  p->shortname = mutt_str_strdup(_("<random>"));
  mix_add_entry(&type2_list, p, &slots, &used);

  while (fgets(line, sizeof(line), fp))
  {
    p = remailer_new();

    t = strtok(line, " \t\n");
    if (!t)
      goto problem;

    p->shortname = mutt_str_strdup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->addr = mutt_str_strdup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->ver = mutt_str_strdup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->caps = mix_get_caps(t);

    mix_add_entry(&type2_list, p, &slots, &used);
    continue;

  problem:
    remailer_free(&p);
  }

  *l = used;

  mix_add_entry(&type2_list, NULL, &slots, &used);
  mutt_wait_filter(mm_pid);

  close(fd_null);

  return type2_list;
}

/**
 * mix_type2_list_free - Free a Remailer List
 * @param[out] ttlp Remailer List to free
 */
static void mix_type2_list_free(struct Remailer ***ttlp)
{
  struct Remailer **type2_list = *ttlp;

  for (int i = 0; type2_list[i]; i++)
    remailer_free(&type2_list[i]);

  FREE(type2_list);
}

/**
 * mix_screen_coordinates - Get the screen coordinates to place a chain
 * @param[in]  win        Window
 * @param[out] type2_list Remailer List
 * @param[out] coordsp    On screen coordinates
 * @param[in]  chain      Chain
 * @param[in]  i          Index in chain
 */
static void mix_screen_coordinates(struct MuttWindow *win, struct Remailer **type2_list,
                                   struct Coord **coordsp, struct MixChain *chain, int i)
{
  if (!chain->cl)
    return;

  short c, r;

  mutt_mem_realloc(coordsp, sizeof(struct Coord) * chain->cl);

  struct Coord *coords = *coordsp;

  if (i)
  {
    c = coords[i - 1].c + strlen(type2_list[chain->ch[i - 1]]->shortname) + 2;
    r = coords[i - 1].r;
  }
  else
  {
    r = MIX_VOFFSET;
    c = MIX_HOFFSET;
  }

  for (; i < chain->cl; i++)
  {
    short oc = c;
    c += strlen(type2_list[chain->ch[i]]->shortname) + 2;

    if (c >= win->state.cols)
    {
      oc = MIX_HOFFSET;
      c = MIX_HOFFSET;
      r++;
    }

    coords[i].c = oc;
    coords[i].r = r;
  }
}

/**
 * mix_redraw_ce - Redraw the Remailer chain
 * @param[in]  win        Window
 * @param[out] type2_list Remailer List
 * @param[in]  coords     Screen Coordinates
 * @param[in]  chain      Chain
 * @param[in]  i          Index in chain
 * @param[in]  selected   true, if this item is selected
 */
static void mix_redraw_ce(struct MuttWindow *win, struct Remailer **type2_list,
                          struct Coord *coords, struct MixChain *chain, int i, bool selected)
{
  if (!coords || !chain)
    return;

  if (coords[i].r < MIX_MAXROW)
  {
    if (selected)
      mutt_curses_set_color(MT_COLOR_INDICATOR);
    else
      mutt_curses_set_color(MT_COLOR_NORMAL);

    mutt_window_mvaddstr(win, coords[i].r, coords[i].c, type2_list[chain->ch[i]]->shortname);
    mutt_curses_set_color(MT_COLOR_NORMAL);

    if (i + 1 < chain->cl)
      mutt_window_addstr(", ");
  }
}

/**
 * mix_redraw_chain - Redraw the chain on screen
 * @param[in]  win        Window
 * @param[out] type2_list Remailer List
 * @param[in]  coords     Where to draw the list on screen
 * @param[in]  chain      Chain to display
 * @param[in]  cur        Chain index of current selection
 */
static void mix_redraw_chain(struct MuttWindow *win, struct Remailer **type2_list,
                             struct Coord *coords, struct MixChain *chain, int cur)
{
  for (int i = MIX_VOFFSET; i < MIX_MAXROW; i++)
  {
    mutt_window_move(win, i, 0);
    mutt_window_clrtoeol(win);
  }

  for (int i = 0; i < chain->cl; i++)
    mix_redraw_ce(win, type2_list, coords, chain, i, i == cur);
}

/**
 * mix_redraw_head - Redraw the Chain info
 * @param win   Window
 * @param chain Chain
 */
static void mix_redraw_head(struct MuttWindow *win, struct MixChain *chain)
{
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_mvprintw(win, MIX_VOFFSET - 1, 0,
                       "-- Remailer chain [Length: %d]", chain ? chain->cl : 0);
  mutt_window_clrtoeol(win);
  mutt_curses_set_color(MT_COLOR_NORMAL);
}

/**
 * mix_format_caps - Turn flags into a MixMaster capability string
 * @param r Remailer to use
 * @retval ptr Capability string
 *
 * @note The string is a static buffer
 */
static const char *mix_format_caps(struct Remailer *r)
{
  static char capbuf[10];
  char *t = capbuf;

  if (r->caps & MIX_CAP_COMPRESS)
    *t++ = 'C';
  else
    *t++ = ' ';

  if (r->caps & MIX_CAP_MIDDLEMAN)
    *t++ = 'M';
  else
    *t++ = ' ';

  if (r->caps & MIX_CAP_NEWSPOST)
  {
    *t++ = 'N';
    *t++ = 'p';
  }
  else
  {
    *t++ = ' ';
    *t++ = ' ';
  }

  if (r->caps & MIX_CAP_NEWSMAIL)
  {
    *t++ = 'N';
    *t++ = 'm';
  }
  else
  {
    *t++ = ' ';
    *t++ = ' ';
  }

  *t = '\0';

  return capbuf;
}

/**
 * mix_format_str - Format a string for the remailer menu - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%a     | The remailer's e-mail address
 * | \%c     | Remailer capabilities
 * | \%n     | The running number on the menu
 * | \%s     | The remailer's short name
 */
static const char *mix_format_str(char *buf, size_t buflen, size_t col, int cols,
                                  char op, const char *src, const char *prec,
                                  const char *if_str, const char *else_str,
                                  unsigned long data, MuttFormatFlags flags)
{
  char fmt[128];
  struct Remailer *remailer = (struct Remailer *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(remailer->addr));
      }
      else if (!remailer->addr)
        optional = false;
      break;

    case 'c':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, mix_format_caps(remailer));
      }
      break;

    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, remailer->num);
      }
      break;

    case 's':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(remailer->shortname));
      }
      else if (!remailer->shortname)
        optional = false;
      break;

    default:
      *buf = '\0';
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, attach_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, attach_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  return src;
}

/**
 * mix_entry - Format a menu item for the mixmaster chain list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
static void mix_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  struct Remailer **type2_list = menu->data;
  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(C_MixEntryFormat), mix_format_str,
                      (unsigned long) type2_list[num], MUTT_FORMAT_ARROWCURSOR);
}

/**
 * mix_chain_add - Add a host to the chain
 * @param[in]  chain      Chain to add to
 * @param[in]  s          Hostname
 * @param[out] type2_list Remailer List
 * @retval  0 Success
 * @retval -1 Error
 */
static int mix_chain_add(struct MixChain *chain, const char *s, struct Remailer **type2_list)
{
  int i;

  if (chain->cl >= MAX_MIXES)
    return -1;

  if ((mutt_str_strcmp(s, "0") == 0) || (mutt_str_strcasecmp(s, "<random>") == 0))
  {
    chain->ch[chain->cl++] = 0;
    return 0;
  }

  for (i = 0; type2_list[i]; i++)
  {
    if (mutt_str_strcasecmp(s, type2_list[i]->shortname) == 0)
    {
      chain->ch[chain->cl++] = i;
      return 0;
    }
  }

  /* replace unknown remailers by <random> */

  if (!type2_list[i])
    chain->ch[chain->cl++] = 0;

  return 0;
}

/**
 * mutt_dlg_mixmaster_observer - Listen for config changes affecting the Mixmaster menu - Implements ::observer_t()
 */
int mutt_dlg_mixmaster_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (mutt_str_strcmp(ec->name, "status_on_top") != 0)
    return 0;

  struct MuttWindow *win_first = TAILQ_FIRST(&dlg->children);

  if ((C_StatusOnTop && (win_first->type == WT_INDEX)) ||
      (!C_StatusOnTop && (win_first->type != WT_INDEX)))
  {
    // Swap the Index and the IndexBar Windows
    TAILQ_REMOVE(&dlg->children, win_first, entries);
    TAILQ_INSERT_TAIL(&dlg->children, win_first, entries);
  }

  mutt_window_reflow(dlg);
  return 0;
}

/**
 * mix_make_chain - Create a Mixmaster chain
 * @param win       Window
 * @param chainhead List if chain links
 * @param cols      Number of screen columns
 *
 * Ask the user to select Mixmaster hosts to create a chain.
 */
void mix_make_chain(struct MuttWindow *win, struct ListHead *chainhead, int cols)
{
  int c_cur = 0, c_old = 0;
  bool c_redraw = true;
  size_t ttll = 0;

  struct Coord *coords = NULL;

  struct Menu *menu = NULL;
  char helpstr[1024];
  bool loop = true;

  char *t = NULL;

  struct Remailer **type2_list = mix_type2_list(&ttll);
  if (!type2_list)
  {
    mutt_error(_("Can't get mixmaster's type2.list"));
    return;
  }

  struct MixChain *chain = mutt_mem_calloc(1, sizeof(struct MixChain));

  struct ListNode *p = NULL;
  STAILQ_FOREACH(p, chainhead, entries)
  {
    mix_chain_add(chain, p->data, type2_list);
  }
  mutt_list_free(chainhead);

  /* safety check */
  for (int i = 0; i < chain->cl; i++)
  {
    if (chain->ch[i] >= ttll)
      chain->ch[i] = 0;
  }

  mix_screen_coordinates(win, type2_list, &coords, chain, 0);

  struct MuttWindow *dlg =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->type = WT_DIALOG;
  struct MuttWindow *index =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  index->type = WT_INDEX;
  struct MuttWindow *ibar = mutt_window_new(
      MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 1, MUTT_WIN_SIZE_UNLIMITED);
  ibar->type = WT_INDEX_BAR;

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  notify_observer_add(NeoMutt->notify, mutt_dlg_mixmaster_observer, dlg);
  dialog_push(dlg);

  menu = mutt_menu_new(MENU_MIX);
  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->max = ttll;
  menu->menu_make_entry = mix_entry;
  menu->menu_tag = NULL;
  menu->title = _("Select a remailer chain");
  menu->data = type2_list;
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_MIX, RemailerHelp);
  menu->pagelen = MIX_VOFFSET - 1;
  mutt_menu_push_current(menu);

  while (loop)
  {
    if (menu->pagelen != MIX_VOFFSET - 1)
    {
      menu->pagelen = MIX_VOFFSET - 1;
      menu->redraw = REDRAW_FULL;
    }

    if (c_redraw)
    {
      mix_redraw_head(menu->win_index, chain);
      mix_redraw_chain(menu->win_index, type2_list, coords, chain, c_cur);
      c_redraw = false;
    }
    else if (c_cur != c_old)
    {
      mix_redraw_ce(menu->win_index, type2_list, coords, chain, c_old, false);
      mix_redraw_ce(menu->win_index, type2_list, coords, chain, c_cur, true);
    }

    c_old = c_cur;

    const int op = mutt_menu_loop(menu);
    switch (op)
    {
      case OP_REDRAW:
      {
        menu_redraw_status(menu);
        mix_redraw_head(menu->win_index, chain);
        mix_screen_coordinates(menu->win_index, type2_list, &coords, chain, 0);
        mix_redraw_chain(menu->win_index, type2_list, coords, chain, c_cur);
        menu->pagelen = MIX_VOFFSET - 1;
        break;
      }

      case OP_EXIT:
      {
        chain->cl = 0;
        loop = false;
        break;
      }

      case OP_MIX_USE:
      {
        if (!chain->cl)
        {
          chain->cl++;
          chain->ch[0] = menu->current;
          mix_screen_coordinates(menu->win_index, type2_list, &coords, chain, c_cur);
          c_redraw = true;
        }

        if (chain->cl && chain->ch[chain->cl - 1] &&
            (type2_list[chain->ch[chain->cl - 1]]->caps & MIX_CAP_MIDDLEMAN))
        {
          mutt_error(
              _("Error: %s can't be used as the final remailer of a chain"),
              type2_list[chain->ch[chain->cl - 1]]->shortname);
        }
        else
        {
          loop = false;
        }
        break;
      }

      case OP_GENERIC_SELECT_ENTRY:
      case OP_MIX_APPEND:
      {
        if ((chain->cl < MAX_MIXES) && (c_cur < chain->cl))
          c_cur++;
      }
      /* fallthrough */
      case OP_MIX_INSERT:
      {
        if (chain->cl < MAX_MIXES)
        {
          chain->cl++;
          for (int i = chain->cl - 1; i > c_cur; i--)
            chain->ch[i] = chain->ch[i - 1];

          chain->ch[c_cur] = menu->current;
          mix_screen_coordinates(menu->win_index, type2_list, &coords, chain, c_cur);
          c_redraw = true;
        }
        else
        {
          /* L10N The '%d' here hard-coded to 19 */
          mutt_error(_("Mixmaster chains are limited to %d elements"), MAX_MIXES);
        }

        break;
      }

      case OP_MIX_DELETE:
      {
        if (chain->cl)
        {
          chain->cl--;

          for (int i = c_cur; i < chain->cl; i++)
            chain->ch[i] = chain->ch[i + 1];

          if ((c_cur == chain->cl) && c_cur)
            c_cur--;

          mix_screen_coordinates(menu->win_index, type2_list, &coords, chain, c_cur);
          c_redraw = true;
        }
        else
        {
          mutt_error(_("The remailer chain is already empty"));
        }
        break;
      }

      case OP_MIX_CHAIN_PREV:
      {
        if (c_cur)
          c_cur--;
        else
          mutt_error(_("You already have the first chain element selected"));

        break;
      }

      case OP_MIX_CHAIN_NEXT:
      {
        if (chain->cl && (c_cur < chain->cl - 1))
          c_cur++;
        else
          mutt_error(_("You already have the last chain element selected"));

        break;
      }
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_pop();
  notify_observer_remove(NeoMutt->notify, mutt_dlg_mixmaster_observer, dlg);
  mutt_window_free(&dlg);

  /* construct the remailer list */

  if (chain->cl)
  {
    for (int i = 0; i < chain->cl; i++)
    {
      const int j = chain->ch[i];
      if (j != 0)
        t = type2_list[j]->shortname;
      else
        t = "*";

      mutt_list_insert_tail(chainhead, mutt_str_strdup(t));
    }
  }

  mix_type2_list_free(&type2_list);
  FREE(&coords);
  FREE(&chain);
}

/**
 * mix_check_message - Safety-check the message before passing it to mixmaster
 * @param e Email
 * @retval  0 Success
 * @retval -1 Error
 */
int mix_check_message(struct Email *e)
{
  bool need_hostname = false;

  if (!TAILQ_EMPTY(&e->env->cc) || !TAILQ_EMPTY(&e->env->bcc))
  {
    mutt_error(_("Mixmaster doesn't accept Cc or Bcc headers"));
    return -1;
  }

  /* When using mixmaster, we MUST qualify any addresses since
   * the message will be delivered through remote systems.
   *
   * use_domain won't be respected at this point, hidden_host will.
   */

  struct Address *a = NULL;
  TAILQ_FOREACH(a, &e->env->to, entries)
  {
    if (!a->group && !strchr(a->mailbox, '@'))
    {
      need_hostname = true;
      break;
    }
  }

  if (need_hostname)
  {
    const char *fqdn = mutt_fqdn(true);
    if (!fqdn)
    {
      mutt_error(_("Please set the hostname variable to a proper value when "
                   "using mixmaster"));
      return -1;
    }

    /* Cc and Bcc are empty at this point. */
    mutt_addrlist_qualify(&e->env->to, fqdn);
    mutt_addrlist_qualify(&e->env->reply_to, fqdn);
    mutt_addrlist_qualify(&e->env->mail_followup_to, fqdn);
  }

  return 0;
}

/**
 * mix_send_message - Send an email via Mixmaster
 * @param chain    String list of hosts
 * @param tempfile Temporary file containing email
 * @retval -1  Error
 * @retval >=0 Success (Mixmaster's return code)
 */
int mix_send_message(struct ListHead *chain, const char *tempfile)
{
  int i = 0;
  struct Buffer *cmd = mutt_buffer_pool_get();
  struct Buffer *cd_quoted = mutt_buffer_pool_get();

  mutt_buffer_printf(cmd, "cat %s | %s -m ", tempfile, C_Mixmaster);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, chain, entries)
  {
    mutt_buffer_addstr(cmd, (i != 0) ? "," : " -l ");
    mutt_buffer_quote_filename(cd_quoted, (char *) np->data, true);
    mutt_buffer_addstr(cmd, mutt_b2s(cd_quoted));
    i = 1;
  }

  mutt_endwin();

  i = mutt_system(cmd->data);
  if (i != 0)
  {
    fprintf(stderr, _("Error sending message, child exited %d.\n"), i);
    if (!OptNoCurses)
    {
      mutt_any_key_to_continue(NULL);
      mutt_error(_("Error sending message"));
    }
  }

  mutt_buffer_pool_release(&cmd);
  mutt_buffer_pool_release(&cd_quoted);
  unlink(tempfile);
  return i;
}
