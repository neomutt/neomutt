/**
 * @file
 * Email private Module data
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

#ifndef MUTT_EMAIL_MODULE_DATA_H
#define MUTT_EMAIL_MODULE_DATA_H

#include "mutt/lib.h"

/**
 * struct EmailModuleData - Email private Module data
 */
struct EmailModuleData
{
  struct ListHead    alternative_order;     ///< List of preferred mime types to display
  struct HashTable  *auto_subscribe_cache;  ///< Hash Table: "mailto:" (no value)
  struct ListHead    auto_view;             ///< List of mime types to auto view
  struct ListHead    header_order;          ///< List of header fields in the order they should be displayed
  struct ListHead    ignore;                ///< Header patterns to ignore
  struct RegexList   mail;                  ///< Regexes to match mailing lists
  struct ListHead    mail_to_allow;         ///< Permitted fields in a mailto: url
  struct RegexList   no_spam;               ///< Regexes to identify non-spam emails
  struct ReplaceList spam;                  ///< Regexes and patterns to match spam emails
  struct RegexList   subscribed;            ///< Regexes to match subscribed mailing lists
  struct HashTable  *tag_formats;           ///< Hash Table: "inbox" -> "GI" - Tag format strings
  struct HashTable  *tag_transforms;        ///< Hash Table: "inbox" -> "i" - Alternative tag names
  struct ListHead    unignore;              ///< Header patterns to unignore
  struct RegexList   unmail;                ///< Regexes to exclude false matches in mail
  struct RegexList   unsubscribed;          ///< Regexes to exclude false matches in subscribed
};

#endif /* MUTT_EMAIL_MODULE_DATA_H */
