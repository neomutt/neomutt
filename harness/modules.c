/**
 * @file
 * NeoMutt Modules
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page harness_modules NeoMutt Modules
 *
 * NeoMutt Modules
 */

#include "config.h"
#include <locale.h>
#include "common.h"

// clang-format off
extern const struct Module ModuleMain;
extern const struct Module ModuleAddress;  extern const struct Module ModuleAlias;    extern const struct Module ModuleAttach;   extern const struct Module ModuleAutocrypt;
extern const struct Module ModuleBcache;   extern const struct Module ModuleBrowser;  extern const struct Module ModuleColor;    extern const struct Module ModuleCommands;
extern const struct Module ModuleComplete; extern const struct Module ModuleCompmbox; extern const struct Module ModuleCompose;  extern const struct Module ModuleCompress;
extern const struct Module ModuleConfig;   extern const struct Module ModuleConn;     extern const struct Module ModuleConvert;  extern const struct Module ModuleCore;
extern const struct Module ModuleEditor;   extern const struct Module ModuleEmail;    extern const struct Module ModuleEnvelope; extern const struct Module ModuleExpando;
extern const struct Module ModuleGui;      extern const struct Module ModuleHcache;   extern const struct Module ModuleHelpbar;  extern const struct Module ModuleHistory;
extern const struct Module ModuleHooks;    extern const struct Module ModuleImap;     extern const struct Module ModuleIndex;    extern const struct Module ModuleKey;
extern const struct Module ModuleLua;      extern const struct Module ModuleMaildir;  extern const struct Module ModuleMbox;     extern const struct Module ModuleMenu;
extern const struct Module ModuleMh;       extern const struct Module ModuleMutt;     extern const struct Module ModuleNcrypt;   extern const struct Module ModuleNntp;
extern const struct Module ModuleNotmuch;  extern const struct Module ModulePager;    extern const struct Module ModuleParse;    extern const struct Module ModulePattern;
extern const struct Module ModulePop;      extern const struct Module ModulePostpone; extern const struct Module ModuleProgress; extern const struct Module ModuleQuestion;
extern const struct Module ModuleSend;     extern const struct Module ModuleSidebar;  extern const struct Module ModuleStore;
// clang-format on

/**
 * Modules - All the library Modules
 */
const struct Module *Modules[] = {
  // clang-format off
  &ModuleMain,     &ModuleGui,
  &ModuleAddress,  &ModuleAlias,    &ModuleAttach,   &ModuleBcache,   &ModuleBrowser,
  &ModuleColor,    &ModuleCommands, &ModuleComplete, &ModuleCompmbox, &ModuleCompose,
  &ModuleConfig,   &ModuleConn,     &ModuleConvert,  &ModuleCore,     &ModuleEditor,
  &ModuleEmail,    &ModuleEnvelope, &ModuleExpando,  &ModuleHelpbar,  &ModuleHistory,
  &ModuleHooks,    &ModuleImap,     &ModuleIndex,    &ModuleKey,      &ModuleMaildir,
  &ModuleMbox,     &ModuleMenu,     &ModuleMh,       &ModuleMutt,     &ModuleNcrypt,
  &ModuleNntp,     &ModulePager,    &ModuleParse,    &ModulePattern,  &ModulePop,
  &ModulePostpone, &ModuleProgress, &ModuleQuestion, &ModuleSend,     &ModuleSidebar,
// clang-format on
#ifdef USE_AUTOCRYPT
  &ModuleAutocrypt,
#endif
#ifdef USE_HCACHE_COMPRESSION
  &ModuleCompress,
#endif
#ifdef USE_HCACHE
  &ModuleHcache,
#endif
#ifdef USE_LUA
  &ModuleLua,
#endif
#ifdef USE_NOTMUCH
  &ModuleNotmuch,
#endif
#ifdef USE_HCACHE
  &ModuleStore,
#endif
  NULL,
};
