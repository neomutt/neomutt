/**
 * @file
 * GUI basic built-in text editor
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page edit GUI basic built-in text editor
 *
 * GUI basic built-in text editor
 */

/* Close approximation of the mailx(1) builtin editor for sending mail. */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "alias.h"
#include "context.h"
#include "curs_lib.h"
#include "globals.h"
#include "hdrline.h"
#include "mutt_curses.h"
#include "mutt_header.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "protos.h"

/* These Config Variables are only used in edit.c */
char *C_Escape; ///< Config: Escape character to use for functions in the built-in editor

/* SLcurses_waddnstr() can't take a "const char *", so this is only
 * declared "static" (sigh)
 */
static char *EditorHelp1 =
    N_("~~              insert a line beginning with a single ~\n"
       "~b users        add users to the Bcc: field\n"
       "~c users        add users to the Cc: field\n"
       "~f messages     include messages\n"
       "~F messages     same as ~f, except also include headers\n"
       "~h              edit the message header\n"
       "~m messages     include and quote messages\n"
       "~M messages     same as ~m, except include headers\n"
       "~p              print the message\n");

static char *EditorHelp2 =
    N_("~q              write file and quit editor\n"
       "~r file         read a file into the editor\n"
       "~t users        add users to the To: field\n"
       "~u              recall the previous line\n"
       "~v              edit message with the $visual editor\n"
       "~w file         write message to file\n"
       "~x              abort changes and quit editor\n"
       "~?              this message\n"
       ".               on a line by itself ends input\n");

/**
 * be_snarf_data - Read data from a file into a buffer
 * @param[in]  fp     File to read from
 * @param[out] buf    Buffer allocated to save data
 * @param[out] bufmax Allocated size of buffer
 * @param[out] buflen Bytes of buffer used
 * @param[in]  offset Start reading at this file offset
 * @param[in]  bytes  Read this many bytes
 * @param[in]  prefix If true, prefix the lines with the #C_IndentString
 * @retval ptr Pointer to allocated buffer
 */
static char **be_snarf_data(FILE *fp, char **buf, int *bufmax, int *buflen,
                            LOFF_T offset, int bytes, int prefix)
{
  char tmp[8192];
  char *p = tmp;
  int tmplen = sizeof(tmp);

  tmp[sizeof(tmp) - 1] = '\0';
  if (prefix)
  {
    mutt_str_strfcpy(tmp, C_IndentString, sizeof(tmp));
    tmplen = mutt_str_strlen(tmp);
    p = tmp + tmplen;
    tmplen = sizeof(tmp) - tmplen;
  }

  fseeko(fp, offset, SEEK_SET);
  while (bytes > 0)
  {
    if (!fgets(p, tmplen - 1, fp))
      break;
    bytes -= mutt_str_strlen(p);
    if (*bufmax == *buflen)
      mutt_mem_realloc(&buf, sizeof(char *) * (*bufmax += 25));
    buf[(*buflen)++] = mutt_str_strdup(tmp);
  }
  if (buf && (*bufmax == *buflen))
  { /* Do not smash memory past buf */
    mutt_mem_realloc(&buf, sizeof(char *) * (++*bufmax));
  }
  if (buf)
    buf[*buflen] = NULL;
  return buf;
}

/**
 * be_snarf_file - Read a file into a buffer
 * @param[in]  path    File to read
 * @param[out] buf     Buffer allocated to save data
 * @param[out] max     Allocated size of buffer
 * @param[out] len     Bytes of buffer used
 * @param[in]  verbose If true, report the file and bytes read
 * @retval ptr Pointer to allocated buffer
 */
static char **be_snarf_file(const char *path, char **buf, int *max, int *len, bool verbose)
{
  char tmp[1024];
  struct stat sb;

  FILE *fp = fopen(path, "r");
  if (fp)
  {
    fstat(fileno(fp), &sb);
    buf = be_snarf_data(fp, buf, max, len, 0, sb.st_size, 0);
    if (verbose)
    {
      snprintf(tmp, sizeof(tmp), "\"%s\" %lu bytes\n", path, (unsigned long) sb.st_size);
      mutt_window_addstr(tmp);
    }
    mutt_file_fclose(&fp);
  }
  else
  {
    snprintf(tmp, sizeof(tmp), "%s: %s\n", path, strerror(errno));
    mutt_window_addstr(tmp);
  }
  return buf;
}

/**
 * be_barf_file - Write a buffer to a file
 * @param[in]  path   Path to write to
 * @param[out] buf    Buffer to read from
 * @param[in]  buflen Length of buffer
 * @retval  0 Success
 * @retval -1 Error
 */
static int be_barf_file(const char *path, char **buf, int buflen)
{
  FILE *fp = fopen(path, "w");
  if (!fp)
  {
    mutt_window_addstr(strerror(errno));
    mutt_window_addch('\n');
    return -1;
  }
  for (int i = 0; i < buflen; i++)
    fputs(buf[i], fp);
  if (mutt_file_fclose(&fp) == 0)
    return 0;
  mutt_window_printf("fclose: %s\n", strerror(errno));
  return -1;
}

/**
 * be_free_memory - Free an array of buffers
 * @param[out] buf    Buffer to free
 * @param[in]  buflen Number of buffers to free
 */
static void be_free_memory(char **buf, int buflen)
{
  while (buflen-- > 0)
    FREE(&buf[buflen]);
  FREE(&buf);
}

/**
 * be_include_messages - Gather the contents of some messages
 * @param[in]  msg      List of message numbers (space or comma separated)
 * @param[out] buf      Buffer allocated to save data
 * @param[out] bufmax   Allocated size of buffer
 * @param[out] buflen   Bytes of buffer used
 * @param[in]  pfx      Prefix
 * @param[in]  inc_hdrs If true, include the message headers
 * @retval ptr Pointer to allocated buffer
 */
static char **be_include_messages(char *msg, char **buf, int *bufmax,
                                  int *buflen, int pfx, int inc_hdrs)
{
  int n;
  // int offset, bytes;
  char tmp[1024];

  if (!msg || !buf || !bufmax || !buflen)
    return buf;

  while ((msg = strtok(msg, " ,")))
  {
    if ((mutt_str_atoi(msg, &n) == 0) && (n > 0) && (n <= Context->mailbox->msg_count))
    {
      n--;

      /* add the attribution */
      if (C_Attribution)
      {
        setlocale(LC_TIME, NONULL(C_AttributionLocale));
        mutt_make_string(tmp, sizeof(tmp) - 1, 0, C_Attribution, Context,
                         Context->mailbox, Context->mailbox->emails[n]);
        setlocale(LC_TIME, "");
        strcat(tmp, "\n");
      }

      if (*bufmax == *buflen)
        mutt_mem_realloc(&buf, sizeof(char *) * (*bufmax += 25));
      buf[(*buflen)++] = mutt_str_strdup(tmp);

#if 0
      /* This only worked for mbox Mailboxes because they had Context->fp set.
       * As that no longer exists, the code is now completely broken. */
      bytes = Context->mailbox->emails[n]->content->length;
      if (inc_hdrs)
      {
        offset = Context->mailbox->emails[n]->offset;
        bytes += Context->mailbox->emails[n]->content->offset - offset;
      }
      else
        offset = Context->mailbox->emails[n]->content->offset;
      buf = be_snarf_data(Context->fp, buf, bufmax, buflen, offset, bytes, pfx);
#endif

      if (*bufmax == *buflen)
        mutt_mem_realloc(&buf, sizeof(char *) * (*bufmax += 25));
      buf[(*buflen)++] = mutt_str_strdup("\n");
    }
    else
      mutt_window_printf(_("%d: invalid message number.\n"), n);
    msg = NULL;
  }
  return buf;
}

/**
 * be_print_header - Print a message Header
 * @param env Envelope to print
 */
static void be_print_header(struct Envelope *env)
{
  char tmp[8192];

  if (!TAILQ_EMPTY(&env->to))
  {
    mutt_window_addstr("To: ");
    tmp[0] = '\0';
    mutt_addrlist_write(tmp, sizeof(tmp), &env->to, true);
    mutt_window_addstr(tmp);
    mutt_window_addch('\n');
  }
  if (!TAILQ_EMPTY(&env->cc))
  {
    mutt_window_addstr("Cc: ");
    tmp[0] = '\0';
    mutt_addrlist_write(tmp, sizeof(tmp), &env->cc, true);
    mutt_window_addstr(tmp);
    mutt_window_addch('\n');
  }
  if (!TAILQ_EMPTY(&env->bcc))
  {
    mutt_window_addstr("Bcc: ");
    tmp[0] = '\0';
    mutt_addrlist_write(tmp, sizeof(tmp), &env->bcc, true);
    mutt_window_addstr(tmp);
    mutt_window_addch('\n');
  }
  if (env->subject)
  {
    mutt_window_addstr("Subject: ");
    mutt_window_addstr(env->subject);
    mutt_window_addch('\n');
  }
  mutt_window_addch('\n');
}

/**
 * be_edit_header - Edit the message headers
 * @param e     Email
 * @param force override the $ask* vars (used for the ~h command)
 */
static void be_edit_header(struct Envelope *e, bool force)
{
  char tmp[8192];

  mutt_window_move(MuttMessageWindow, 0, 0);

  mutt_window_addstr("To: ");
  tmp[0] = '\0';
  mutt_addrlist_to_local(&e->to);
  mutt_addrlist_write(tmp, sizeof(tmp), &e->to, false);
  if (TAILQ_EMPTY(&e->to) || force)
  {
    if (mutt_enter_string(tmp, sizeof(tmp), 4, MUTT_COMP_NO_FLAGS) == 0)
    {
      mutt_addrlist_clear(&e->to);
      mutt_addrlist_parse2(&e->to, tmp);
      mutt_expand_aliases(&e->to);
      mutt_addrlist_to_intl(&e->to, NULL); /* XXX - IDNA error reporting? */
      tmp[0] = '\0';
      mutt_addrlist_write(tmp, sizeof(tmp), &e->to, true);
      mutt_window_mvaddstr(MuttMessageWindow, 0, 4, tmp);
    }
  }
  else
  {
    mutt_addrlist_to_intl(&e->to, NULL); /* XXX - IDNA error reporting? */
    mutt_window_addstr(tmp);
  }
  mutt_window_addch('\n');

  if (!e->subject || force)
  {
    mutt_window_addstr("Subject: ");
    mutt_str_strfcpy(tmp, e->subject ? e->subject : "", sizeof(tmp));
    if (mutt_enter_string(tmp, sizeof(tmp), 9, MUTT_COMP_NO_FLAGS) == 0)
      mutt_str_replace(&e->subject, tmp);
    mutt_window_addch('\n');
  }

  if ((TAILQ_EMPTY(&e->cc) && C_Askcc) || force)
  {
    mutt_window_addstr("Cc: ");
    tmp[0] = '\0';
    mutt_addrlist_to_local(&e->cc);
    mutt_addrlist_write(tmp, sizeof(tmp), &e->cc, false);
    if (mutt_enter_string(tmp, sizeof(tmp), 4, MUTT_COMP_NO_FLAGS) == 0)
    {
      mutt_addrlist_clear(&e->cc);
      mutt_addrlist_parse2(&e->cc, tmp);
      mutt_expand_aliases(&e->cc);
      tmp[0] = '\0';
      mutt_addrlist_to_intl(&e->cc, NULL);
      mutt_addrlist_write(tmp, sizeof(tmp), &e->cc, true);
      mutt_window_mvaddstr(MuttMessageWindow, 0, 4, tmp);
    }
    else
      mutt_addrlist_to_intl(&e->cc, NULL);
    mutt_window_addch('\n');
  }

  if (C_Askbcc || force)
  {
    mutt_window_addstr("Bcc: ");
    tmp[0] = '\0';
    mutt_addrlist_to_local(&e->bcc);
    mutt_addrlist_write(tmp, sizeof(tmp), &e->bcc, false);
    if (mutt_enter_string(tmp, sizeof(tmp), 5, MUTT_COMP_NO_FLAGS) == 0)
    {
      mutt_addrlist_clear(&e->bcc);
      mutt_addrlist_parse2(&e->bcc, tmp);
      mutt_expand_aliases(&e->bcc);
      mutt_addrlist_to_intl(&e->bcc, NULL);
      tmp[0] = '\0';
      mutt_addrlist_write(tmp, sizeof(tmp), &e->bcc, true);
      mutt_window_mvaddstr(MuttMessageWindow, 0, 5, tmp);
    }
    else
      mutt_addrlist_to_intl(&e->bcc, NULL);
    mutt_window_addch('\n');
  }
}

/**
 * mutt_builtin_editor - Show the user the built-in editor
 * @param path File to read
 * @param e_new  New Email
 * @param e_cur  Current Email
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_builtin_editor(const char *path, struct Email *e_new, struct Email *e_cur)
{
  char **buf = NULL;
  int bufmax = 0, buflen = 0;
  char tmp[1024];
  bool abort = false;
  bool done = false;
  char *p = NULL;

  scrollok(stdscr, true);

  be_edit_header(e_new->env, false);

  mutt_window_addstr(_("(End message with a . on a line by itself)\n"));

  buf = be_snarf_file(path, buf, &bufmax, &buflen, false);

  tmp[0] = '\0';
  while (!done)
  {
    if (mutt_enter_string(tmp, sizeof(tmp), 0, MUTT_COMP_NO_FLAGS) == -1)
    {
      tmp[0] = '\0';
      continue;
    }
    mutt_window_addch('\n');

    if (C_Escape && (tmp[0] == C_Escape[0]) && (tmp[1] != C_Escape[0]))
    {
      /* remove trailing whitespace from the line */
      p = tmp + mutt_str_strlen(tmp) - 1;
      while ((p >= tmp) && IS_SPACE(*p))
        *p-- = '\0';

      p = tmp + 2;
      SKIPWS(p);

      switch (tmp[1])
      {
        case '?':
          mutt_window_addstr(_(EditorHelp1));
          mutt_window_addstr(_(EditorHelp2));
          break;
        case 'b':
          mutt_addrlist_parse2(&e_new->env->bcc, p);
          mutt_expand_aliases(&e_new->env->bcc);
          break;
        case 'c':
          mutt_addrlist_parse2(&e_new->env->cc, p);
          mutt_expand_aliases(&e_new->env->cc);
          break;
        case 'h':
          be_edit_header(e_new->env, true);
          break;
        case 'F':
        case 'f':
        case 'm':
        case 'M':
          if (Context)
          {
            if (!*p && e_cur)
            {
              /* include the current message */
              p = tmp + mutt_str_strlen(tmp) + 1;
              snprintf(tmp + mutt_str_strlen(tmp),
                       sizeof(tmp) - mutt_str_strlen(tmp), " %d", e_cur->msgno + 1);
            }
            buf = be_include_messages(p, buf, &bufmax, &buflen, (tolower(tmp[1]) == 'm'),
                                      (isupper((unsigned char) tmp[1])));
          }
          else
            mutt_window_addstr(_("No mailbox.\n"));
          break;
        case 'p':
          mutt_window_addstr("-----\n");
          mutt_window_addstr(_("Message contains:\n"));
          be_print_header(e_new->env);
          for (int i = 0; i < buflen; i++)
            mutt_window_addstr(buf[i]);
          /* L10N: This entry is shown AFTER the message content,
             not IN the middle of the content.
             So it doesn't mean "(message will continue)"
             but means "(press any key to continue using neomutt)". */
          mutt_window_addstr(_("(continue)\n"));
          break;
        case 'q':
          done = true;
          break;
        case 'r':
          if (*p)
          {
            mutt_str_strfcpy(tmp, p, sizeof(tmp));
            mutt_expand_path(tmp, sizeof(tmp));
            buf = be_snarf_file(tmp, buf, &bufmax, &buflen, true);
          }
          else
            mutt_window_addstr(_("missing filename.\n"));
          break;
        case 's':
          mutt_str_replace(&e_new->env->subject, p);
          break;
        case 't':
          mutt_addrlist_parse(&e_new->env->to, p);
          mutt_expand_aliases(&e_new->env->to);
          break;
        case 'u':
          if (buflen)
          {
            buflen--;
            mutt_str_strfcpy(tmp, buf[buflen], sizeof(tmp));
            tmp[mutt_str_strlen(tmp) - 1] = '\0';
            FREE(&buf[buflen]);
            buf[buflen] = NULL;
            continue;
          }
          else
            mutt_window_addstr(_("No lines in message.\n"));
          break;

        case 'e':
        case 'v':
          if (be_barf_file(path, buf, buflen) == 0)
          {
            const char *tag = NULL;
            char *err = NULL;
            be_free_memory(buf, buflen);
            buf = NULL;
            bufmax = 0;
            buflen = 0;

            if (C_EditHeaders)
            {
              mutt_env_to_local(e_new->env);
              mutt_edit_headers(NONULL(C_Visual), path, e_new, NULL);
              if (mutt_env_to_intl(e_new->env, &tag, &err))
                mutt_window_printf(_("Bad IDN in '%s': '%s'"), tag, err);
              /* tag is a statically allocated string and should not be freed */
              FREE(&err);
            }
            else
              mutt_edit_file(NONULL(C_Visual), path);

            buf = be_snarf_file(path, buf, &bufmax, &buflen, false);

            mutt_window_addstr(_("(continue)\n"));
          }
          break;
        case 'w':
          be_barf_file((p[0] != '\0') ? p : path, buf, buflen);
          break;
        case 'x':
          abort = true;
          done = true;
          break;
        default:
          mutt_window_printf(_("%s: unknown editor command (~? for help)\n"), tmp);
          break;
      }
    }
    else if (mutt_str_strcmp(".", tmp) == 0)
      done = true;
    else
    {
      mutt_str_strcat(tmp, sizeof(tmp), "\n");
      if (buflen == bufmax)
        mutt_mem_realloc(&buf, sizeof(char *) * (bufmax += 25));
      buf[buflen++] = mutt_str_strdup((tmp[1] == '~') ? tmp + 1 : tmp);
    }

    tmp[0] = '\0';
  }

  if (!abort)
    be_barf_file(path, buf, buflen);
  be_free_memory(buf, buflen);

  return abort ? -1 : 0;
}
