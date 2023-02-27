/**
 * @file
 * Routines for querying an external address book
 *
 * @authors
 * Copyright (C) 1996-2000,2003,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

/**
 * @page alias_dlg_query Address Query Dialog
 *
 * The Address Query Dialog will show aliases from an external query.
 * User can select aliases from the list.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                 | Type         | See Also           |
 * | :------------------- | :----------- | :----------------- |
 * | Address Query Dialog | WT_DLG_QUERY | dlg_select_query() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 * - #AliasMenuData
 *
 * The @ref gui_simple holds a Menu.  The Address Query Dialog stores
 * its data (#AliasMenuData) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                    |
 * | :-------------------- | :------------------------- |
 * | #NT_CONFIG            | alias_config_observer()    |
 * | #NT_WINDOW            | query_window_observer()    |
 * | MuttWindow::recalc()  | alias_recalc()             |
 *
 * The Address Query Dialog doesn't have any specific colours, so it doesn't
 * need to support #NT_COLOR.
 *
 * MuttWindow::recalc() is handled to support custom sorting.
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "enter/lib.h"
#include "menu/lib.h"
#include "send/lib.h"
#include "alias.h"
#include "format_flags.h"
#include "functions.h"
#include "gui.h"
#include "keymap.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "opcodes.h"

/// Help Bar for the Address Query dialog
static const struct Mapping QueryHelp[] = {
  // clang-format off
  { N_("Exit"),       OP_EXIT },
  { N_("Mail"),       OP_MAIL },
  { N_("New Query"),  OP_QUERY },
  { N_("Make Alias"), OP_CREATE_ALIAS },
  { N_("Sort"),       OP_SORT },
  { N_("Rev-Sort"),   OP_SORT_REVERSE },
  { N_("Search"),     OP_SEARCH },
  { N_("Help"),       OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * alias_to_addrlist - Turn an Alias into an AddressList
 * @param al    AddressList to fill (must be empty)
 * @param alias Alias to use
 * @retval true Success
 */
bool alias_to_addrlist(struct AddressList *al, struct Alias *alias)
{
  if (!al || !TAILQ_EMPTY(al) || !alias)
    return false;

  mutt_addrlist_copy(al, &alias->addr, false);
  if (!TAILQ_EMPTY(al))
  {
    struct Address *first = TAILQ_FIRST(al);
    struct Address *second = TAILQ_NEXT(first, entries);
    if (!second && !first->personal)
      first->personal = mutt_str_dup(alias->name);

    mutt_addrlist_to_intl(al, NULL);
  }

  return true;
}

/**
 * query_format_str - Format a string for the query menu - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :-------------------------------------------------------
 * | \%a     | Destination address
 * | \%c     | Current entry number
 * | \%e     | Extra information
 * | \%n     | Destination name
 * | \%t     | `*` if current entry is tagged, a space otherwise
 */
static const char *query_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    intptr_t data, MuttFormatFlags flags)
{
  struct AliasView *av = (struct AliasView *) data;
  struct Alias *alias = av->alias;
  char fmt[128] = { 0 };
  char tmp[256] = { 0 };
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
    {
      struct Buffer *tmpbuf = mutt_buffer_pool_get();
      tmp[0] = '<';
      mutt_addrlist_write(&alias->addr, tmpbuf, true);
      mutt_str_copy(tmp + 1, mutt_buffer_string(tmpbuf), sizeof(tmp) - 1);
      mutt_buffer_pool_release(&tmpbuf);
      const size_t len = strlen(tmp);
      if (len < (sizeof(tmp) - 1))
      {
        tmp[len] = '>';
        tmp[len + 1] = '\0';
      }
      mutt_format_s(buf, buflen, prec, tmp);
      break;
    }
    case 'c':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, av->num + 1);
      break;
    case 'e':
      if (!optional)
        mutt_format_s(buf, buflen, prec, NONULL(alias->comment));
      else if (!alias->comment || (*alias->comment == '\0'))
        optional = false;
      break;
    case 'n':
      mutt_format_s(buf, buflen, prec, NONULL(alias->name));
      break;
    case 't':
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, av->is_tagged ? '*' : ' ');
      break;
    default:
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, op);
      break;
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, query_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, query_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * query_make_entry - Format a menu item for the query list - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $query_format, query_format_str()
 */
static void query_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  const struct AliasMenuData *mdata = menu->mdata;
  const struct AliasViewArray *ava = &mdata->ava;
  struct AliasView *av = ARRAY_GET(ava, line);

  const char *const c_query_format = cs_subset_string(mdata->sub, "query_format");

  mutt_expando_format(buf, buflen, 0, menu->win->state.cols, NONULL(c_query_format),
                      query_format_str, (intptr_t) av, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * query_tag - Tag an entry in the Query Menu - Implements Menu::tag() - @ingroup menu_tag
 */
static int query_tag(struct Menu *menu, int sel, int act)
{
  const struct AliasMenuData *mdata = menu->mdata;
  const struct AliasViewArray *ava = &mdata->ava;
  struct AliasView *av = ARRAY_GET(ava, sel);

  bool ot = av->is_tagged;

  av->is_tagged = ((act >= 0) ? act : !av->is_tagged);
  return av->is_tagged - ot;
}

/**
 * query_run - Run an external program to find Addresses
 * @param s       String to match
 * @param verbose If true, print progress messages
 * @param al      Alias list to fill
 * @param sub     Config items
 * @retval  0 Success
 * @retval -1 Error
 */
int query_run(const char *s, bool verbose, struct AliasList *al, const struct ConfigSubset *sub)
{
  FILE *fp = NULL;
  char *buf = NULL;
  size_t buflen;
  char *msg = NULL;
  size_t msglen = 0;
  char *p = NULL;
  struct Buffer *cmd = mutt_buffer_pool_get();

  const char *const c_query_command = cs_subset_string(sub, "query_command");
  mutt_buffer_file_expand_fmt_quote(cmd, c_query_command, s);

  pid_t pid = filter_create(mutt_buffer_string(cmd), NULL, &fp, NULL);
  if (pid < 0)
  {
    mutt_debug(LL_DEBUG1, "unable to fork command: %s\n", mutt_buffer_string(cmd));
    mutt_buffer_pool_release(&cmd);
    return -1;
  }
  mutt_buffer_pool_release(&cmd);

  if (verbose)
    mutt_message(_("Waiting for response..."));

  /* The query protocol first reads one NL-terminated line. If an error
   * occurs, this is assumed to be an error message. Otherwise it's ignored. */
  msg = mutt_file_read_line(msg, &msglen, fp, NULL, MUTT_RL_NO_FLAGS);
  while ((buf = mutt_file_read_line(buf, &buflen, fp, NULL, MUTT_RL_NO_FLAGS)))
  {
    p = strtok(buf, "\t\n");
    if (p)
    {
      struct Alias *alias = alias_new();

      mutt_addrlist_parse(&alias->addr, p);
      p = strtok(NULL, "\t\n");
      if (p)
      {
        alias->name = mutt_str_dup(p);
        p = strtok(NULL, "\t\n");
        alias->comment = mutt_str_dup(p);
      }
      TAILQ_INSERT_TAIL(al, alias, entries);
    }
  }
  FREE(&buf);
  mutt_file_fclose(&fp);
  if (filter_wait(pid))
  {
    mutt_debug(LL_DEBUG1, "Error: %s\n", msg);
    if (verbose)
      mutt_error("%s", msg);
  }
  else
  {
    if (verbose)
      mutt_message("%s", msg);
  }
  FREE(&msg);

  return 0;
}

/**
 * query_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int query_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->notify, alias_config_observer, menu);
  notify_observer_remove(win_menu->notify, query_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * query_dialog_new - Create an Query Selection Dialog
 * @param mdata Menu data holding Aliases
 * @param query Initial query string
 * @retval ptr New Dialog
 */
static struct MuttWindow *query_dialog_new(struct AliasMenuData *mdata, const char *query)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_QUERY, WT_DLG_QUERY, QueryHelp);

  struct Menu *menu = dlg->wdata;

  menu->make_entry = query_make_entry;
  menu->custom_search = true;
  menu->tag = query_tag;
  menu->max = ARRAY_SIZE(&mdata->ava);
  mdata->title = mutt_str_dup(_("Query"));
  menu->mdata = mdata;
  menu->mdata_free = NULL; // Menu doesn't own the data

  struct MuttWindow *win_menu = menu->win;

  // Override the Simple Dialog's recalc()
  win_menu->recalc = alias_recalc;

  char title[256] = { 0 };
  snprintf(title, sizeof(title), "%s: %s", mdata->title, query);
  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, title);

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->notify, NT_CONFIG, alias_config_observer, menu);
  notify_observer_add(win_menu->notify, NT_WINDOW, query_window_observer, win_menu);

  return dlg;
}

/**
 * dlg_select_query - Get the user to enter an Address Query
 * @param buf   Buffer for the query
 * @param mdata Menu data holding Aliases
 * @retval true Selection was made
 */
static bool dlg_select_query(struct Buffer *buf, struct AliasMenuData *mdata)
{
  struct MuttWindow *dlg = query_dialog_new(mdata, mutt_buffer_string(buf));
  struct Menu *menu = dlg->wdata;
  struct MuttWindow *win_sbar = window_find_child(dlg, WT_STATUS_BAR);
  struct MuttWindow *win_menu = window_find_child(dlg, WT_MENU);
  mdata->menu = menu;
  mdata->sbar = win_sbar;
  mdata->query = buf;

  alias_array_sort(&mdata->ava, mdata->sub);

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, &mdata->ava)
  {
    avp->num = ARRAY_FOREACH_IDX;
  }

  // ---------------------------------------------------------------------------
  // Event Loop
  int rc = 0;
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_QUERY);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_QUERY);
      continue;
    }
    mutt_clear_error();

    rc = alias_function_dispatcher(dlg, op);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(win_menu, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while ((rc != FR_DONE) && (rc != FR_CONTINUE));
  // ---------------------------------------------------------------------------

  simple_dialog_free(&dlg);
  window_redraw(NULL);
  return (rc == FR_CONTINUE); // Was a selection made?
}

/**
 * query_complete - Perform auto-complete using an Address Query
 * @param buf    Buffer for completion
 * @param sub    Config item
 * @retval 0 Always
 */
int query_complete(struct Buffer *buf, struct ConfigSubset *sub)
{
  struct AliasMenuData mdata = { ARRAY_HEAD_INITIALIZER, NULL, sub };

  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
  const char *const c_query_command = cs_subset_string(sub, "query_command");
  if (!c_query_command)
  {
    mutt_warning(_("Query command not defined"));
    goto done;
  }

  query_run(mutt_buffer_string(buf), true, &al, sub);
  if (TAILQ_EMPTY(&al))
    goto done;

  mdata.al = &al;

  struct Alias *a_first = TAILQ_FIRST(&al);
  if (!TAILQ_NEXT(a_first, entries)) // only one response?
  {
    struct AddressList addr = TAILQ_HEAD_INITIALIZER(addr);
    if (alias_to_addrlist(&addr, a_first))
    {
      mutt_addrlist_to_local(&addr);
      mutt_buffer_reset(buf);
      mutt_addrlist_write(&addr, buf, false);
      mutt_addrlist_clear(&addr);
      mutt_clear_error();
    }
    goto done;
  }

  struct Alias *np = NULL;
  TAILQ_FOREACH(np, mdata.al, entries)
  {
    alias_array_alias_add(&mdata.ava, np);
  }

  /* multiple results, choose from query menu */
  if (!dlg_select_query(buf, &mdata))
    goto done;

  mutt_buffer_reset(buf);
  mutt_buffer_alloc(buf, 8192);
  bool first = true;
  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, &mdata.ava)
  {
    if (!avp->is_tagged)
      continue;

    if (!first)
    {
      mutt_buffer_addstr(buf, ", ");
    }

    first = false;
    struct AddressList al_copy = TAILQ_HEAD_INITIALIZER(al_copy);
    if (alias_to_addrlist(&al_copy, avp->alias))
    {
      mutt_addrlist_to_local(&al_copy);
      mutt_addrlist_write(&al_copy, buf, false);
      mutt_addrlist_clear(&al_copy);
    }
  }

done:
  ARRAY_FREE(&mdata.ava);
  FREE(&mdata.title);
  FREE(&mdata.limit);
  aliaslist_free(&al);
  return 0;
}

/**
 * query_index - Perform an Alias Query and display the results
 * @param m   Mailbox
 * @param sub Config item
 */
void query_index(struct Mailbox *m, struct ConfigSubset *sub)
{
  const char *const c_query_command = cs_subset_string(sub, "query_command");
  if (!c_query_command)
  {
    mutt_warning(_("Query command not defined"));
    return;
  }

  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
  struct AliasMenuData mdata = { ARRAY_HEAD_INITIALIZER, NULL, sub };
  mdata.al = &al;

  struct Buffer *buf = mutt_buffer_pool_get();
  if ((mutt_buffer_get_field(_("Query: "), buf, MUTT_COMP_NO_FLAGS, false, NULL,
                             NULL, NULL) != 0) ||
      mutt_buffer_is_empty(buf))
  {
    goto done;
  }

  query_run(mutt_buffer_string(buf), false, &al, sub);
  if (TAILQ_EMPTY(&al))
    goto done;

  struct Alias *np = NULL;
  TAILQ_FOREACH(np, mdata.al, entries)
  {
    alias_array_alias_add(&mdata.ava, np);
  }

  if (!dlg_select_query(buf, &mdata))
    goto done;

  // Prepare the "To:" field of a new email
  struct Email *e = email_new();
  e->env = mutt_env_new();

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, &mdata.ava)
  {
    if (!avp->is_tagged)
      continue;

    struct AddressList al_copy = TAILQ_HEAD_INITIALIZER(al_copy);
    if (alias_to_addrlist(&al_copy, avp->alias))
    {
      mutt_addrlist_copy(&e->env->to, &al_copy, false);
      mutt_addrlist_clear(&al_copy);
    }
  }

  mutt_send_message(SEND_REVIEW_TO, e, NULL, m, NULL, sub);

done:
  ARRAY_FREE(&mdata.ava);
  FREE(&mdata.title);
  FREE(&mdata.limit);
  aliaslist_free(&al);
  mutt_buffer_pool_release(&buf);
}
