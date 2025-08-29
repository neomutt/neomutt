/**
 * @file
 * Routines for querying an external address book
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020-2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
 * | Name                 | Type         | See Also    |
 * | :------------------- | :----------- | :---------- |
 * | Address Query Dialog | WT_DLG_QUERY | dlg_query() |
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
#include "editor/lib.h"
#include "expando/lib.h"
#include "history/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "send/lib.h"
#include "alias.h"
#include "functions.h"
#include "globals.h"
#include "gui.h"
#include "mutt_logging.h"

const struct ExpandoRenderData QueryRenderData[];

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
    {
      first->personal = buf_new(alias->name);
    }

    mutt_addrlist_to_intl(al, NULL);
  }

  return true;
}

/**
 * query_a - Query: Address - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void query_a(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  struct Buffer *addrs = buf_pool_get();
  mutt_addrlist_write(&alias->addr, addrs, true);

  buf_printf(buf, "<%s>", buf_string(addrs));
}

/**
 * query_c_num - Query: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long query_c_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AliasView *av = data;

  return av->num + 1;
}

/**
 * query_e - Query: Extra information - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void query_e(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  const char *s = alias->comment;
  buf_strcpy(buf, s);
}

/**
 * query_n - Query: Name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void query_n(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  const char *s = alias->name;
  buf_strcpy(buf, s);
}

/**
 * query_t_num - Query: Tagged char - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long query_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AliasView *av = data;
  return av->is_tagged;
}

/**
 * query_t - Query: Tagged char - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void query_t(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct AliasView *av = data;

  // NOTE(g0mb4): use $flag_chars?
  const char *s = av->is_tagged ? "*" : " ";
  buf_strcpy(buf, s);
}

/**
 * query_Y - Query: Tags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void query_Y(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
             struct Buffer *buf)
{
  const struct AliasView *av = data;

  alias_tags_to_buffer(&av->alias->tags, buf);
}

/**
 * query_make_entry - Format an Alias for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $query_format
 */
static int query_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  const struct AliasMenuData *mdata = menu->mdata;
  const struct AliasViewArray *ava = &mdata->ava;
  struct AliasView *av = ARRAY_GET(ava, line);

  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  if (c_arrow_cursor)
  {
    const char *const c_arrow_string = cs_subset_string(menu->sub, "arrow_string");
    max_cols -= (mutt_strwidth(c_arrow_string) + 1);
  }

  const struct Expando *c_query_format = cs_subset_expando(mdata->sub, "query_format");
  return expando_filter(c_query_format, QueryRenderData, av,
                        MUTT_FORMAT_ARROWCURSOR, max_cols, buf);
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
  char *tok = NULL;
  char *next_tok = NULL;
  struct Buffer *cmd = buf_pool_get();

  const char *const c_query_command = cs_subset_string(sub, "query_command");
  buf_file_expand_fmt_quote(cmd, c_query_command, s);

  pid_t pid = filter_create(buf_string(cmd), NULL, &fp, NULL, EnvList);
  if (pid < 0)
  {
    mutt_debug(LL_DEBUG1, "unable to fork command: %s\n", buf_string(cmd));
    buf_pool_release(&cmd);
    return -1;
  }
  buf_pool_release(&cmd);

  if (verbose)
    mutt_message(_("Waiting for response..."));

  /* The query protocol first reads one NL-terminated line. If an error
   * occurs, this is assumed to be an error message. Otherwise it's ignored. */
  msg = mutt_file_read_line(msg, &msglen, fp, NULL, MUTT_RL_NO_FLAGS);
  while ((buf = mutt_file_read_line(buf, &buflen, fp, NULL, MUTT_RL_NO_FLAGS)))
  {
    tok = buf;
    next_tok = strchr(tok, '\t');
    if (next_tok)
      *next_tok++ = '\0';

    if (*tok == '\0')
      continue;

    struct Alias *alias = alias_new();

    mutt_addrlist_parse(&alias->addr, tok);

    if (next_tok)
    {
      tok = next_tok;
      next_tok = strchr(tok, '\t');
      if (next_tok)
        *next_tok++ = '\0';

      alias->name = mutt_str_dup(tok);
      parse_alias_comments(alias, next_tok);
    }

    TAILQ_INSERT_TAIL(al, alias, entries);
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

  notify_observer_remove(NeoMutt->sub->notify, alias_config_observer, menu);
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
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, alias_config_observer, menu);
  notify_observer_add(win_menu->notify, NT_WINDOW, query_window_observer, win_menu);

  return dlg;
}

/**
 * dlg_query - Get the user to enter an Address Query - @ingroup gui_dlg
 * @param buf   Buffer for the query
 * @param mdata Menu data holding Aliases
 * @retval true Selection was made
 *
 * The Select Query Dialog is an Address Book.
 * It is dynamically created from an external source using $query_command.
 *
 * The user can select addresses to add to an Email.
 */
static bool dlg_query(struct Buffer *buf, struct AliasMenuData *mdata)
{
  struct MuttWindow *dlg = query_dialog_new(mdata, buf_string(buf));
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

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int rc = 0;
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_QUERY, GETCH_NO_FLAGS);
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

  window_set_focus(old_focus);
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
  mdata.search_state = search_state_new();

  struct AliasList al = TAILQ_HEAD_INITIALIZER(al);
  const char *const c_query_command = cs_subset_string(sub, "query_command");
  if (!c_query_command)
  {
    mutt_warning(_("Query command not defined"));
    goto done;
  }

  query_run(buf_string(buf), true, &al, sub);
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
      buf_reset(buf);
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
  if (!dlg_query(buf, &mdata))
    goto done;

  buf_reset(buf);
  buf_alloc(buf, 8192);
  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, &mdata.ava)
  {
    if (!avp->is_tagged)
      continue;

    struct AddressList al_copy = TAILQ_HEAD_INITIALIZER(al_copy);
    if (alias_to_addrlist(&al_copy, avp->alias))
    {
      mutt_addrlist_to_local(&al_copy);
      mutt_addrlist_write(&al_copy, buf, false);
      mutt_addrlist_clear(&al_copy);
    }
    buf_addstr(buf, ", ");
  }

done:
  ARRAY_FREE(&mdata.ava);
  FREE(&mdata.title);
  FREE(&mdata.limit);
  search_state_free(&mdata.search_state);
  aliaslist_clear(&al);
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
  mdata.search_state = search_state_new();

  struct Buffer *buf = buf_pool_get();
  if ((mw_get_field(_("Query: "), buf, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) != 0) ||
      buf_is_empty(buf))
  {
    goto done;
  }

  query_run(buf_string(buf), false, &al, sub);
  if (TAILQ_EMPTY(&al))
    goto done;

  struct Alias *np = NULL;
  TAILQ_FOREACH(np, mdata.al, entries)
  {
    alias_array_alias_add(&mdata.ava, np);
  }

  if (!dlg_query(buf, &mdata))
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
  search_state_free(&mdata.search_state);
  aliaslist_clear(&al);
  buf_pool_release(&buf);
}

/**
 * QueryRenderData - Callbacks for Query Expandos
 *
 * @sa QueryFormatDef, ExpandoDataAlias, ExpandoDataGlobal
 */
const struct ExpandoRenderData QueryRenderData[] = {
  // clang-format off
  { ED_ALIAS,  ED_ALI_ADDRESS, query_a,     NULL },
  { ED_ALIAS,  ED_ALI_NUMBER,  NULL,        query_c_num },
  { ED_ALIAS,  ED_ALI_COMMENT, query_e,     NULL },
  { ED_ALIAS,  ED_ALI_NAME,    query_n,     NULL },
  { ED_ALIAS,  ED_ALI_TAGGED,  query_t,     query_t_num },
  { ED_ALIAS,  ED_ALI_TAGS,    query_Y,     NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
