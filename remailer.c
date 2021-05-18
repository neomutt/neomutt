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
 * @page neo_remailer Support of Mixmaster anonymous remailer
 *
 * Support of Mixmaster anonymous remailer
 */

#include "config.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "send/lib.h"
#include "format_flags.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#ifdef MIXMASTER
#include "remailer.h"
#endif

/**
 * struct Coord - Screen coordinates
 */
struct Coord
{
  short r; ///< row
  short c; ///< column
};

/// Help Bar for the Mixmaster dialog
static const struct Mapping RemailerHelp[] = {
  // clang-format off
  { N_("Append"), OP_MIX_APPEND },
  { N_("Insert"), OP_MIX_INSERT },
  { N_("Delete"), OP_MIX_DELETE },
  { N_("Abort"),  OP_EXIT },
  { N_("OK"),     OP_MIX_USE },
  { NULL, 0 },
  // clang-format on
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
  const char *const c_mixmaster = cs_subset_string(NeoMutt->sub, "mixmaster");
  mutt_buffer_printf(cmd, "%s -T", c_mixmaster);

  pid_t mm_pid =
      filter_create_fd(mutt_buffer_string(cmd), NULL, &fp, NULL, fd_null, -1, fd_null);
  if (mm_pid == -1)
  {
    mutt_buffer_pool_release(&cmd);
    close(fd_null);
    return NULL;
  }

  mutt_buffer_pool_release(&cmd);

  /* first, generate the "random" remailer */

  p = remailer_new();
  p->shortname = mutt_str_dup(_("<random>"));
  mix_add_entry(&type2_list, p, &slots, &used);

  while (fgets(line, sizeof(line), fp))
  {
    p = remailer_new();

    t = strtok(line, " \t\n");
    if (!t)
      goto problem;

    p->shortname = mutt_str_dup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->addr = mutt_str_dup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->ver = mutt_str_dup(t);

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
  filter_wait(mm_pid);

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
  const int wrap_indent = 2;

  if (!chain->cl)
    return;

  short c, r;

  mutt_mem_realloc(coordsp, sizeof(struct Coord) * chain->cl);

  struct Coord *coords = *coordsp;

  if (i == 0)
  {
    r = 0;
    c = 0;
  }
  else
  {
    c = coords[i - 1].c + strlen(type2_list[chain->ch[i - 1]]->shortname) + 2;
    r = coords[i - 1].r;
  }

  for (; i < chain->cl; i++)
  {
    short oc = c;
    c += strlen(type2_list[chain->ch[i]]->shortname) + 2;

    if (c >= win->state.cols)
    {
      oc = wrap_indent;
      c = wrap_indent;
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

  if (coords[i].r < win->state.rows)
  {
    if (selected)
      mutt_curses_set_color(MT_COLOR_INDICATOR);
    else
      mutt_curses_set_color(MT_COLOR_NORMAL);

    mutt_window_mvaddstr(win, coords[i].c, coords[i].r, type2_list[chain->ch[i]]->shortname);
    mutt_curses_set_color(MT_COLOR_NORMAL);

    if (i + 1 < chain->cl)
      mutt_window_addstr(win, ", ");
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
  for (int i = 0; i < win->state.rows; i++)
  {
    mutt_window_move(win, 0, i);
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
  char buf[1024];
  snprintf(buf, sizeof(buf), "-- Remailer chain [Length: %ld]", chain ? chain->cl : 0);
  sbar_set_title(win, buf);
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
                                  intptr_t data, MuttFormatFlags flags)
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
      if (optional)
        break;

      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, mix_format_caps(remailer));
      break;

    case 'n':
      if (optional)
        break;

      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, remailer->num);
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
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, mix_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, mix_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * mix_make_entry - Format a menu item for the mixmaster chain list - Implements Menu::make_entry()
 */
static void mix_make_entry(struct Menu *menu, char *buf, size_t buflen, int num)
{
  struct Remailer **type2_list = menu->mdata;
  const char *const c_mix_entry_format =
      cs_subset_string(NeoMutt->sub, "mix_entry_format");
  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(c_mix_entry_format), mix_format_str,
                      (intptr_t) type2_list[num], MUTT_FORMAT_ARROWCURSOR);
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

  if (mutt_str_equal(s, "0") || mutt_istr_equal(s, "<random>"))
  {
    chain->ch[chain->cl++] = 0;
    return 0;
  }

  for (i = 0; type2_list[i]; i++)
  {
    if (mutt_istr_equal(s, type2_list[i]->shortname))
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
 * remailer_config_observer - Listen for config changes affecting the remailer - Implements ::observer_t
 */
static int remailer_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  const bool c_status_on_top = cs_subset_bool(ev_c->sub, "status_on_top");
  struct MuttWindow *win_move = NULL;
  if (c_status_on_top)
  {
    win_move = TAILQ_LAST(&dlg->children, MuttWindowList);
    TAILQ_REMOVE(&dlg->children, win_move, entries);
    TAILQ_INSERT_HEAD(&dlg->children, win_move, entries);
  }
  else
  {
    win_move = TAILQ_FIRST(&dlg->children);
    TAILQ_REMOVE(&dlg->children, win_move, entries);
    TAILQ_INSERT_TAIL(&dlg->children, win_move, entries);
  }

  mutt_window_reflow(dlg);
  mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
  return 0;
}

/**
 * dlg_select_mixmaster_chain - Create a Mixmaster chain
 * @param chainhead List of chain links
 *
 * Ask the user to select Mixmaster hosts to create a chain.
 */
void dlg_select_mixmaster_chain(struct ListHead *chainhead)
{
  int c_cur = 0, c_old = 0;
  bool c_redraw = true;
  size_t ttll = 0;

  struct Coord *coords = NULL;

  struct Menu *menu = NULL;
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

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_REMAILER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->help_menu = MENU_MIX;
  dlg->help_data = RemailerHelp;

  struct MuttWindow *win_hosts = menu_new_window(MENU_MIX, NeoMutt->sub);
  win_hosts->focus = win_hosts;

  struct MuttWindow *win_chain =
      mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                      MUTT_WIN_SIZE_UNLIMITED, 4);

  struct MuttWindow *win_cbar = sbar_create(dlg);
  struct MuttWindow *win_rbar = sbar_create(dlg);

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(dlg, win_rbar);
    mutt_window_add_child(dlg, win_hosts);
    mutt_window_add_child(dlg, win_cbar);
    mutt_window_add_child(dlg, win_chain);
  }
  else
  {
    mutt_window_add_child(dlg, win_hosts);
    mutt_window_add_child(dlg, win_cbar);
    mutt_window_add_child(dlg, win_chain);
    mutt_window_add_child(dlg, win_rbar);
  }
  sbar_set_title(win_rbar, _("Select a remailer chain"));

  mix_screen_coordinates(dlg, type2_list, &coords, chain, 0);

  menu = win_hosts->wdata;
  menu->max = ttll;
  menu->make_entry = mix_make_entry;
  menu->tag = NULL;
  menu->mdata = type2_list;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, remailer_config_observer, dlg);
  dialog_push(dlg);

  while (loop)
  {
    if (c_redraw)
    {
      mix_redraw_head(win_cbar, chain);
      mix_redraw_chain(win_chain, type2_list, coords, chain, c_cur);
      c_redraw = false;
    }
    else if (c_cur != c_old)
    {
      mix_redraw_ce(win_chain, type2_list, coords, chain, c_old, false);
      mix_redraw_ce(win_chain, type2_list, coords, chain, c_cur, true);
    }

    c_old = c_cur;

    window_redraw(dlg, true);
    const int op = menu_loop(menu);
    switch (op)
    {
      case OP_REDRAW:
      {
        mix_redraw_head(win_cbar, chain);
        mix_screen_coordinates(menu->win_index, type2_list, &coords, chain, 0);
        mix_redraw_chain(win_chain, type2_list, coords, chain, c_cur);
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
          chain->ch[0] = menu_get_index(menu);
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

          chain->ch[c_cur] = menu_get_index(menu);
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

  dialog_pop();
  notify_observer_remove(NeoMutt->notify, remailer_config_observer, dlg);

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

      mutt_list_insert_tail(chainhead, mutt_str_dup(t));
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
    const char *fqdn = mutt_fqdn(true, NeoMutt->sub);
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

  const char *const c_mixmaster = cs_subset_string(NeoMutt->sub, "mixmaster");
  mutt_buffer_printf(cmd, "cat %s | %s -m ", tempfile, c_mixmaster);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, chain, entries)
  {
    mutt_buffer_addstr(cmd, (i != 0) ? "," : " -l ");
    mutt_buffer_quote_filename(cd_quoted, (char *) np->data, true);
    mutt_buffer_addstr(cmd, mutt_buffer_string(cd_quoted));
    i = 1;
  }

  mutt_endwin();

  i = mutt_system(cmd->data);
  if (i != 0)
  {
    fprintf(stderr, _("Error sending message, child exited %d\n"), i);
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
