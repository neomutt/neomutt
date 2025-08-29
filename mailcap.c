/**
 * @file
 * RFC1524 Mailcap routines
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Federico Kircheis <federico.kircheis@gmail.com>
 * Copyright (C) 2018-2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page neo_mailcap RFC1524 Mailcap routines
 *
 * RFC1524 defines a format for the Multimedia Mail Configuration, which is the
 * standard mailcap file format under Unix which specifies what external
 * programs should be used to view/compose/edit multimedia files based on
 * content type.
 *
 * This file contains various functions for implementing a fair subset of
 * RFC1524.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mailcap.h"
#include "attach/lib.h"
#include "muttlib.h"
#include "protos.h"

/**
 * mailcap_expand_command - Expand expandos in a command
 * @param b        Email Body
 * @param filename File containing the email text
 * @param type     Type, e.g. "text/plain"
 * @param command  Buffer containing command
 * @retval 0 Command works on a file
 * @retval 1 Command works on a pipe
 *
 * The command semantics include the following:
 * %s is the filename that contains the mail body data
 * %t is the content type, like text/plain
 * %{parameter} is replaced by the parameter value from the content-type field
 * \% is %
 * Unsupported rfc1524 parameters: these would probably require some doing
 * by neomutt, and can probably just be done by piping the message to metamail
 * %n is the integer number of sub-parts in the multipart
 * %F is "content-type filename" repeated for each sub-part
 */
int mailcap_expand_command(struct Body *b, const char *filename,
                           const char *type, struct Buffer *command)
{
  int needspipe = true;
  struct Buffer *buf = buf_pool_get();
  struct Buffer *quoted = buf_pool_get();
  struct Buffer *param = NULL;
  struct Buffer *type2 = NULL;

  const bool c_mailcap_sanitize = cs_subset_bool(NeoMutt->sub, "mailcap_sanitize");
  const char *cptr = buf_string(command);
  while (*cptr)
  {
    if (*cptr == '\\')
    {
      cptr++;
      if (*cptr)
        buf_addch(buf, *cptr++);
    }
    else if (*cptr == '%')
    {
      cptr++;
      if (*cptr == '{')
      {
        const char *pvalue2 = NULL;

        if (param)
          buf_reset(param);
        else
          param = buf_pool_get();

        /* Copy parameter name into param buffer */
        cptr++;
        while (*cptr && (*cptr != '}'))
          buf_addch(param, *cptr++);

        /* In send mode, use the current charset, since the message hasn't
         * been converted yet.   If noconv is set, then we assume the
         * charset parameter has the correct value instead. */
        if (mutt_istr_equal(buf_string(param), "charset") && b->charset && !b->noconv)
          pvalue2 = b->charset;
        else
          pvalue2 = mutt_param_get(&b->parameter, buf_string(param));

        /* Now copy the parameter value into param buffer */
        if (c_mailcap_sanitize)
          buf_sanitize_filename(param, NONULL(pvalue2), false);
        else
          buf_strcpy(param, pvalue2);

        buf_quote_filename(quoted, buf_string(param), true);
        buf_addstr(buf, buf_string(quoted));
      }
      else if ((*cptr == 's') && filename)
      {
        buf_quote_filename(quoted, filename, true);
        buf_addstr(buf, buf_string(quoted));
        needspipe = false;
      }
      else if (*cptr == 't')
      {
        if (!type2)
        {
          type2 = buf_pool_get();
          if (c_mailcap_sanitize)
            buf_sanitize_filename(type2, type, false);
          else
            buf_strcpy(type2, type);
        }
        buf_quote_filename(quoted, buf_string(type2), true);
        buf_addstr(buf, buf_string(quoted));
      }

      if (*cptr)
        cptr++;
    }
    else
    {
      buf_addch(buf, *cptr++);
    }
  }
  buf_copy(command, buf);

  buf_pool_release(&buf);
  buf_pool_release(&quoted);
  buf_pool_release(&param);
  buf_pool_release(&type2);

  return needspipe;
}

/**
 * get_field - NUL terminate a RFC1524 field
 * @param s String to alter
 * @retval ptr  Start of next field
 * @retval NULL Error
 */
static char *get_field(char *s)
{
  if (!s)
    return NULL;

  char *ch = NULL;

  while ((ch = strpbrk(s, ";\\")))
  {
    if (*ch == '\\')
    {
      s = ch + 1;
      if (*s)
        s++;
    }
    else
    {
      *ch = '\0';
      ch = mutt_str_skip_email_wsp(ch + 1);
      break;
    }
  }
  mutt_str_remove_trailing_ws(s);
  return ch;
}

/**
 * get_field_text - Get the matching text from a mailcap
 * @param field    String to parse
 * @param entry    Save the entry here
 * @param type     Type, e.g. "text/plain"
 * @param filename Mailcap filename
 * @param line     Mailcap line
 * @retval 1 Success
 * @retval 0 Failure
 */
static int get_field_text(char *field, char **entry, const char *type,
                          const char *filename, int line)
{
  field = mutt_str_skip_whitespace(field);
  if (*field == '=')
  {
    if (entry)
    {
      field++;
      field = mutt_str_skip_whitespace(field);
      mutt_str_replace(entry, field);
    }
    return 1;
  }
  else
  {
    mutt_error(_("Improperly formatted entry for type %s in \"%s\" line %d"),
               type, filename, line);
    return 0;
  }
}

/**
 * rfc1524_mailcap_parse - Parse a mailcap entry
 * @param b        Email Body
 * @param filename Filename
 * @param type     Type, e.g. "text/plain"
 * @param entry    Entry, e.g. "compose"
 * @param opt      Option, see #MailcapLookup
 * @retval true  Success
 * @retval false Failure
 */
static bool rfc1524_mailcap_parse(struct Body *b, const char *filename, const char *type,
                                  struct MailcapEntry *entry, enum MailcapLookup opt)
{
  char *buf = NULL;
  bool found = false;
  int line = 0;

  /* rfc1524 mailcap file is of the format:
   * base/type; command; extradefs
   * type can be * for matching all
   * base with no /type is an implicit wild
   * command contains a %s for the filename to pass, default to pipe on stdin
   * extradefs are of the form:
   *  def1="definition"; def2="define \;";
   * line wraps with a \ at the end of the line
   * # for comments */

  /* find length of basetype */
  char *ch = strchr(type, '/');
  if (!ch)
    return false;
  const int btlen = ch - type;

  FILE *fp = mutt_file_fopen(filename, "r");
  if (fp)
  {
    size_t buflen;
    while (!found && (buf = mutt_file_read_line(buf, &buflen, fp, &line, MUTT_RL_CONT)))
    {
      /* ignore comments */
      if (*buf == '#')
        continue;
      mutt_debug(LL_DEBUG2, "mailcap entry: %s\n", buf);

      /* check type */
      ch = get_field(buf);
      if (!mutt_istr_equal(buf, type) && (!mutt_istrn_equal(buf, type, btlen) ||
                                          ((buf[btlen] != '\0') && /* implicit wild */
                                           !mutt_str_equal(buf + btlen, "/*")))) /* wildsubtype */
      {
        continue;
      }

      /* next field is the viewcommand */
      char *field = ch;
      ch = get_field(ch);
      if (entry)
        entry->command = mutt_str_dup(field);

      /* parse the optional fields */
      found = true;
      bool copiousoutput = false;
      bool composecommand = false;
      bool editcommand = false;
      bool printcommand = false;

      while (ch)
      {
        field = ch;
        ch = get_field(ch);
        mutt_debug(LL_DEBUG2, "field: %s\n", field);
        size_t plen;

        if (mutt_istr_equal(field, "needsterminal"))
        {
          if (entry)
            entry->needsterminal = true;
        }
        else if (mutt_istr_equal(field, "copiousoutput"))
        {
          copiousoutput = true;
          if (entry)
            entry->copiousoutput = true;
        }
        else if ((plen = mutt_istr_startswith(field, "composetyped")))
        {
          /* this compare most occur before compose to match correctly */
          if (get_field_text(field + plen, entry ? &entry->composetypecommand : NULL,
                             type, filename, line))
          {
            composecommand = true;
          }
        }
        else if ((plen = mutt_istr_startswith(field, "compose")))
        {
          if (get_field_text(field + plen, entry ? &entry->composecommand : NULL,
                             type, filename, line))
          {
            composecommand = true;
          }
        }
        else if ((plen = mutt_istr_startswith(field, "print")))
        {
          if (get_field_text(field + plen, entry ? &entry->printcommand : NULL,
                             type, filename, line))
          {
            printcommand = true;
          }
        }
        else if ((plen = mutt_istr_startswith(field, "edit")))
        {
          if (get_field_text(field + plen, entry ? &entry->editcommand : NULL,
                             type, filename, line))
          {
            editcommand = true;
          }
        }
        else if ((plen = mutt_istr_startswith(field, "nametemplate")))
        {
          get_field_text(field + plen, entry ? &entry->nametemplate : NULL,
                         type, filename, line);
        }
        else if ((plen = mutt_istr_startswith(field, "x-convert")))
        {
          get_field_text(field + plen, entry ? &entry->convert : NULL, type, filename, line);
        }
        else if ((plen = mutt_istr_startswith(field, "test")))
        {
          /* This routine executes the given test command to determine
           * if this is the right entry.  */
          char *test_command = NULL;

          if (get_field_text(field + plen, &test_command, type, filename, line) && test_command)
          {
            struct Buffer *command = buf_pool_get();
            struct Buffer *afilename = buf_pool_get();
            buf_strcpy(command, test_command);
            const bool c_mailcap_sanitize = cs_subset_bool(NeoMutt->sub, "mailcap_sanitize");
            if (c_mailcap_sanitize)
              buf_sanitize_filename(afilename, NONULL(b->filename), true);
            else
              buf_strcpy(afilename, b->filename);
            if (mailcap_expand_command(b, buf_string(afilename), type, command) == 1)
            {
              mutt_debug(LL_DEBUG1, "mailcap command needs a pipe: %s\n",
                         buf_string(command));
            }

            if (mutt_system(buf_string(command)))
            {
              /* a non-zero exit code means test failed */
              found = false;
            }
            FREE(&test_command);
            buf_pool_release(&command);
            buf_pool_release(&afilename);
          }
        }
        else if (mutt_istr_startswith(field, "x-neomutt-keep"))
        {
          if (entry)
            entry->xneomuttkeep = true;
        }
        else if (mutt_istr_startswith(field, "x-neomutt-nowrap"))
        {
          if (entry)
            entry->xneomuttnowrap = true;
          b->nowrap = true;
        }
      } /* while (ch) */

      if (opt == MUTT_MC_AUTOVIEW)
      {
        if (!copiousoutput)
          found = false;
      }
      else if (opt == MUTT_MC_COMPOSE)
      {
        if (!composecommand)
          found = false;
      }
      else if (opt == MUTT_MC_EDIT)
      {
        if (!editcommand)
          found = false;
      }
      else if (opt == MUTT_MC_PRINT)
      {
        if (!printcommand)
          found = false;
      }

      if (!found)
      {
        /* reset */
        if (entry)
        {
          FREE(&entry->command);
          FREE(&entry->composecommand);
          FREE(&entry->composetypecommand);
          FREE(&entry->editcommand);
          FREE(&entry->printcommand);
          FREE(&entry->nametemplate);
          FREE(&entry->convert);
          entry->needsterminal = false;
          entry->copiousoutput = false;
          entry->xneomuttkeep = false;
        }
      }
    }
    mutt_file_fclose(&fp);
  }

  FREE(&buf);
  return found;
}

/**
 * mailcap_entry_new - Allocate memory for a new rfc1524 entry
 * @retval ptr An un-initialized struct MailcapEntry
 */
struct MailcapEntry *mailcap_entry_new(void)
{
  return MUTT_MEM_CALLOC(1, struct MailcapEntry);
}

/**
 * mailcap_entry_free - Deallocate an struct MailcapEntry
 * @param[out] ptr MailcapEntry to deallocate
 */
void mailcap_entry_free(struct MailcapEntry **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MailcapEntry *me = *ptr;

  FREE(&me->command);
  FREE(&me->testcommand);
  FREE(&me->composecommand);
  FREE(&me->composetypecommand);
  FREE(&me->editcommand);
  FREE(&me->printcommand);
  FREE(&me->nametemplate);
  FREE(ptr);
}

/**
 * mailcap_lookup - Find given type in the list of mailcap files
 * @param b      Message body
 * @param type   Text type in "type/subtype" format
 * @param typelen Length of the type
 * @param entry  struct MailcapEntry to populate with results
 * @param opt    Type of mailcap entry to lookup, see #MailcapLookup
 * @retval true  If *entry is not NULL it populates it with the mailcap entry
 * @retval false No matching entry is found
 *
 * Find the given type in the list of mailcap files.
 */
bool mailcap_lookup(struct Body *b, char *type, size_t typelen,
                    struct MailcapEntry *entry, enum MailcapLookup opt)
{
  /* rfc1524 specifies that a path of mailcap files should be searched.
   * joy.  They say
   * $HOME/.mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap, etc
   * and overridden by the MAILCAPS environment variable, and, just to be nice,
   * we'll make it specifiable in .neomuttrc */
  const struct Slist *c_mailcap_path = cs_subset_slist(NeoMutt->sub, "mailcap_path");
  if (!c_mailcap_path || (c_mailcap_path->count == 0))
  {
    /* L10N:
       Mutt is trying to look up a mailcap value, but $mailcap_path is empty.
       We added a reference to the MAILCAPS environment variable as a hint too.

       Because the variable is automatically populated by Mutt, this
       should only occur if the user deliberately runs in their shell:
         export MAILCAPS=

       or deliberately runs inside Mutt or their .muttrc:
         set mailcap_path=""
         -or-
         unset mailcap_path
    */
    mutt_error(_("Neither mailcap_path nor MAILCAPS specified"));
    return false;
  }

  mutt_check_lookup_list(b, type, typelen);

  struct Buffer *path = buf_pool_get();
  bool found = false;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &c_mailcap_path->head, entries)
  {
    buf_strcpy(path, np->data);
    buf_expand_path(path);

    mutt_debug(LL_DEBUG2, "Checking mailcap file: %s\n", buf_string(path));
    found = rfc1524_mailcap_parse(b, buf_string(path), type, entry, opt);
    if (found)
      break;
  }

  buf_pool_release(&path);

  if (entry && !found)
    mutt_error(_("mailcap entry for type %s not found"), type);

  return found;
}

/**
 * mailcap_expand_filename - Expand a new filename from a template or existing filename
 * @param nametemplate Template
 * @param oldfile      Original filename
 * @param newfile      Buffer for new filename
 *
 * If there is no nametemplate, the stripped oldfile name is used as the
 * template for newfile.
 *
 * If there is no oldfile, the stripped nametemplate name is used as the
 * template for newfile.
 *
 * If both a nametemplate and oldfile are specified, the template is checked
 * for a "%s". If none is found, the nametemplate is used as the template for
 * newfile.  The first path component of the nametemplate and oldfile are ignored.
 */
void mailcap_expand_filename(const char *nametemplate, const char *oldfile,
                             struct Buffer *newfile)
{
  int i, j, k;
  char *s = NULL;
  bool lmatch = false, rmatch = false;

  buf_reset(newfile);

  /* first, ignore leading path components */

  if (nametemplate && (s = strrchr(nametemplate, '/')))
    nametemplate = s + 1;

  if (oldfile && (s = strrchr(oldfile, '/')))
    oldfile = s + 1;

  if (!nametemplate)
  {
    if (oldfile)
      buf_strcpy(newfile, oldfile);
  }
  else if (!oldfile)
  {
    mutt_file_expand_fmt(newfile, nametemplate, "neomutt");
  }
  else /* oldfile && nametemplate */
  {
    /* first, compare everything left from the "%s"
     * (if there is one).  */

    lmatch = true;
    bool ps = false;
    for (i = 0; nametemplate[i]; i++)
    {
      if ((nametemplate[i] == '%') && (nametemplate[i + 1] == 's'))
      {
        ps = true;
        break;
      }

      /* note that the following will _not_ read beyond oldfile's end. */

      if (lmatch && (nametemplate[i] != oldfile[i]))
        lmatch = false;
    }

    if (ps)
    {
      /* If we had a "%s", check the rest. */

      /* now, for the right part: compare everything right from
       * the "%s" to the final part of oldfile.
       *
       * The logic here is as follows:
       *
       * - We start reading from the end.
       * - There must be a match _right_ from the "%s",
       *   thus the i + 2.
       * - If there was a left hand match, this stuff
       *   must not be counted again.  That's done by the
       *   condition (j >= (lmatch ? i : 0)).  */

      rmatch = true;

      for (j = mutt_str_len(oldfile) - 1, k = mutt_str_len(nametemplate) - 1;
           (j >= (lmatch ? i : 0)) && (k >= (i + 2)); j--, k--)
      {
        if (nametemplate[k] != oldfile[j])
        {
          rmatch = false;
          break;
        }
      }

      /* Now, check if we had a full match. */

      if (k >= i + 2)
        rmatch = false;

      struct Buffer *left = buf_pool_get();
      struct Buffer *right = buf_pool_get();

      if (!lmatch)
        buf_strcpy_n(left, nametemplate, i);
      if (!rmatch)
        buf_strcpy(right, nametemplate + i + 2);
      buf_printf(newfile, "%s%s%s", buf_string(left), oldfile, buf_string(right));

      buf_pool_release(&left);
      buf_pool_release(&right);
    }
    else
    {
      /* no "%s" in the name template. */
      buf_strcpy(newfile, nametemplate);
    }
  }

  mutt_adv_mktemp(newfile);
}
