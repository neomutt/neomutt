/**
 * @file
 * Theme loading and management
 *
 * @authors
 * Copyright (C) 2026 Pedro Schreiber
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
 * @page color_theme Theme loading and management
 *
 * Theme loading and management
 *
 * A theme is a directory containing up to three files:
 * - palette.neomuttrc  - raw color definitions as my_ variables
 * - spec.neomuttrc     - semantic role mappings
 * - groups.neomuttrc   - color commands for all UI objects
 *
 * The `theme` command resets all colors, then sources the theme's
 * groups.neomuttrc (which chains to spec and palette via `source`).
 */

#include "config.h"
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "commands/source.h"
#include "muttlib.h"
#include "color.h"
#include "theme.h"

/// Name of the currently loaded theme
static char *CurrentTheme = NULL;

/**
 * theme_dir_exists - Check if a directory exists
 * @param path Directory path
 * @retval true  Directory exists
 * @retval false Directory doesn't exist
 */
static bool theme_dir_exists(const char *path)
{
  struct stat st = { 0 };
  return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
}

/**
 * theme_scan_dir - Scan a directory for theme subdirectories
 * @param path    Directory to scan
 * @param source  Label for this search path (e.g. "user", "system")
 * @param buf     Buffer to append results to
 *
 * A valid theme is a subdirectory containing groups.neomuttrc.
 */
static void theme_scan_dir(const char *path, const char *source, struct Buffer *buf)
{
  DIR *dirp = opendir(path);
  if (!dirp)
    return;

  struct Buffer *groups_path = buf_pool_get();
  struct dirent *de = NULL;

  while ((de = readdir(dirp)))
  {
    if (de->d_name[0] == '.')
      continue;

    buf_concat_path(groups_path, path, de->d_name);

    if (!theme_dir_exists(buf_string(groups_path)))
      continue;

    // Check that groups.neomuttrc exists inside
    struct Buffer *gfile = buf_pool_get();
    buf_concat_path(gfile, buf_string(groups_path), "groups.neomuttrc");

    struct stat st = { 0 };
    if (stat(buf_string(gfile), &st) == 0)
    {
      bool is_current = CurrentTheme && mutt_str_equal(de->d_name, CurrentTheme);
      buf_add_printf(buf, "  %s %-20s  (%s)\n",
                     is_current ? "*" : " ", de->d_name, source);
    }
    buf_pool_release(&gfile);
  }

  buf_pool_release(&groups_path);
  closedir(dirp);
}

/**
 * theme_list - List all available themes from all search paths
 */
static void theme_list(void)
{
  struct Buffer *buf = buf_pool_get();
  struct Buffer *dir = buf_pool_get();

  buf_addstr(buf, "Available themes (* = current):\n\n");

  // 1. $theme_dir
  const char *theme_dir = cs_subset_path(NeoMutt->sub, "theme_dir");
  if (theme_dir && (theme_dir[0] != '\0'))
  {
    buf_add_printf(buf, "[%s]\n", theme_dir);
    theme_scan_dir(theme_dir, "theme_dir", buf);
    buf_addstr(buf, "\n");
  }

  // 2. ~/.config/neomutt/themes/
  buf_strcpy(dir, "~/.config/neomutt/themes");
  expand_path(dir, false);
  if (theme_dir_exists(buf_string(dir)))
  {
    buf_add_printf(buf, "[%s]\n", buf_string(dir));
    theme_scan_dir(buf_string(dir), "user", buf);
    buf_addstr(buf, "\n");
  }

  // 3. PKGDATADIR/themes/
  buf_printf(dir, "%s/themes", PKGDATADIR);
  if (theme_dir_exists(buf_string(dir)))
  {
    buf_add_printf(buf, "[%s]\n", buf_string(dir));
    theme_scan_dir(buf_string(dir), "system", buf);
    buf_addstr(buf, "\n");
  }

  // Write to temp file and display in pager
  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  FILE *fp = mutt_file_fopen(buf_string(tempfile), "w");
  if (fp)
  {
    mutt_file_save_str(fp, buf_string(buf));
    mutt_file_fclose(&fp);

    struct PagerData pdata = { 0 };
    struct PagerView pview = { &pdata };

    pdata.fname = buf_string(tempfile);
    pview.banner = "theme list";
    pview.flags = MUTT_SHOWFLAT;
    pview.mode = PAGER_MODE_OTHER;

    mutt_do_pager(&pview, NULL);
  }

  buf_pool_release(&tempfile);
  buf_pool_release(&dir);
  buf_pool_release(&buf);
}

/**
 * theme_find - Search for a theme directory in the search path
 * @param name Theme name
 * @param path Buffer for the result path to groups.neomuttrc
 * @retval true  Theme found
 * @retval false Theme not found
 *
 * Search order:
 *   1. `$theme_dir/<name>/`
 *   2. `~/.config/neomutt/themes/<name>/`
 *   3. `PKGDATADIR/themes/<name>/`
 */
static bool theme_find(const char *name, struct Buffer *path)
{
  struct Buffer *dir = buf_pool_get();

  // 1. Check $theme_dir if set
  const char *theme_dir = cs_subset_path(NeoMutt->sub, "theme_dir");
  if (theme_dir && (theme_dir[0] != '\0'))
  {
    buf_concat_path(dir, theme_dir, name);
    if (theme_dir_exists(buf_string(dir)))
    {
      buf_concat_path(path, buf_string(dir), "groups.neomuttrc");
      buf_pool_release(&dir);
      return true;
    }
  }

  // 2. Check ~/.config/neomutt/themes/
  buf_strcpy(dir, "~/.config/neomutt/themes/");
  buf_addstr(dir, name);
  expand_path(dir, false);
  if (theme_dir_exists(buf_string(dir)))
  {
    buf_concat_path(path, buf_string(dir), "groups.neomuttrc");
    buf_pool_release(&dir);
    return true;
  }

  // 3. Check PKGDATADIR/themes/
  buf_printf(dir, "%s/themes/%s", PKGDATADIR, name);
  if (theme_dir_exists(buf_string(dir)))
  {
    buf_concat_path(path, buf_string(dir), "groups.neomuttrc");
    buf_pool_release(&dir);
    return true;
  }

  buf_pool_release(&dir);
  return false;
}

/**
 * parse_theme - Parse the 'theme' command - Implements Command::parse() - @ingroup command_parse
 *
 * Usage:
 * - `theme <name>`   - Load a named theme (reset all colors first)
 * - `theme`          - Display the current theme name
 */
enum CommandResult parse_theme(const struct Command *cmd, struct Buffer *line,
                               const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  // No arguments: display current theme
  if (!MoreArgs(line))
  {
    if (CurrentTheme)
      mutt_message(_("Current theme: %s"), CurrentTheme);
    else
      mutt_message(_("No theme loaded"));
    buf_pool_release(&token);
    return MUTT_CMD_SUCCESS;
  }

  // Extract theme name or subcommand
  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  const char *name = buf_string(token);

  // Handle 'theme list'
  if (mutt_str_equal(name, "list"))
  {
    theme_list();
    buf_pool_release(&token);
    return MUTT_CMD_SUCCESS;
  }

  // Find the theme directory
  struct Buffer *theme_path = buf_pool_get();
  if (!theme_find(name, theme_path))
  {
    buf_printf(err, _("Theme '%s' not found"), name);
    buf_pool_release(&token);
    buf_pool_release(&theme_path);
    return MUTT_CMD_ERROR;
  }

  // Reset all colors before loading the new theme
  colors_reset();

  // Source the theme's groups.neomuttrc
  int r = source_rc(buf_string(theme_path), (struct ParseContext *) pc, pe);
  if (r != 0)
  {
    buf_printf(err, _("Error loading theme '%s'"), name);
    rc = MUTT_CMD_ERROR;
  }
  else
  {
    // Store the current theme name
    mutt_str_replace(&CurrentTheme, name);
    rc = MUTT_CMD_SUCCESS;
  }

  buf_pool_release(&token);
  buf_pool_release(&theme_path);
  return rc;
}
