/**
 * @file
 * Definitions of user functions
 *
 * @authors
 * Copyright (C) 2021-2026 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page gui_functions Definitions of user functions
 *
 * Definitions of user functions
 *
 * This file contains the structures needed to parse "bind" commands, as well
 * as the default bindings for each menu.
 *
 * Notes:
 *
 * - If you need to bind a control char, use the octal value because the `\cX`
 *   construct does not work at this level.
 *
 * - The magic "map:" comments define how the map will be called in the manual.
 *   Lines starting with "**" will be included in the manual.
 *
 * - For "enter" bindings, add entries for "\n" and "\r" and "<keypadenter>".
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "module_data.h"
#include "opcodes.h"

// clang-format off
/**
 * OpGeneric - Functions for the Generic Menu
 */
static const struct MenuFuncOp OpGeneric[] = { /* map: generic */
  /*
  ** <para>
  ** The <emphasis>generic</emphasis> menu is not a real menu, but specifies common functions
  ** (such as movement) available in all menus except for <emphasis>pager</emphasis> and
  ** <emphasis>editor</emphasis>.  Changing settings for this menu will affect the default
  ** bindings for all menus (except as noted).
  ** </para>
  */
  { "activate-entry",                OP_ACTIVATE_ENTRY },
  { "apply-to-tagged",               OP_APPLY_TO_TAGGED },
  { "apply-to-tagged-begin",         OP_APPLY_TO_TAGGED_BEGIN },
  { "apply-to-tagged-end",           OP_APPLY_TO_TAGGED_END },
  { "check-stats",                   OP_CHECK_STATS },
  { "display-help",                  OP_DISPLAY_HELP },
  { "display-log",                   OP_DISPLAY_LOG },
  { "exit",                          OP_EXIT },
  { "fold-all-trees",                OP_FOLD_ALL_TREES },
  { "fold-tree",                     OP_FOLD_TREE },
  { "forget-passphrase",             OP_FORGET_PASSPHRASE },
  { "quit",                          OP_QUIT },
  { "redraw-screen",                 OP_REDRAW_SCREEN },
  { "run-command",                   OP_RUN_COMMAND },
  { "run-shell-command",             OP_RUN_SHELL_COMMAND },
  { "scroll-end",                    OP_SCROLL_END },
  { "scroll-half-down",              OP_SCROLL_HALF_DOWN },
  { "scroll-half-up",                OP_SCROLL_HALF_UP },
  { "scroll-home",                   OP_SCROLL_HOME },
  { "scroll-line-down",              OP_SCROLL_LINE_DOWN },
  { "scroll-line-up",                OP_SCROLL_LINE_UP },
  { "scroll-page-down",              OP_SCROLL_PAGE_DOWN },
  { "scroll-page-up",                OP_SCROLL_PAGE_UP },
  { "scroll-selection-to-bottom",    OP_SCROLL_SELECTION_TO_BOTTOM },
  { "scroll-selection-to-middle",    OP_SCROLL_SELECTION_TO_MIDDLE },
  { "scroll-selection-to-top",       OP_SCROLL_SELECTION_TO_TOP },
  { "search-backward",               OP_SEARCH_BACKWARD },
  { "search-forward",                OP_SEARCH_FORWARD },
  { "search-next",                   OP_SEARCH_NEXT },
  { "search-previous",               OP_SEARCH_PREVIOUS },
  { "select-entry-by-number",        OP_SELECT_ENTRY_BY_NUMBER },
  { "select-first-entry",            OP_SELECT_FIRST_ENTRY },
  { "select-last-entry",             OP_SELECT_LAST_ENTRY },
  { "select-next-entry",             OP_SELECT_NEXT_ENTRY },
  { "select-next-subtree",           OP_SELECT_NEXT_SUBTREE },
  { "select-next-tree",              OP_SELECT_NEXT_TREE },
  { "select-page-bottom",            OP_SELECT_PAGE_BOTTOM },
  { "select-page-middle",            OP_SELECT_PAGE_MIDDLE },
  { "select-page-top",               OP_SELECT_PAGE_TOP },
  { "select-previous-entry",         OP_SELECT_PREVIOUS_ENTRY },
  { "select-previous-subtree",       OP_SELECT_PREVIOUS_SUBTREE },
  { "select-previous-tree",          OP_SELECT_PREVIOUS_TREE },
  { "select-tree-parent-entry",      OP_SELECT_TREE_PARENT_ENTRY },
  { "select-tree-root-entry",        OP_SELECT_TREE_ROOT_ENTRY },
  { "show-version",                  OP_SHOW_VERSION },
  { "toggle-all-trees",              OP_TOGGLE_ALL_TREES },
  { "toggle-tag",                    OP_TOGGLE_TAG },
  { "toggle-tag-subtree",            OP_TOGGLE_TAG_SUBTREE },
  { "toggle-tag-tree",               OP_TOGGLE_TAG_TREE },
  { "toggle-tree",                   OP_TOGGLE_TREE },
  { "unfold-all-trees",              OP_UNFOLD_ALL_TREES },
  { "unfold-tree",                   OP_UNFOLD_TREE },
  { "view-keycodes",                 OP_VIEW_KEYCODES },

  // Deprecated
  { "bottom-page",                   OP_SELECT_PAGE_BOTTOM,                     MFF_DEPRECATED },
  { "current-bottom",                OP_SCROLL_SELECTION_TO_BOTTOM,             MFF_DEPRECATED },
  { "current-middle",                OP_SCROLL_SELECTION_TO_MIDDLE,             MFF_DEPRECATED },
  { "current-top",                   OP_SCROLL_SELECTION_TO_TOP,                MFF_DEPRECATED },
  { "end-cond",                      OP_APPLY_TO_TAGGED_END,                    MFF_DEPRECATED },
  { "enter-command",                 OP_RUN_COMMAND,                            MFF_DEPRECATED },
  { "error-history",                 OP_DISPLAY_LOG,                            MFF_DEPRECATED },
  { "first-entry",                   OP_SELECT_FIRST_ENTRY,                     MFF_DEPRECATED },
  { "half-down",                     OP_SCROLL_HALF_DOWN,                       MFF_DEPRECATED },
  { "half-up",                       OP_SCROLL_HALF_UP,                         MFF_DEPRECATED },
  { "help",                          OP_DISPLAY_HELP,                           MFF_DEPRECATED },
  { "jump",                          OP_SELECT_ENTRY_BY_NUMBER,                 MFF_DEPRECATED },
  { "last-entry",                    OP_SELECT_LAST_ENTRY,                      MFF_DEPRECATED },
  { "middle-page",                   OP_SELECT_PAGE_MIDDLE,                     MFF_DEPRECATED },
  { "next-entry",                    OP_SELECT_NEXT_ENTRY,                      MFF_DEPRECATED },
  { "next-line",                     OP_SCROLL_LINE_DOWN,                       MFF_DEPRECATED },
  { "next-page",                     OP_SCROLL_PAGE_DOWN,                       MFF_DEPRECATED },
  { "previous-entry",                OP_SELECT_PREVIOUS_ENTRY,                  MFF_DEPRECATED },
  { "previous-line",                 OP_SCROLL_LINE_UP,                         MFF_DEPRECATED },
  { "previous-page",                 OP_SCROLL_PAGE_UP,                         MFF_DEPRECATED },
  { "refresh",                       OP_REDRAW_SCREEN,                          MFF_DEPRECATED },
  { "search",                        OP_SEARCH_FORWARD,                         MFF_DEPRECATED },
  { "search-next",                   OP_SEARCH_NEXT,                            MFF_DEPRECATED },
  { "search-opposite",               OP_SEARCH_PREVIOUS,                        MFF_DEPRECATED },
  { "search-reverse",                OP_SEARCH_BACKWARD,                        MFF_DEPRECATED },
  { "select-entry",                  OP_ACTIVATE_ENTRY,                         MFF_DEPRECATED },
  { "shell-escape",                  OP_RUN_SHELL_COMMAND,                      MFF_DEPRECATED },
  { "show-log-messages",             OP_DISPLAY_LOG,                            MFF_DEPRECATED },
  { "tag-entry",                     OP_TOGGLE_TAG,                             MFF_DEPRECATED },
  { "tag-prefix",                    OP_APPLY_TO_TAGGED,                        MFF_DEPRECATED },
  { "tag-prefix-cond",               OP_APPLY_TO_TAGGED_BEGIN,                  MFF_DEPRECATED },
  { "top-page",                      OP_SELECT_PAGE_TOP,                        MFF_DEPRECATED },
  { "what-key",                      OP_VIEW_KEYCODES,                          MFF_DEPRECATED },
  { NULL, 0 },
};

/**
 * GenericDefaultBindings - Key bindings for the Generic Menu
 */
static const struct MenuOpSeq GenericDefaultBindings[] = { /* map: generic */
  { OP_ACTIVATE_ENTRY,                     "<keypadenter>" },
  { OP_ACTIVATE_ENTRY,                     "\n" },             // <Enter>
  { OP_ACTIVATE_ENTRY,                     "\r" },             // <Return>
  { OP_APPLY_TO_TAGGED,                    ";" },
  { OP_DISPLAY_HELP,                       "?" },
  { OP_QUIT,                               "q" },
  { OP_REDRAW_SCREEN,                      "\014" },           // <Ctrl-L>
  { OP_RUN_COMMAND,                        ":" },
  { OP_RUN_SHELL_COMMAND,                  "!" },
  { OP_SCROLL_HALF_DOWN,                   "]" },
  { OP_SCROLL_HALF_UP,                     "[" },
  { OP_SCROLL_LINE_DOWN,                   ">" },
  { OP_SCROLL_LINE_UP,                     "<" },
  { OP_SCROLL_PAGE_DOWN,                   "<pagedown>" },
  { OP_SCROLL_PAGE_DOWN,                   "<right>" },
  { OP_SCROLL_PAGE_DOWN,                   "z" },
  { OP_SCROLL_PAGE_UP,                     "<left>" },
  { OP_SCROLL_PAGE_UP,                     "<pageup>" },
  { OP_SCROLL_PAGE_UP,                     "Z" },
  { OP_SEARCH_BACKWARD,                    "\033/" },          // <Alt-/>
  { OP_SEARCH_FORWARD,                     "/" },
  { OP_SEARCH_NEXT,                        "n" },
  { OP_SELECT_FIRST_ENTRY,                 "<home>" },
  { OP_SELECT_FIRST_ENTRY,                 "=" },
  { OP_SELECT_LAST_ENTRY,                  "*" },
  { OP_SELECT_LAST_ENTRY,                  "<end>" },
  { OP_SELECT_NEXT_ENTRY,                  "<down>" },
  { OP_SELECT_NEXT_ENTRY,                  "j" },
  { OP_SELECT_PAGE_BOTTOM,                 "L" },
  { OP_SELECT_PAGE_MIDDLE,                 "M" },
  { OP_SELECT_PAGE_TOP,                    "H" },
  { OP_SELECT_PREVIOUS_ENTRY,              "<up>" },
  { OP_SELECT_PREVIOUS_ENTRY,              "k" },
  { OP_SHOW_VERSION,                       "V" },
  { OP_TOGGLE_TAG,                         "t" },
  { 0, NULL },
};
// clang-format on

/**
 * generic_init_keys - Initialise the Generic Keybindings
 */
struct SubMenu *generic_init_keys(struct NeoMutt *n)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_GUI);
  ASSERT(mod_data);

  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;

  sm = km_register_submenu(OpGeneric);
  md = km_register_menu(MENU_GENERIC, "generic");
  km_menu_add_submenu(md, sm);
  km_menu_add_bindings(md, GenericDefaultBindings);

  mod_data->md_generic = md;
  mod_data->sm_generic = sm;

  return sm;
}

/**
 * generic_get_menu_definition - Get the Generic Menu Definition
 * @retval ptr Generic Menu Definition
 */
struct MenuDefinition *generic_get_menu_definition(void)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  ASSERT(mod_data);

  return mod_data->md_generic;
}
