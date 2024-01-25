/**
 * @file
 * Definitions of user functions
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#include <stddef.h>
#include "key/lib.h"
#include "opcodes.h"
#endif
#endif

// clang-format off
/**
 * OpDialog - Functions for Simple Dialogs
 */
const struct MenuFuncOp OpDialog[] = {
  { "exit",                          OP_EXIT },
  { NULL, 0 },
};

/**
 * OpGeneric - Functions for the Generic Menu
 */
const struct MenuFuncOp OpGeneric[] = { /* map: generic */
  /*
  ** <para>
  ** The <emphasis>generic</emphasis> menu is not a real menu, but specifies common functions
  ** (such as movement) available in all menus except for <emphasis>pager</emphasis> and
  ** <emphasis>editor</emphasis>.  Changing settings for this menu will affect the default
  ** bindings for all menus (except as noted).
  ** </para>
  */
  { "bottom-page",                   OP_BOTTOM_PAGE },
  { "check-stats",                   OP_CHECK_STATS },
  { "current-bottom",                OP_CURRENT_BOTTOM },
  { "current-middle",                OP_CURRENT_MIDDLE },
  { "current-top",                   OP_CURRENT_TOP },
  { "end-cond",                      OP_END_COND },
  { "enter-command",                 OP_ENTER_COMMAND },
  { "exit",                          OP_EXIT },
  { "first-entry",                   OP_FIRST_ENTRY },
  { "half-down",                     OP_HALF_DOWN },
  { "half-up",                       OP_HALF_UP },
  { "help",                          OP_HELP },
  { "jump",                          OP_JUMP },
  { "jump",                          OP_JUMP_1 },
  { "jump",                          OP_JUMP_2 },
  { "jump",                          OP_JUMP_3 },
  { "jump",                          OP_JUMP_4 },
  { "jump",                          OP_JUMP_5 },
  { "jump",                          OP_JUMP_6 },
  { "jump",                          OP_JUMP_7 },
  { "jump",                          OP_JUMP_8 },
  { "jump",                          OP_JUMP_9 },
  { "last-entry",                    OP_LAST_ENTRY },
  { "middle-page",                   OP_MIDDLE_PAGE },
  { "next-entry",                    OP_NEXT_ENTRY },
  { "next-line",                     OP_NEXT_LINE },
  { "next-page",                     OP_NEXT_PAGE },
  { "previous-entry",                OP_PREV_ENTRY },
  { "previous-line",                 OP_PREV_LINE },
  { "previous-page",                 OP_PREV_PAGE },
  { "redraw-screen",                 OP_REDRAW },
  { "search",                        OP_SEARCH },
  { "search-next",                   OP_SEARCH_NEXT },
  { "search-opposite",               OP_SEARCH_OPPOSITE },
  { "search-reverse",                OP_SEARCH_REVERSE },
  { "select-entry",                  OP_GENERIC_SELECT_ENTRY },
  { "shell-escape",                  OP_SHELL_ESCAPE },
  { "show-log-messages",             OP_SHOW_LOG_MESSAGES },
  { "show-version",                  OP_VERSION },
  { "tag-entry",                     OP_TAG },
  { "tag-prefix",                    OP_TAG_PREFIX },
  { "tag-prefix-cond",               OP_TAG_PREFIX_COND },
  { "top-page",                      OP_TOP_PAGE },
  { "what-key",                      OP_WHAT_KEY },
  // Deprecated
  { "error-history",                 OP_SHOW_LOG_MESSAGES },
  { "refresh",                       OP_REDRAW },
  { NULL, 0 },
};

/**
 * DialogDefaultBindings - Key bindings for Simple Dialogs
 */
const struct MenuOpSeq DialogDefaultBindings[] = {
  { OP_QUIT,                               "q" },
  { 0, NULL },
};

/**
 * GenericDefaultBindings - Key bindings for the Generic Menu
 */
const struct MenuOpSeq GenericDefaultBindings[] = { /* map: generic */
  { OP_BOTTOM_PAGE,                        "L" },
  { OP_ENTER_COMMAND,                      ":" },
  { OP_FIRST_ENTRY,                        "<home>" },
  { OP_FIRST_ENTRY,                        "=" },
  { OP_GENERIC_SELECT_ENTRY,               "<keypadenter>" },
  { OP_GENERIC_SELECT_ENTRY,               "\n" },             // <Enter>
  { OP_GENERIC_SELECT_ENTRY,               "\r" },             // <Return>
  { OP_HALF_DOWN,                          "]" },
  { OP_HALF_UP,                            "[" },
  { OP_HELP,                               "?" },
  { OP_JUMP_1,                             "1" },
  { OP_JUMP_2,                             "2" },
  { OP_JUMP_3,                             "3" },
  { OP_JUMP_4,                             "4" },
  { OP_JUMP_5,                             "5" },
  { OP_JUMP_6,                             "6" },
  { OP_JUMP_7,                             "7" },
  { OP_JUMP_8,                             "8" },
  { OP_JUMP_9,                             "9" },
  { OP_LAST_ENTRY,                         "*" },
  { OP_LAST_ENTRY,                         "<end>" },
  { OP_MIDDLE_PAGE,                        "M" },
  { OP_NEXT_ENTRY,                         "<down>" },
  { OP_NEXT_ENTRY,                         "j" },
  { OP_NEXT_LINE,                          ">" },
  { OP_NEXT_PAGE,                          "<pagedown>" },
  { OP_NEXT_PAGE,                          "<right>" },
  { OP_NEXT_PAGE,                          "z" },
  { OP_PREV_ENTRY,                         "<up>" },
  { OP_PREV_ENTRY,                         "k" },
  { OP_PREV_LINE,                          "<" },
  { OP_PREV_PAGE,                          "<left>" },
  { OP_PREV_PAGE,                          "<pageup>" },
  { OP_PREV_PAGE,                          "Z" },
  { OP_REDRAW,                             "\014" },           // <Ctrl-L>
  { OP_SEARCH,                             "/" },
  { OP_SEARCH_NEXT,                        "n" },
  { OP_SEARCH_REVERSE,                     "\033/" },          // <Alt-/>
  { OP_SHELL_ESCAPE,                       "!" },
  { OP_TAG,                                "t" },
  { OP_TAG_PREFIX,                         ";" },
  { OP_TOP_PAGE,                           "H" },
  { OP_VERSION,                            "V" },
  { 0, NULL },
};
// clang-format on
