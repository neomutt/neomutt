/**
 * @file
 * Dump all the config
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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
 * @page config_dump Dump all the config
 *
 * Dump all the config items in various formats.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "dump.h"
#include "set.h"
#include "subset.h"
#include "types.h"

void mutt_pretty_mailbox(char *buf, size_t buflen);

/**
 * escape_string - Write a string to a buffer, escaping special characters
 * @param buf Buffer to write to
 * @param src String to write
 * @retval num Bytes written to buffer
 */
size_t escape_string(struct Buffer *buf, const char *src)
{
  if (!buf || !src)
    return 0;

  size_t len = 0;
  for (; *src; src++)
  {
    switch (*src)
    {
      case '\007':
        len += mutt_buffer_addstr(buf, "\\g");
        break;
      case '\n':
        len += mutt_buffer_addstr(buf, "\\n");
        break;
      case '\r':
        len += mutt_buffer_addstr(buf, "\\r");
        break;
      case '\t':
        len += mutt_buffer_addstr(buf, "\\t");
        break;
      default:
        if ((*src == '\\') || (*src == '"'))
          len += mutt_buffer_addch(buf, '\\');
        len += mutt_buffer_addch(buf, src[0]);
    }
  }
  return len;
}

/**
 * pretty_var - Escape and stringify a config item value
 * @param str    String to escape
 * @param buf    Buffer to write to
 * @retval num Number of bytes written to buffer
 */
size_t pretty_var(const char *str, struct Buffer *buf)
{
  if (!buf || !str)
    return 0;

  int len = 0;

  len += mutt_buffer_addch(buf, '"');
  len += escape_string(buf, str);
  len += mutt_buffer_addch(buf, '"');

  return len;
}

/**
 * dump_config_neo - Dump the config in the style of NeoMutt
 * @param cs      Config items
 * @param he      HashElem representing config item
 * @param value   Current value of the config item
 * @param initial Initial value of the config item
 * @param flags   Flags, see #ConfigDumpFlags
 * @param fp      File pointer to write to
 */
void dump_config_neo(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value,
                     struct Buffer *initial, ConfigDumpFlags flags, FILE *fp)
{
  if (!he || !value || !fp)
    return;

  const char *name = he->key.strkey;

  if ((flags & CS_DUMP_ONLY_CHANGED) &&
      (!initial || mutt_str_equal(value->data, initial->data)))
    return;

  if (he->type == DT_SYNONYM)
  {
    const struct ConfigDef *cdef = he->data;
    const char *syn = (const char *) cdef->initial;
    fprintf(fp, "# synonym: %s -> %s\n", name, syn);
    return;
  }

  bool show_name = !(flags & CS_DUMP_HIDE_NAME);
  bool show_value = !(flags & CS_DUMP_HIDE_VALUE);

  if (show_name && show_value)
    fprintf(fp, "set ");
  if (show_name)
    fprintf(fp, "%s", name);
  if (show_name && show_value)
    fprintf(fp, " = ");
  if (show_value)
    fprintf(fp, "%s", value->data);
  if (show_name || show_value)
    fprintf(fp, "\n");

  if (flags & CS_DUMP_SHOW_DEFAULTS)
  {
    const struct ConfigSetType *cst = cs_get_type_def(cs, he->type);
    if (cst)
      fprintf(fp, "# %s %s %s\n", cst->name, name, value->data);
  }
}

/**
 * dump_config - Write all the config to a file
 * @param cs    ConfigSet to dump
 * @param flags Flags, see #ConfigDumpFlags
 * @param fp    File to write config to
 */
bool dump_config(struct ConfigSet *cs, ConfigDumpFlags flags, FILE *fp)
{
  if (!cs)
    return false;

  struct HashElem *he = NULL;

  struct HashElem **list = get_elem_list(cs);
  if (!list)
    return false; /* LCOV_EXCL_LINE */

  bool result = true;

  struct Buffer value = mutt_buffer_make(256);
  struct Buffer initial = mutt_buffer_make(256);
  struct Buffer tmp = mutt_buffer_make(256);

  for (size_t i = 0; list[i]; i++)
  {
    mutt_buffer_reset(&value);
    mutt_buffer_reset(&initial);
    he = list[i];
    const int type = DTYPE(he->type);

    if ((type == DT_SYNONYM) && !(flags & CS_DUMP_SHOW_SYNONYMS))
      continue;

    if ((he->type & DT_DEPRECATED) && !(flags & CS_DUMP_SHOW_DEPRECATED))
      continue;

    // if ((type == DT_DISABLED) && !(flags & CS_DUMP_SHOW_DISABLED))
    //   continue;

    if (type != DT_SYNONYM)
    {
      /* If necessary, get the current value */
      if ((flags & CS_DUMP_ONLY_CHANGED) || !(flags & CS_DUMP_HIDE_VALUE) ||
          (flags & CS_DUMP_SHOW_DEFAULTS))
      {
        int rc = cs_he_string_get(cs, he, &value);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
        {
          result = false; /* LCOV_EXCL_LINE */
          break;          /* LCOV_EXCL_LINE */
        }

        const struct ConfigDef *cdef = he->data;
        if ((type == DT_STRING) && IS_SENSITIVE(*cdef) &&
            (flags & CS_DUMP_HIDE_SENSITIVE) && !mutt_buffer_is_empty(&value))
        {
          mutt_buffer_reset(&value);
          mutt_buffer_addstr(&value, "***");
        }

        if (((type == DT_PATH) || IS_MAILBOX(he)) && (value.data[0] == '/'))
          mutt_pretty_mailbox(value.data, value.dsize);

        if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) &&
            (type != DT_QUAD) && !(flags & CS_DUMP_NO_ESCAPING))
        {
          mutt_buffer_reset(&tmp);
          pretty_var(value.data, &tmp);
          mutt_buffer_strcpy(&value, tmp.data);
        }
      }

      /* If necessary, get the default value */
      if (flags & (CS_DUMP_ONLY_CHANGED | CS_DUMP_SHOW_DEFAULTS))
      {
        int rc = cs_he_initial_get(cs, he, &initial);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
        {
          result = false; /* LCOV_EXCL_LINE */
          break;          /* LCOV_EXCL_LINE */
        }

        if (((type == DT_PATH) || IS_MAILBOX(he)) && !(he->type & DT_MAILBOX))
          mutt_pretty_mailbox(initial.data, initial.dsize);

        if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) &&
            (type != DT_QUAD) && !(flags & CS_DUMP_NO_ESCAPING))
        {
          mutt_buffer_reset(&tmp);
          pretty_var(initial.data, &tmp);
          mutt_buffer_strcpy(&initial, tmp.data);
        }
      }
    }

    dump_config_neo(cs, he, &value, &initial, flags, fp);
  }

  FREE(&list);
  mutt_buffer_dealloc(&value);
  mutt_buffer_dealloc(&initial);
  mutt_buffer_dealloc(&tmp);

  return result;
}
