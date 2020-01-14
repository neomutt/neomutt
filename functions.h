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

extern struct Binding OpAlias[];
extern struct Binding OpAttach[];
#ifdef USE_AUTOCRYPT
extern struct Binding OpAutocryptAcct[];
#endif
extern struct Binding OpBrowser[];
extern struct Binding OpCompose[];
extern struct Binding OpEditor[];
extern struct Binding OpGeneric[];
extern struct Binding OpMain[];
#ifdef MIXMASTER
extern struct Binding OpMix[];
#endif
extern struct Binding OpPager[];
extern struct Binding OpPgp[];
extern struct Binding OpPost[];
extern struct Binding OpQuery[];
extern struct Binding OpSmime[];

#endif /* MUTT_FUNCTIONS_H */
