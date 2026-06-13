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
  { "check-stats",                   OP_CHECK_STATS },
  { "end-cond",                      OP_END_COND },
  { "enter-command",                 OP_ENTER_COMMAND },
  { "exit",                          OP_EXIT },
  { "help",                          OP_HELP },
  { "jump",                          OP_JUMP },
  { "quit",                          OP_QUIT },
  { "redraw-screen",                 OP_REDRAW },
  { "search",                        OP_SEARCH },
  { "search-next",                   OP_SEARCH_NEXT },
  { "search-previous",               OP_SEARCH_PREVIOUS },
  { "search-reverse",                OP_SEARCH_REVERSE },
  { "select-bottom-of-page",         OP_SELECT_BOTTOM_OF_PAGE },
  { "select-entry",                  OP_GENERIC_SELECT_ENTRY },
  { "select-first-entry",            OP_SELECT_FIRST_ENTRY },
  { "select-last-entry",             OP_SELECT_LAST_ENTRY },
  { "select-middle-of-page",         OP_SELECT_MIDDLE_OF_PAGE },
  { "select-next-entry",             OP_SELECT_NEXT_ENTRY },
  { "select-previous-entry",         OP_SELECT_PREVIOUS_ENTRY },
  { "select-top-of-page",            OP_SELECT_TOP_OF_PAGE },
  { "shell-escape",                  OP_SHELL_ESCAPE },
  { "show-log-messages",             OP_SHOW_LOG_MESSAGES },
  { "show-version",                  OP_VERSION },
  { "tag-entry",                     OP_TAG },
  { "tag-prefix",                    OP_TAG_PREFIX },
  { "tag-prefix-cond",               OP_TAG_PREFIX_COND },
  { "view-half-down",                OP_VIEW_HALF_DOWN },
  { "view-half-up",                  OP_VIEW_HALF_UP },
  { "view-next-line",                OP_VIEW_NEXT_LINE },
  { "view-next-page",                OP_VIEW_NEXT_PAGE },
  { "view-previous-line",            OP_VIEW_PREVIOUS_LINE },
  { "view-previous-page",            OP_VIEW_PREVIOUS_PAGE },
  { "view-selection-to-bottom",      OP_VIEW_SELECTION_TO_BOTTOM },
  { "view-selection-to-middle",      OP_VIEW_SELECTION_TO_MIDDLE },
  { "view-selection-to-top",         OP_VIEW_SELECTION_TO_TOP },
  { "what-key",                      OP_WHAT_KEY },

  // Deprecated
  { "bottom-page",                   OP_SELECT_BOTTOM_OF_PAGE,    MFF_DEPRECATED },
  { "current-bottom",                OP_VIEW_SELECTION_TO_BOTTOM, MFF_DEPRECATED },
  { "current-middle",                OP_VIEW_SELECTION_TO_MIDDLE, MFF_DEPRECATED },
  { "current-top",                   OP_VIEW_SELECTION_TO_TOP,    MFF_DEPRECATED },
  { "error-history",                 OP_SHOW_LOG_MESSAGES,        MFF_DEPRECATED },
  { "first-entry",                   OP_SELECT_FIRST_ENTRY,       MFF_DEPRECATED },
  { "half-down",                     OP_VIEW_HALF_DOWN,           MFF_DEPRECATED },
  { "half-up",                       OP_VIEW_HALF_UP,             MFF_DEPRECATED },
  { "last-entry",                    OP_SELECT_LAST_ENTRY,        MFF_DEPRECATED },
  { "middle-page",                   OP_SELECT_MIDDLE_OF_PAGE,    MFF_DEPRECATED },
  { "next-entry",                    OP_SELECT_NEXT_ENTRY,        MFF_DEPRECATED },
  { "next-line",                     OP_VIEW_NEXT_LINE,           MFF_DEPRECATED },
  { "next-page",                     OP_VIEW_NEXT_PAGE,           MFF_DEPRECATED },
  { "previous-entry",                OP_SELECT_PREVIOUS_ENTRY,    MFF_DEPRECATED },
  { "previous-line",                 OP_VIEW_PREVIOUS_LINE,       MFF_DEPRECATED },
  { "previous-page",                 OP_VIEW_PREVIOUS_PAGE,       MFF_DEPRECATED },
  { "refresh",                       OP_REDRAW,                   MFF_DEPRECATED },
  { "search-opposite",               OP_SEARCH_PREVIOUS,          MFF_DEPRECATED },
  { "top-page",                      OP_SELECT_TOP_OF_PAGE,       MFF_DEPRECATED },
  { NULL, 0 },
};

/**
 * GenericDefaultBindings - Key bindings for the Generic Menu
 */
static const struct MenuOpSeq GenericDefaultBindings[] = { /* map: generic */
  { OP_SELECT_BOTTOM_OF_PAGE,                        "L" },
  { OP_ENTER_COMMAND,                      ":" },
  { OP_SELECT_FIRST_ENTRY,                        "<home>" },
  { OP_SELECT_FIRST_ENTRY,                        "=" },
  { OP_GENERIC_SELECT_ENTRY,               "<keypadenter>" },
  { OP_GENERIC_SELECT_ENTRY,               "\n" },             // <Enter>
  { OP_GENERIC_SELECT_ENTRY,               "\r" },             // <Return>
  { OP_VIEW_HALF_DOWN,                     "]" },
  { OP_VIEW_HALF_UP,                       "[" },
  { OP_HELP,                               "?" },
  { OP_SELECT_LAST_ENTRY,                         "*" },
  { OP_SELECT_LAST_ENTRY,                         "<end>" },
  { OP_SELECT_MIDDLE_OF_PAGE,                        "M" },
  { OP_SELECT_NEXT_ENTRY,                         "<down>" },
  { OP_SELECT_NEXT_ENTRY,                         "j" },
  { OP_VIEW_NEXT_LINE,                          ">" },
  { OP_VIEW_NEXT_PAGE,                          "<pagedown>" },
  { OP_VIEW_NEXT_PAGE,                          "<right>" },
  { OP_VIEW_NEXT_PAGE,                          "z" },
  { OP_SELECT_PREVIOUS_ENTRY,                         "<up>" },
  { OP_SELECT_PREVIOUS_ENTRY,                         "k" },
  { OP_VIEW_PREVIOUS_LINE,                          "<" },
  { OP_VIEW_PREVIOUS_PAGE,                          "<left>" },
  { OP_VIEW_PREVIOUS_PAGE,                          "<pageup>" },
  { OP_VIEW_PREVIOUS_PAGE,                          "Z" },
  { OP_QUIT,                               "q" },
  { OP_REDRAW,                             "\014" },           // <Ctrl-L>
  { OP_SEARCH,                             "/" },
  { OP_SEARCH_NEXT,                        "n" },
  { OP_SEARCH_REVERSE,                     "\033/" },          // <Alt-/>
  { OP_SHELL_ESCAPE,                       "!" },
  { OP_TAG,                                "t" },
  { OP_TAG_PREFIX,                         ";" },
  { OP_SELECT_TOP_OF_PAGE,                           "H" },
  { OP_VERSION,                            "V" },
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
