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
#include "key/lib.h"

extern const struct MenuFuncOp OpAlias[];
extern const struct MenuFuncOp OpAttachment[];
#ifdef USE_AUTOCRYPT
extern const struct MenuFuncOp OpAutocrypt[];
#endif
extern const struct MenuFuncOp OpBrowser[];
extern const struct MenuFuncOp OpCompose[];
extern const struct MenuFuncOp OpDialog[];
extern const struct MenuFuncOp OpEditor[];
extern const struct MenuFuncOp OpGeneric[];
extern const struct MenuFuncOp OpIndex[];
#ifdef MIXMASTER
extern const struct MenuFuncOp OpMixmaster[];
#endif
extern const struct MenuFuncOp OpPager[];
extern const struct MenuFuncOp OpPgp[];
extern const struct MenuFuncOp OpPostponed[];
extern const struct MenuFuncOp OpQuery[];
extern const struct MenuFuncOp OpSmime[];

extern const struct MenuOpSeq AliasDefaultBindings[];
extern const struct MenuOpSeq AttachmentDefaultBindings[];
#ifdef USE_AUTOCRYPT
extern const struct MenuOpSeq AutocryptDefaultBindings[];
#endif
extern const struct MenuOpSeq BrowserDefaultBindings[];
extern const struct MenuOpSeq ComposeDefaultBindings[];
extern const struct MenuOpSeq DialogDefaultBindings[];
extern const struct MenuOpSeq EditorDefaultBindings[];
extern const struct MenuOpSeq GenericDefaultBindings[];
extern const struct MenuOpSeq IndexDefaultBindings[];
#ifdef MIXMASTER
extern const struct MenuOpSeq MixmasterDefaultBindings[];
#endif
extern const struct MenuOpSeq PagerDefaultBindings[];
extern const struct MenuOpSeq PgpDefaultBindings[];
extern const struct MenuOpSeq PostponedDefaultBindings[];
extern const struct MenuOpSeq QueryDefaultBindings[];
extern const struct MenuOpSeq SmimeDefaultBindings[];

#endif /* MUTT_FUNCTIONS_H */
