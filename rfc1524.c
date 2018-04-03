/**
 * @file
 * RFC1524 Mailcap routines
 *
 * @authors
 * Copyright (C) 1996-2000,2003,2012 Michael R. Elkins <me@mutt.org>
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

/* RFC1524 defines a format for the Multimedia Mail Configuration, which is the
 * standard mailcap file format under Unix which specifies what external
 * programs should be used to view/compose/edit multimedia files based on
 * content type.
 *
 * This file contains various functions for implementing a fair subset of
 * RFC1524.
 */

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "rfc1524.h"
#include "body.h"
#include "globals.h"
#include "options.h"
#include "protos.h"

/**
 * rfc1524_expand_command - Expand expandos in a command
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
 *
 * In addition, this function returns a 0 if the command works on a file,
 * and 1 if the command works on a pipe.
 */
int rfc1524_expand_command(struct Body *a, char *filename, char *type, char *command, int clen)
{
  int x = 0, y = 0;
  int needspipe = true;
  char buf[LONG_STRING];
  char type2[LONG_STRING];

  mutt_str_strfcpy(type2, type, sizeof(type2));

  if (MailcapSanitize)
    mutt_file_sanitize_filename(type2, 0);

  while (x < clen - 1 && command[x] && y < sizeof(buf) - 1)
  {
    if (command[x] == '\\')
    {
      x++;
      buf[y++] = command[x++];
    }
    else if (command[x] == '%')
    {
      x++;
      if (command[x] == '{')
      {
        char param[STRING];
        char pvalue[STRING];
        char *pvalue2 = NULL;
        int z = 0;

        x++;
        while (command[x] && command[x] != '}' && z < sizeof(param) - 1)
          param[z++] = command[x++];
        param[z] = '\0';

        pvalue2 = mutt_param_get(&a->parameter, param);
        mutt_str_strfcpy(pvalue, NONULL(pvalue2), sizeof(pvalue));
        if (MailcapSanitize)
          mutt_file_sanitize_filename(pvalue, 0);

        y += mutt_file_quote_filename(buf + y, sizeof(buf) - y, pvalue);
      }
      else if (command[x] == 's' && filename != NULL)
      {
        y += mutt_file_quote_filename(buf + y, sizeof(buf) - y, filename);
        needspipe = false;
      }
      else if (command[x] == 't')
      {
        y += mutt_file_quote_filename(buf + y, sizeof(buf) - y, type2);
      }
      x++;
    }
    else
      buf[y++] = command[x++];
  }
  buf[y] = '\0';
  mutt_str_strfcpy(command, buf, clen);

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
  char *ch = NULL;

  if (!s)
    return NULL;

  while ((ch = strpbrk(s, ";\\")) != NULL)
  {
    if (*ch == '\\')
    {
      s = ch + 1;
      if (*s)
        s++;
    }
    else
    {
      *ch = 0;
      ch = mutt_str_skip_email_wsp(ch + 1);
      break;
    }
  }
  mutt_str_remove_trailing_ws(s);
  return ch;
}

static int get_field_text(char *field, char **entry, char *type, char *filename, int line)
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

static int rfc1524_mailcap_parse(struct Body *a, char *filename, char *type,
                                 struct Rfc1524MailcapEntry *entry, int opt)
{
  char *buf = NULL;
  int found = false;
  int line = 0;

  /* rfc1524 mailcap file is of the format:
   * base/type; command; extradefs
   * type can be * for matching all
   * base with no /type is an implicit wild
   * command contains a %s for the filename to pass, default to pipe on stdin
   * extradefs are of the form:
   *  def1="definition"; def2="define \;";
   * line wraps with a \ at the end of the line
   * # for comments
   */

  /* find length of basetype */
  char *ch = strchr(type, '/');
  if (!ch)
    return false;
  const int btlen = ch - type;

  FILE *fp = fopen(filename, "r");
  if (fp)
  {
    size_t buflen;
    while (!found && (buf = mutt_file_read_line(buf, &buflen, fp, &line, MUTT_CONT)) != NULL)
    {
      /* ignore comments */
      if (*buf == '#')
        continue;
      mutt_debug(2, "mailcap entry: %s\n", buf);

      /* check type */
      ch = get_field(buf);
      if ((mutt_str_strcasecmp(buf, type) != 0) &&
          ((mutt_str_strncasecmp(buf, type, btlen) != 0) ||
           (buf[btlen] != 0 &&                           /* implicit wild */
            (mutt_str_strcmp(buf + btlen, "/*") != 0)))) /* wildsubtype */
        continue;

      /* next field is the viewcommand */
      char *field = ch;
      ch = get_field(ch);
      if (entry)
        entry->command = mutt_str_strdup(field);

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
        mutt_debug(2, "field: %s\n", field);

        if (mutt_str_strcasecmp(field, "needsterminal") == 0)
        {
          if (entry)
            entry->needsterminal = true;
        }
        else if (mutt_str_strcasecmp(field, "copiousoutput") == 0)
        {
          copiousoutput = true;
          if (entry)
            entry->copiousoutput = true;
        }
        else if (mutt_str_strncasecmp(field, "composetyped", 12) == 0)
        {
          /* this compare most occur before compose to match correctly */
          if (get_field_text(field + 12, entry ? &entry->composetypecommand : NULL,
                             type, filename, line))
          {
            composecommand = true;
          }
        }
        else if (mutt_str_strncasecmp(field, "compose", 7) == 0)
        {
          if (get_field_text(field + 7, entry ? &entry->composecommand : NULL,
                             type, filename, line))
          {
            composecommand = true;
          }
        }
        else if (mutt_str_strncasecmp(field, "print", 5) == 0)
        {
          if (get_field_text(field + 5, entry ? &entry->printcommand : NULL,
                             type, filename, line))
          {
            printcommand = true;
          }
        }
        else if (mutt_str_strncasecmp(field, "edit", 4) == 0)
        {
          if (get_field_text(field + 4, entry ? &entry->editcommand : NULL, type, filename, line))
            editcommand = true;
        }
        else if (mutt_str_strncasecmp(field, "nametemplate", 12) == 0)
        {
          get_field_text(field + 12, entry ? &entry->nametemplate : NULL, type,
                         filename, line);
        }
        else if (mutt_str_strncasecmp(field, "x-convert", 9) == 0)
        {
          get_field_text(field + 9, entry ? &entry->convert : NULL, type, filename, line);
        }
        else if (mutt_str_strncasecmp(field, "test", 4) == 0)
        {
          /*
           * This routine executes the given test command to determine
           * if this is the right entry.
           */
          char *test_command = NULL;

          if (get_field_text(field + 4, &test_command, type, filename, line) && test_command)
          {
            const size_t len = mutt_str_strlen(test_command) + STRING;
            mutt_mem_realloc(&test_command, len);
            if (rfc1524_expand_command(a, a->filename, type, test_command, len) == 1)
            {
              mutt_debug(1, "Command is expecting to be piped\n");
            }
            if (mutt_system(test_command) != 0)
            {
              /* a non-zero exit code means test failed */
              found = false;
            }
            FREE(&test_command);
          }
        }
      } /* while (ch) */

      if (opt == MUTT_AUTOVIEW)
      {
        if (!copiousoutput)
          found = false;
      }
      else if (opt == MUTT_COMPOSE)
      {
        if (!composecommand)
          found = false;
      }
      else if (opt == MUTT_EDIT)
      {
        if (!editcommand)
          found = false;
      }
      else if (opt == MUTT_PRINT)
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
        }
      }
    } /* while (!found && (buf = mutt_file_read_line ())) */
    mutt_file_fclose(&fp);
  } /* if ((fp = fopen ())) */
  FREE(&buf);
  return found;
}

/**
 * rfc1524_new_entry - Allocate memory for a new rfc1524 entry
 * @retval ptr An un-initialized struct Rfc1524MailcapEntry
 */
struct Rfc1524MailcapEntry *rfc1524_new_entry(void)
{
  return mutt_mem_calloc(1, sizeof(struct Rfc1524MailcapEntry));
}

/**
 * rfc1524_free_entry - Deallocate an struct Rfc1524MailcapEntry
 * @param entry Rfc1524MailcapEntry to deallocate
 */
void rfc1524_free_entry(struct Rfc1524MailcapEntry **entry)
{
  struct Rfc1524MailcapEntry *p = *entry;

  FREE(&p->command);
  FREE(&p->testcommand);
  FREE(&p->composecommand);
  FREE(&p->composetypecommand);
  FREE(&p->editcommand);
  FREE(&p->printcommand);
  FREE(&p->nametemplate);
  FREE(entry);
}

/**
 * rfc1524_mailcap_lookup - Find given type in the list of mailcap files
 * @param a      Message body
 * @param type   Text type in "type/subtype" format
 * @param entry  struct Rfc1524MailcapEntry to populate with results
 * @param opt    Type of mailcap entry to lookup
 * @retval 1 on success. If *entry is not NULL it populates it with the mailcap entry
 * @retval 0 if no matching entry is found
 *
 * opt can be one of: #MUTT_EDIT, #MUTT_COMPOSE, #MUTT_PRINT, #MUTT_AUTOVIEW
 *
 * Find the given type in the list of mailcap files.
 */
int rfc1524_mailcap_lookup(struct Body *a, char *type,
                           struct Rfc1524MailcapEntry *entry, int opt)
{
  char path[_POSIX_PATH_MAX];
  int found = false;
  char *curr = MailcapPath;

  /* rfc1524 specifies that a path of mailcap files should be searched.
   * joy.  They say
   * $HOME/.mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap, etc
   * and overridden by the MAILCAPS environment variable, and, just to be nice,
   * we'll make it specifiable in .neomuttrc
   */
  if (!curr || !*curr)
  {
    mutt_error(_("No mailcap path specified"));
    return 0;
  }

  mutt_check_lookup_list(a, type, SHORT_STRING);

  while (!found && *curr)
  {
    int x = 0;
    while (*curr && *curr != ':' && x < sizeof(path) - 1)
    {
      path[x++] = *curr;
      curr++;
    }
    if (*curr)
      curr++;

    if (!x)
      continue;

    path[x] = '\0';
    mutt_expand_path(path, sizeof(path));

    mutt_debug(2, "Checking mailcap file: %s\n", path);
    found = rfc1524_mailcap_parse(a, path, type, entry, opt);
  }

  if (entry && !found)
    mutt_error(_("mailcap entry for type %s not found"), type);

  return found;
}

/**
 * rfc1524_expand_filename - Expand a new filename from a template or existing filename
 * @param nametemplate Template
 * @param oldfile      Original filename
 * @param newfile      Buffer for new filename
 * @param nflen        Buffer length
 * @retval 0 if the left and right components of the oldfile and newfile match
 * @retval 1 otherwise
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
 *
 */
int rfc1524_expand_filename(char *nametemplate, char *oldfile, char *newfile, size_t nflen)
{
  int i, j, k, ps;
  char *s = NULL;
  bool lmatch = false, rmatch = false;
  char left[_POSIX_PATH_MAX];
  char right[_POSIX_PATH_MAX];

  newfile[0] = 0;

  /* first, ignore leading path components */

  if (nametemplate && (s = strrchr(nametemplate, '/')))
    nametemplate = s + 1;

  if (oldfile && (s = strrchr(oldfile, '/')))
    oldfile = s + 1;

  if (!nametemplate)
  {
    if (oldfile)
      mutt_str_strfcpy(newfile, oldfile, nflen);
  }
  else if (!oldfile)
  {
    mutt_expand_fmt(newfile, nflen, nametemplate, "neomutt");
  }
  else /* oldfile && nametemplate */
  {
    /* first, compare everything left from the "%s"
     * (if there is one).  */

    lmatch = true;
    ps = 0;
    for (i = 0; nametemplate[i]; i++)
    {
      if (nametemplate[i] == '%' && nametemplate[i + 1] == 's')
      {
        ps = 1;
        break;
      }

      /* note that the following will _not_ read beyond oldfile's end. */

      if (lmatch && nametemplate[i] != oldfile[i])
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
       *   condition (j >= (lmatch ? i : 0)).
       */

      rmatch = true;

      for (j = mutt_str_strlen(oldfile) - 1, k = mutt_str_strlen(nametemplate) - 1;
           j >= (lmatch ? i : 0) && k >= i + 2; j--, k--)
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

      if (lmatch)
        *left = 0;
      else
        mutt_str_strnfcpy(left, nametemplate, i, sizeof(left));

      if (rmatch)
        *right = 0;
      else
        mutt_str_strfcpy(right, nametemplate + i + 2, sizeof(right));

      snprintf(newfile, nflen, "%s%s%s", left, oldfile, right);
    }
    else
    {
      /* no "%s" in the name template. */
      mutt_str_strfcpy(newfile, nametemplate, nflen);
    }
  }

  mutt_adv_mktemp(newfile, nflen);

  if (rmatch && lmatch)
    return 0;
  else
    return 1;
}
