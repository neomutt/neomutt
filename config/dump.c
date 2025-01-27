/**
 * @file
 * Dump all the config
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "dump.h"
#include "color/lib.h"
#include "pfile/lib.h"
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
        len += buf_addstr(buf, "\\g");
        break;
      case '\n':
        len += buf_addstr(buf, "\\n");
        break;
      case '\r':
        len += buf_addstr(buf, "\\r");
        break;
      case '\t':
        len += buf_addstr(buf, "\\t");
        break;
      default:
        if ((*src == '\\') || (*src == '"'))
          len += buf_addch(buf, '\\');
        len += buf_addch(buf, src[0]);
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

  len += buf_addch(buf, '"');
  len += escape_string(buf, str);
  len += buf_addch(buf, '"');

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
  {
    return;
  }

  if (he->type == DT_SYNONYM)
  {
    const struct ConfigDef *cdef = he->data;
    const char *syn = (const char *) cdef->initial;
    fprintf(fp, "# synonym: %s -> %s\n", name, syn);
    return;
  }

  if (flags & CS_DUMP_SHOW_DOCS)
  {
    const struct ConfigDef *cdef = he->data;
    fprintf(fp, "# %s\n", cdef->docs);
  }

  bool show_name = !(flags & CS_DUMP_HIDE_NAME);
  bool show_value = !(flags & CS_DUMP_HIDE_VALUE);

  if (show_name && show_value)
    fprintf(fp, "set ");
  if (show_name)
  {
    if (flags & CS_DUMP_LINK_DOCS)
    {
      // Used to generate unique ids for the urls
      static int seq_num = 1;

      if (CONFIG_TYPE(he->type) == DT_MYVAR)
      {
        static const char *url = "https://neomutt.org/guide/configuration#set-myvar";
        fprintf(fp, "\033]8;id=%d;%s\a%s\033]8;;\a", seq_num++, url, name);
      }
      else
      {
        char *fragment = mutt_str_dup(name);
        for (char *underscore = fragment; (underscore = strchr(underscore, '_')); underscore++)
        {
          *underscore = '-';
        }

        static const char *url = "https://neomutt.org/guide/reference";
        fprintf(fp, "\033]8;id=%d;%s#%s\a%s\033]8;;\a", seq_num++, url, fragment, name);
        FREE(&fragment);
      }
    }
    else
    {
      fprintf(fp, "%s", name);
    }
  }
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

  if (flags & CS_DUMP_SHOW_DOCS)
    fprintf(fp, "\n");
}

/**
 * dump_config - Write all the config to a file
 * @param cs    ConfigSet to dump
 * @param hea   Array of Config HashElem to dump
 * @param flags Flags, see #ConfigDumpFlags
 * @param fp    File to write config to
 */
bool dump_config(struct ConfigSet *cs, struct HashElemArray *hea,
                 ConfigDumpFlags flags, FILE *fp)
{
  if (!cs || !hea || !fp)
    return false;

  bool result = true;

  struct Buffer *value = buf_pool_get();
  struct Buffer *initial = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  struct HashElem **hep = NULL;
  ARRAY_FOREACH(hep, hea)
  {
    struct HashElem *he = *hep;
    buf_reset(value);
    buf_reset(initial);
    const int type = CONFIG_TYPE(he->type);

    /* If necessary, get the current value */
    if ((flags & CS_DUMP_ONLY_CHANGED) || !(flags & CS_DUMP_HIDE_VALUE) ||
        (flags & CS_DUMP_SHOW_DEFAULTS))
    {
      int rc = cs_he_string_get(cs, he, value);
      if (CSR_RESULT(rc) != CSR_SUCCESS)
      {
        result = false; /* LCOV_EXCL_LINE */
        break;          /* LCOV_EXCL_LINE */
      }

      const struct ConfigDef *cdef = he->data;
      if ((type == DT_STRING) && (cdef->type & D_SENSITIVE) &&
          (flags & CS_DUMP_HIDE_SENSITIVE) && !buf_is_empty(value))
      {
        buf_reset(value);
        buf_addstr(value, "***");
      }

      if (((type == DT_PATH) || IS_MAILBOX(he->type)) && (buf_at(value, 0) == '/'))
        mutt_pretty_mailbox(value->data, value->dsize);

      // Quote/escape the values of config options NOT of these types
      if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) &&
          (type != DT_QUAD) && (type != DT_ENUM) && (type != DT_SORT) &&
          !(flags & CS_DUMP_NO_ESCAPING))
      {
        buf_reset(tmp);
        pretty_var(value->data, tmp);
        buf_strcpy(value, tmp->data);
      }
    }

    /* If necessary, get the default value */
    if (flags & (CS_DUMP_ONLY_CHANGED | CS_DUMP_SHOW_DEFAULTS))
    {
      int rc = cs_he_initial_get(cs, he, initial);
      if (CSR_RESULT(rc) != CSR_SUCCESS)
      {
        result = false; /* LCOV_EXCL_LINE */
        break;          /* LCOV_EXCL_LINE */
      }

      if (((type == DT_PATH) || IS_MAILBOX(he->type)) && (buf_at(initial, 0) == '/'))
        mutt_pretty_mailbox(initial->data, initial->dsize);

      // Quote/escape the values of config options NOT of these types
      if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) &&
          (type != DT_QUAD) && (type != DT_ENUM) && (type != DT_SORT) &&
          !(flags & CS_DUMP_NO_ESCAPING))
      {
        buf_reset(tmp);
        pretty_var(initial->data, tmp);
        buf_strcpy(initial, tmp->data);
      }
    }

    dump_config_neo(cs, he, value, initial, flags, fp);
  }

  buf_pool_release(&value);
  buf_pool_release(&initial);
  buf_pool_release(&tmp);

  return result;
}

/**
 * dump_config2 - XXX
 * bool ansi colour
 * bool url
 * bool docs
 * bool format
 */
void dump_config2(struct ConfigSet *cs, struct HashElemArray *hea,
                  ConfigDumpFlags flags, struct PagedFile *pf)
{
  if (!cs || !hea || !pf)
    return;

  struct Buffer *tmp = buf_pool_get();
  struct Buffer *value = buf_pool_get();
  struct Buffer *swatch = buf_pool_get();

  // measure the width of the config names
  int width = 0;
  struct HashElem **hep = NULL;
  ARRAY_FOREACH(hep, hea)
  {
    const struct ConfigDef *cdef = (*hep)->data;
    width = MAX(width, mutt_str_len(cdef->name));
  }

  const bool align_text = (flags & CS_DUMP_ALIGN_TEXT);
  const bool ansi_color = (flags & CS_DUMP_ANSI_COLOUR);
  const bool link_docs = (flags & CS_DUMP_LINK_DOCS);
  const bool show_docs = (flags & CS_DUMP_SHOW_DOCS);

  int len;
  struct AttrColor *ac = NULL;
  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(hep, hea)
  {
    const struct ConfigDef *cdef = (*hep)->data;

    if (show_docs)
    {
      pr = paged_file_new_row(pf);

      if (ansi_color)
      {
        ac = simple_color_get(MT_COLOR_COMMENT);
        color_log_color_attrs(ac, swatch);
        if (!buf_is_empty(swatch))
          fputs(buf_string(swatch), pf->fp);
      }

      paged_row_add_colored_text(pr, MT_COLOR_COMMENT, "# ");
      paged_row_add_colored_text(pr, MT_COLOR_COMMENT, cdef->docs);
      paged_row_add_newline(pr);

      if (ansi_color)
      {
        if (!buf_is_empty(swatch))
          fputs("\033[0m", pf->fp);
      }
    }

    pr = paged_file_new_row(pf);

    // set config =
    if (ansi_color)
    {
      ac = simple_color_get(MT_COLOR_FUNCTION);
      color_log_color_attrs(ac, swatch);
      if (!buf_is_empty(swatch))
        fputs(buf_string(swatch), pf->fp);
    }

    paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, "set");

    if (ansi_color)
    {
      if (!buf_is_empty(swatch))
        fputs("\033[0m", pf->fp);
    }

    paged_row_add_text(pr, " ");

    if (ansi_color)
    {
      ac = simple_color_get(MT_COLOR_IDENTIFIER);
      color_log_color_attrs(ac, swatch);
      if (!buf_is_empty(swatch))
        fputs(buf_string(swatch), pf->fp);
    }

    if (link_docs)
    {
      // Used to generate unique ids for the urls
      static int seq_num = 1;

      if (CONFIG_TYPE((*hep)->type) == DT_MYVAR)
      {
        static const char *url = "https://neomutt.org/guide/configuration#set-myvar";
        fprintf(pf->fp, "\033]8;id=%d;%s\a", seq_num++, url);
        len = paged_row_add_colored_text(pr, MT_COLOR_IDENTIFIER, cdef->name);
        fputs("\033]8;;\a", pf->fp);
      }
      else
      {
        char *fragment = mutt_str_dup(cdef->name);
        for (char *underscore = fragment; (underscore = strchr(underscore, '_')); underscore++)
        {
          *underscore = '-';
        }

        static const char *url = "https://neomutt.org/guide/reference";
        fprintf(pf->fp, "\033]8;id=%d;%s#%s\a", seq_num++, url, fragment);
        len = paged_row_add_colored_text(pr, MT_COLOR_IDENTIFIER, cdef->name);
        fputs("\033]8;;\a", pf->fp);

        FREE(&fragment);
      }
    }
    else
    {
      len = paged_row_add_colored_text(pr, MT_COLOR_IDENTIFIER, cdef->name);
    }

    if (ansi_color)
    {
      if (!buf_is_empty(swatch))
        fputs("\033[0m", pf->fp);
    }

    if (align_text)
    {
      buf_printf(tmp, "%*s", width - len + 1, "");
      paged_row_add_text(pr, buf_string(tmp));
    }
    else
    {
      paged_row_add_text(pr, " ");
    }

    if (ansi_color)
    {
      ac = simple_color_get(MT_COLOR_OPERATOR);
      color_log_color_attrs(ac, swatch);
      if (!buf_is_empty(swatch))
        fputs(buf_string(swatch), pf->fp);
    }

    paged_row_add_colored_text(pr, MT_COLOR_OPERATOR, "=");
    paged_row_add_text(pr, " ");

    if (ansi_color)
    {
      if (!buf_is_empty(swatch))
        fputs("\033[0m", pf->fp);
    }

    buf_reset(value);
    cs_subset_he_string_get(NeoMutt->sub, *hep, value);

    int type = CONFIG_TYPE((*hep)->type);
    if ((type == DT_STRING) && (cdef->type & D_SENSITIVE) &&
        (flags & CS_DUMP_HIDE_SENSITIVE) && !buf_is_empty(value))
    {
      buf_strcpy(value, "***");
      type = DT_ENUM;
    }

    int cid;
    if ((type == DT_BOOL) || (type == DT_ENUM) || (type == DT_QUAD) || (type == DT_SORT))
      cid = MT_COLOR_ENUM;
    else if ((type == DT_LONG) || (type == DT_NUMBER))
      cid = MT_COLOR_NUMBER;
    else
      cid = MT_COLOR_STRING;

    if (((type == DT_PATH) || IS_MAILBOX((*hep)->type)) && (value->data[0] == '/'))
      mutt_pretty_mailbox(value->data, value->dsize);

    // Quote/escape the values of config options NOT of these types
    if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) &&
        (type != DT_QUAD) && (type != DT_ENUM) && (type != DT_SORT))
    {
      buf_reset(tmp);
      pretty_var(value->data, tmp);
      buf_strcpy(value, tmp->data);
    }

    if (ansi_color)
    {
      ac = simple_color_get(cid);
      color_log_color_attrs(ac, swatch);
      if (!buf_is_empty(swatch))
        fputs(buf_string(swatch), pf->fp);
    }

    paged_row_add_colored_text(pr, cid, buf_string(value));

    if (ansi_color)
    {
      if (!buf_is_empty(swatch))
        fputs("\033[0m", pf->fp);
    }

    paged_row_add_newline(pr);

    if (show_docs)
    {
      pr = paged_file_new_row(pf);
      paged_row_add_newline(pr);
    }
  }

  // :set [hea,flags,pf]; gen_pf(); spager(pf)
  // main [hea,flags,pf]; gen_pf(); done

  // flags: ansi, url, format

  // gen_pf()
  //      ∀ hea collect [name,value,docs]; measure (if format)
  //      ∀ hea write to pf
  //              mt_color always
  //              ansi/url/format flag

  // ansi => mt_color -> ansi conversion

  buf_pool_release(&tmp);
  buf_pool_release(&value);
  buf_pool_release(&swatch);
}
