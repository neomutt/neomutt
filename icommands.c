/**
 * @file
 * Information commands
 *
 * @authors
 * Copyright (C) 2016 Christopher John Czettel <chris@meicloud.at>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Victor Fernandes <criw@pm.me>
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
 * @page neo_icommands Information commands
 *
 * Information commands
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "icommands.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "functions.h"
#include "init.h"
#include "keymap.h"
#include "muttlib.h"
#include "opcodes.h"
#include "version.h"
#ifdef USE_DEBUG_COLOR
#include "gui/lib.h"
#include "color/lib.h"
#include "pager/private_data.h"
#endif

// clang-format off
static enum CommandResult icmd_bind   (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
#ifdef USE_DEBUG_COLOR
static enum CommandResult icmd_color  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
#endif
static enum CommandResult icmd_set    (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
static enum CommandResult icmd_version(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
// clang-format on

/**
 * ICommandList - All available informational commands
 *
 * @note These commands take precedence over conventional NeoMutt rc-lines
 */
static const struct ICommand ICommandList[] = {
  // clang-format off
  { "bind",     icmd_bind,     0 },
#ifdef USE_DEBUG_COLOR
  { "color",    icmd_color,    0 },
#endif
  { "macro",    icmd_bind,     1 },
  { "set",      icmd_set,      0 },
  { "version",  icmd_version,  0 },
  { NULL, NULL, 0 },
  // clang-format on
};

/**
 * mutt_parse_icommand - Parse an informational command
 * @param line Command to execute
 * @param err  Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Success
 * @retval #MUTT_CMD_WARNING Warning with message: command failed
 * @retval #MUTT_CMD_ERROR
 * - Error (no message): command not found
 * - Error with message: command failed
 */
enum CommandResult mutt_parse_icommand(const char *line, struct Buffer *err)
{
  if (!line || (*line == '\0') || !err)
    return MUTT_CMD_ERROR;

  enum CommandResult rc = MUTT_CMD_ERROR;

  struct Buffer *token = mutt_buffer_pool_get();
  struct Buffer expn = mutt_buffer_make(0);
  mutt_buffer_addstr(&expn, line);
  mutt_buffer_seek(&expn, 0);

  mutt_buffer_reset(err);

  SKIPWS(expn.dptr);
  mutt_extract_token(token, &expn, MUTT_TOKEN_NO_FLAGS);
  for (size_t i = 0; ICommandList[i].name; i++)
  {
    if (!mutt_str_equal(token->data, ICommandList[i].name))
      continue;

    rc = ICommandList[i].parse(token, &expn, ICommandList[i].data, err);
    if (rc != MUTT_CMD_SUCCESS)
      goto finish;

    break; /* Continue with next command */
  }

finish:
  mutt_buffer_pool_release(&token);
  mutt_buffer_dealloc(&expn);
  return rc;
}

/**
 * dump_bind - Dump a bind map to a buffer
 * @param buf  Output buffer
 * @param menu Map menu
 * @param map  Bind keymap
 */
static void dump_bind(struct Buffer *buf, struct Mapping *menu, struct Keymap *map)
{
  char key_binding[32];
  const char *fn_name = NULL;

  km_expand_key(key_binding, sizeof(key_binding), map);
  if (map->op == OP_NULL)
  {
    mutt_buffer_add_printf(buf, "bind %s %s noop\n", menu->name, key_binding);
    return;
  }

  /* The pager and editor menus don't use the generic map,
   * however for other menus try generic first. */
  if ((menu->value != MENU_PAGER) && (menu->value != MENU_EDITOR) && (menu->value != MENU_GENERIC))
  {
    fn_name = mutt_get_func(OpGeneric, map->op);
  }

  /* if it's one of the menus above or generic doesn't find
   * the function, try with its own menu. */
  if (!fn_name)
  {
    const struct MenuFuncOp *funcs = km_get_table(menu->value);
    if (!funcs)
      return;

    fn_name = mutt_get_func(funcs, map->op);
  }

  mutt_buffer_add_printf(buf, "bind %s %s %s\n", menu->name, key_binding, fn_name);
}

/**
 * dump_macro - Dump a macro map to a buffer
 * @param buf  Output buffer
 * @param menu Map menu
 * @param map  Macro keymap
 */
static void dump_macro(struct Buffer *buf, struct Mapping *menu, struct Keymap *map)
{
  char key_binding[MAX_SEQ];
  km_expand_key(key_binding, MAX_SEQ, map);

  struct Buffer tmp = mutt_buffer_make(0);
  escape_string(&tmp, map->macro);

  if (map->desc)
  {
    mutt_buffer_add_printf(buf, "macro %s %s \"%s\" \"%s\"\n", menu->name,
                           key_binding, tmp.data, map->desc);
  }
  else
  {
    mutt_buffer_add_printf(buf, "macro %s %s \"%s\"\n", menu->name, key_binding, tmp.data);
  }

  mutt_buffer_dealloc(&tmp);
}

/**
 * dump_menu - Dumps all the binds or macros maps of a menu into a buffer
 * @param buf   Output buffer
 * @param menu  Menu to dump
 * @param bind  If true it's :bind, else :macro
 * @retval true  Menu is empty
 * @retval false Menu is not empty
 */
static bool dump_menu(struct Buffer *buf, struct Mapping *menu, bool bind)
{
  bool empty = true;
  struct Keymap *map = NULL;

  STAILQ_FOREACH(map, &Keymaps[menu->value], entries)
  {
    if (bind && (map->op != OP_MACRO))
    {
      empty = false;
      dump_bind(buf, menu, map);
    }
    else if (!bind && (map->op == OP_MACRO))
    {
      empty = false;
      dump_macro(buf, menu, map);
    }
  }

  return empty;
}

/**
 * dump_all_menus - Dumps all the binds or macros inside every menu
 * @param buf  Output buffer
 * @param bind If true it's :bind, else :macro
 */
static void dump_all_menus(struct Buffer *buf, bool bind)
{
  for (int i = 0; i < MENU_MAX; i++)
  {
    const char *menu_name = mutt_map_get_name(i, MenuNames);
    struct Mapping menu = { menu_name, i };

    const bool empty = dump_menu(buf, &menu, bind);

    /* Add a new line for readability between menus. */
    if (!empty && (i < (MENU_MAX - 1)))
      mutt_buffer_addch(buf, '\n');
  }
}

/**
 * icmd_bind - Parse 'bind' and 'macro' commands - Implements ICommand::parse()
 */
static enum CommandResult icmd_bind(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  FILE *fp_out = NULL;
  char tempfile[PATH_MAX];
  bool dump_all = false, bind = (data == 0);

  if (!MoreArgs(s))
    dump_all = true;
  else
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (MoreArgs(s))
  {
    /* More arguments potentially means the user is using the
     * ::command_t :bind command thus we delegate the task. */
    return MUTT_CMD_ERROR;
  }

  struct Buffer filebuf = mutt_buffer_make(4096);
  if (dump_all || mutt_istr_equal(buf->data, "all"))
  {
    dump_all_menus(&filebuf, bind);
  }
  else
  {
    const int menu_index = mutt_map_get_value(buf->data, MenuNames);
    if (menu_index == -1)
    {
      // L10N: '%s' is the (misspelled) name of the menu, e.g. 'index' or 'pager'
      mutt_buffer_printf(err, _("%s: no such menu"), buf->data);
      mutt_buffer_dealloc(&filebuf);
      return MUTT_CMD_ERROR;
    }

    struct Mapping menu = { buf->data, menu_index };
    dump_menu(&filebuf, &menu, bind);
  }

  if (mutt_buffer_is_empty(&filebuf))
  {
    // L10N: '%s' is the name of the menu, e.g. 'index' or 'pager',
    //       it might also be 'all' when all menus are affected.
    mutt_buffer_printf(err, bind ? _("%s: no binds for this menu") : _("%s: no macros for this menu"),
                       dump_all ? "all" : buf->data);
    mutt_buffer_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }

  mutt_mktemp(tempfile, sizeof(tempfile));
  fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    mutt_buffer_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }
  fputs(filebuf.data, fp_out);

  mutt_file_fclose(&fp_out);
  mutt_buffer_dealloc(&filebuf);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = tempfile;

  pview.banner = (bind) ? "bind" : "macro";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  return MUTT_CMD_SUCCESS;
}

#ifdef USE_DEBUG_COLOR
/**
 * icmd_color - Parse 'color' command to display colours - Implements ICommand::parse()
 */
static enum CommandResult icmd_color(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  if (MoreArgs(s))
    return MUTT_CMD_ERROR;

  FILE *fp_out = NULL;
  char tempfile[PATH_MAX];
  struct Buffer filebuf = mutt_buffer_make(4096);
  char color_fg[32];
  char color_bg[32];

  mutt_mktemp(tempfile, sizeof(tempfile));
  fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    mutt_buffer_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }

  mutt_buffer_addstr(&filebuf, "# All Colours\n\n");
  mutt_buffer_addstr(&filebuf, "# Simple Colours\n");
  for (enum ColorId cid = MT_COLOR_NONE + 1; cid < MT_COLOR_MAX; cid++)
  {
    struct AttrColor *ac = simple_color_get(cid);
    if (!ac)
      continue;

    struct CursesColor *cc = ac->curses_color;
    if (!cc)
      continue;

    const char *name = mutt_map_get_name(cid, ColorFields);
    if (!name)
      continue;

    const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
    mutt_buffer_add_printf(&filebuf, "color %-18s %-30s %-8s %-8s # %s\n", name,
                           color_debug_log_attrs_list(ac->attrs),
                           color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                           color_debug_log_name(color_bg, sizeof(color_bg), cc->bg), swatch);
  }

  if (NumQuotedColors > 0)
  {
    mutt_buffer_addstr(&filebuf, "\n# Quoted Colours\n");
    for (int i = 0; i < NumQuotedColors; i++)
    {
      struct AttrColor *ac = quoted_colors_get(i);
      if (!ac)
        continue;

      struct CursesColor *cc = ac->curses_color;
      if (!cc)
        continue;

      const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
      mutt_buffer_add_printf(&filebuf, "color quoted%d %-30s %-8s %-8s # %s\n",
                             i, color_debug_log_attrs_list(ac->attrs),
                             color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                             color_debug_log_name(color_bg, sizeof(color_bg), cc->bg),
                             swatch);
    }
  }

  static const int regex_lists[] = {
    MT_COLOR_ATTACH_HEADERS, MT_COLOR_BODY,
    MT_COLOR_HEADER,         MT_COLOR_INDEX,
    MT_COLOR_INDEX_AUTHOR,   MT_COLOR_INDEX_FLAGS,
    MT_COLOR_INDEX_SUBJECT,  MT_COLOR_INDEX_TAG,
    MT_COLOR_STATUS,         0,
  };

  int rl_count = 0;
  for (int i = 0; regex_lists[i]; i++)
  {
    struct RegexColorList *rcl = regex_colors_get_list(regex_lists[i]);
    if (!STAILQ_EMPTY(rcl))
      rl_count++;
  }

  if (rl_count > 0)
  {
    for (int i = 0; regex_lists[i]; i++)
    {
      struct RegexColorList *rcl = regex_colors_get_list(regex_lists[i]);
      if (STAILQ_EMPTY(rcl))
        continue;

      const char *name = mutt_map_get_name(regex_lists[i], ColorFields);
      if (!name)
        continue;

      mutt_buffer_add_printf(&filebuf, "\n# Regex Colour %s\n", name);

      struct RegexColor *rc = NULL;
      STAILQ_FOREACH(rc, rcl, entries)
      {
        struct AttrColor *ac = &rc->attr_color;
        struct CursesColor *cc = ac->curses_color;
        if (!cc)
          continue;

        const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
        mutt_buffer_add_printf(&filebuf, "color %-14s %-30s %-8s %-8s %-30s # %s\n",
                               name, color_debug_log_attrs_list(ac->attrs),
                               color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                               color_debug_log_name(color_bg, sizeof(color_bg), cc->bg),
                               rc->pattern, swatch);
      }
    }
  }

#ifdef USE_DEBUG_COLOR
  if (!TAILQ_EMPTY(&MergedColors))
  {
    mutt_buffer_addstr(&filebuf, "\n# Merged Colours\n");
    struct AttrColor *ac = NULL;
    TAILQ_FOREACH(ac, &MergedColors, entries)
    {
      struct CursesColor *cc = ac->curses_color;
      if (!cc)
        continue;

      const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
      mutt_buffer_add_printf(&filebuf, "# %-30s %-8s %-8s # %s\n",
                             color_debug_log_attrs_list(ac->attrs),
                             color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                             color_debug_log_name(color_bg, sizeof(color_bg), cc->bg),
                             swatch);
    }
  }

  struct MuttWindow *win = window_get_focus();
  if (win && (win->type == WT_CUSTOM) && win->parent && (win->parent->type == WT_PAGER))
  {
    struct PagerPrivateData *priv = win->parent->wdata;
    if (priv && !TAILQ_EMPTY(&priv->ansi_list))
    {
      mutt_buffer_addstr(&filebuf, "\n# Ansi Colours\n");
      struct AttrColor *ac = NULL;
      TAILQ_FOREACH(ac, &priv->ansi_list, entries)
      {
        struct CursesColor *cc = ac->curses_color;
        if (!cc)
          continue;

        const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
        mutt_buffer_add_printf(&filebuf, "# %-30s %-8s %-8s # %s\n",
                               color_debug_log_attrs_list(ac->attrs),
                               color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                               color_debug_log_name(color_bg, sizeof(color_bg), cc->bg),
                               swatch);
      }
    }
  }
#endif

  fputs(filebuf.data, fp_out);

  mutt_file_fclose(&fp_out);
  mutt_buffer_dealloc(&filebuf);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = tempfile;

  pview.banner = "color";
  pview.flags = MUTT_SHOWCOLOR;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  return MUTT_CMD_SUCCESS;
}
#endif

/**
 * icmd_set - Parse 'set' command to display config - Implements ICommand::parse()
 */
static enum CommandResult icmd_set(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  const bool set = mutt_str_startswith(s->data, "set");
  const bool set_all = mutt_str_startswith(s->data, "set all");

  if (!set && !set_all)
    return MUTT_CMD_ERROR;

  char tempfile[PATH_MAX];
  mutt_mktemp(tempfile, sizeof(tempfile));

  FILE *fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    return MUTT_CMD_ERROR;
  }

  if (set_all)
    dump_config(NeoMutt->sub->cs, CS_DUMP_NO_FLAGS, fp_out);
  else
    dump_config(NeoMutt->sub->cs, CS_DUMP_ONLY_CHANGED, fp_out);

  mutt_file_fclose(&fp_out);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = tempfile;

  pview.banner = "set";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);

  return MUTT_CMD_SUCCESS;
}

/**
 * icmd_version - Parse 'version' command - Implements ICommand::parse()
 */
static enum CommandResult icmd_version(struct Buffer *buf, struct Buffer *s,
                                       intptr_t data, struct Buffer *err)
{
  char tempfile[PATH_MAX];
  mutt_mktemp(tempfile, sizeof(tempfile));

  FILE *fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_buffer_printf(err, _("Could not create temporary file %s"), tempfile);
    return MUTT_CMD_ERROR;
  }

  print_version(fp_out);
  mutt_file_fclose(&fp_out);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = tempfile;

  pview.banner = "version";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  return MUTT_CMD_SUCCESS;
}
