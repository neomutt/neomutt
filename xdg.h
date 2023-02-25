/**
 * @file
 * XDG Base Directory Specification handling
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

#ifndef MUTT_XDG_H
#define MUTT_XDG_H

struct Buffer;

/**
 * enum XdgType - XDG variables
 *
 * Constants for the environment variables defined by the XDG Base Directory
 * Specification.
 */
enum XdgEnvVar
{
  XDG_DATA_HOME, ///< XDG data dir; usually ~/.local/share
  XDG_CONFIG_HOME, ///< XDG config dir; usually ~/.config
  XDG_STATE_HOME, ///< XDG state dir; usually ~/.local/state
/* Not officially named yet
  XDG_BIN_HOME, ///< XDG dir for executables; usually ~/.local/bin
*/
  XDG_DATA_DIRS, ///< additional XDG data dirs; usually /usr/local/share/:/usr/share/
  XDG_CONFIG_DIRS, ///< additional XDG config dirs; usually /etc/xdg
  XDG_CACHE_HOME, ///< XDG cache dir; usually ~/.cache
/* Has no default value
  XDG_RUNTIME_DIR, ///< XDG runtime dir
*/
};

extern const char *const XdgEnvVarNames[];

int mutt_xdg_get_path(enum XdgEnvVar type, struct Buffer *buf);
int mutt_xdg_get_app_path(enum XdgEnvVar type, struct Buffer *buf);
int mutt_xdg_get_first_existing_path(enum XdgEnvVar type, const char *const path, struct Buffer *buf);


#endif /* MUTT_XDG_H */
