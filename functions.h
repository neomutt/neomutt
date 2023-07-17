/**
 * @file
 * Definitions of user functions
 *
 * @authors
 * Copyright (C) 1996-2000,2002 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_FUNCTIONS_H
#define MUTT_FUNCTIONS_H

#include "config.h"
#include "keymap.h"

extern struct MenuFuncOp OpAlias[];
extern struct MenuFuncOp OpAttach[];
#ifdef USE_AUTOCRYPT
extern struct MenuFuncOp OpAutocrypt[];
#endif
extern struct MenuFuncOp OpBrowser[];
extern struct MenuFuncOp OpCompose[];
extern struct MenuFuncOp OpDialog[];
extern struct MenuFuncOp OpEditor[];
extern struct MenuFuncOp OpGeneric[];
extern struct MenuFuncOp OpIndex[];
#ifdef MIXMASTER
extern struct MenuFuncOp OpMix[];
#endif
extern struct MenuFuncOp OpPager[];
extern struct MenuFuncOp OpPgp[];
extern struct MenuFuncOp OpPostpone[];
extern struct MenuFuncOp OpQuery[];
extern struct MenuFuncOp OpSmime[];

extern const struct MenuOpSeq AliasDefaultBindings[];
extern const struct MenuOpSeq AttachDefaultBindings[];
#ifdef USE_AUTOCRYPT
extern const struct MenuOpSeq AutocryptAcctDefaultBindings[];
#endif
extern const struct MenuOpSeq BrowserDefaultBindings[];
extern const struct MenuOpSeq ComposeDefaultBindings[];
extern const struct MenuOpSeq DialogDefaultBindings[];
extern const struct MenuOpSeq EditorDefaultBindings[];
extern const struct MenuOpSeq GenericDefaultBindings[];
extern const struct MenuOpSeq IndexDefaultBindings[];
#ifdef MIXMASTER
extern const struct MenuOpSeq MixDefaultBindings[];
#endif
extern const struct MenuOpSeq PagerDefaultBindings[];
extern const struct MenuOpSeq PgpDefaultBindings[];
extern const struct MenuOpSeq PostDefaultBindings[];
extern const struct MenuOpSeq QueryDefaultBindings[];
extern const struct MenuOpSeq SmimeDefaultBindings[];

#endif /* MUTT_FUNCTIONS_H */
