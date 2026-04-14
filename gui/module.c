/**
 * @file
 * Definition of the Gui Module
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page gui_module Definition of the Gui Module
 *
 * Definition of the Gui Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "color/lib.h"
#include "curs_lib.h"
#include "module_data.h"
#include "mutt_curses.h"
#include "mutt_logging.h"
#include "mutt_window.h"
#include "rootwin.h"
#include "terminal.h"

extern struct ConfigDef GuiVars[];

/**
 * log_gui - Log info about the GUI
 */
static void log_gui(void)
{
  const char *term = mutt_str_getenv("TERM");
  const char *color_term = mutt_str_getenv("COLORTERM");
  bool true_color = false;
#ifdef NEOMUTT_DIRECT_COLORS
  true_color = true;
#endif

  mutt_debug(LL_DEBUG1, "GUI:\n");
  mutt_debug(LL_DEBUG1, "    Curses: %s\n", curses_version());
  mutt_debug(LL_DEBUG1, "    COLORS=%d\n", COLORS);
  mutt_debug(LL_DEBUG1, "    COLOR_PAIRS=%d\n", COLOR_PAIRS);
  mutt_debug(LL_DEBUG1, "    TERM=%s\n", NONULL(term));
  mutt_debug(LL_DEBUG1, "    COLORTERM=%s\n", NONULL(color_term));
  mutt_debug(LL_DEBUG1, "    True color support: %s\n", true_color ? "YES" : "NO");
  mutt_debug(LL_DEBUG1, "    Screen: %dx%d\n", RootWindow->state.cols,
             RootWindow->state.rows);
}

/**
 * gui_init - Initialise a Module - Implements Module::init()
 */
static bool gui_init(struct NeoMutt *n)
{
  // struct GuiModuleData *md = MUTT_MEM_CALLOC(1, struct GuiModuleData);
  // neomutt_set_module_data(n, MODULE_ID_GUI, md);

  return true;
}

/**
 * gui_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool gui_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, GuiVars);
}

/**
 * gui_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool gui_gui_init(struct NeoMutt *n)
{
  /* check whether terminal status is supported (must follow curses init) */
  TsSupported = mutt_ts_capability();
  mutt_resize_screen();
  log_gui();

  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  clear();

  MuttLogger = log_disp_curses;
  log_queue_flush(log_disp_curses);
  log_queue_set_max_size(100);

  return true;
}

/**
 * gui_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void gui_gui_cleanup(struct NeoMutt *n)
{
  rootwin_cleanup();

  mutt_endwin();
}

/**
 * gui_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool gui_cleanup(struct NeoMutt *n)
{
  // struct GuiModuleData *md = neomutt_get_module_data(n, MODULE_ID_GUI);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * ModuleGui - Module for the Gui library
 */
const struct Module ModuleGui = {
  MODULE_ID_GUI,
  "gui",
  gui_init,
  NULL, // config_define_types
  gui_config_define_variables,
  NULL, // commands_register
  gui_gui_init,
  gui_gui_cleanup,
  gui_cleanup,
};
