/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include "mutt_curses.h"
#include "rfc2047.h"
#include "mx.h"
#include "mime.h"
#include "mailbox.h"
#include "copy.h"
#include "pager.h"
#include "charset.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else /* Make sure EX_OK is defined <philiph@pobox.com> */
#define EX_OK 0
#endif

extern char RFC822Specials[];

static struct sysexits
{
  int v;
  const char *str;
} 
sysexits_h[] = 
{
#ifdef EX_USAGE
  { 0xff & EX_USAGE, "Bad usage." },
#endif
#ifdef EX_DATAERR
  { 0xff & EX_DATAERR, "Data format error." },
#endif
#ifdef EX_NOINPUT
  { 0xff & EX_NOINPUT, "Cannot open input." },
#endif
#ifdef EX_NOUSER
  { 0xff & EX_NOUSER, "User unknown." },
#endif
#ifdef EX_NOHOST
  { 0xff & EX_NOHOST, "Host unknown." },
#endif
#ifdef EX_UNAVAILABLE
  { 0xff & EX_UNAVAILABLE, "Service unavailable." },
#endif
#ifdef EX_SOFTWARE
  { 0xff & EX_SOFTWARE, "Internal error." },
#endif
#ifdef EX_OSERR
  { 0xff & EX_OSERR, "Operating system error." },
#endif
#ifdef EX_OSFILE
  { 0xff & EX_OSFILE, "System file missing." },
#endif
#ifdef EX_CANTCREAT
  { 0xff & EX_CANTCREAT, "Can't create output." },
#endif
#ifdef EX_IOERR
  { 0xff & EX_IOERR, "I/O error." },
#endif
#ifdef EX_TEMPFAIL
  { 0xff & EX_TEMPFAIL, "Deferred." },
#endif
#ifdef EX_PROTOCOL
  { 0xff & EX_PROTOCOL, "Remote protocol error." },
#endif
#ifdef EX_NOPERM
  { 0xff & EX_NOPERM, "Insufficient permission." },
#endif
#ifdef EX_CONFIG
  { 0xff & EX_NOPERM, "Local configuration error." },
#endif
  { S_ERR, "Exec error." },
  { -1, NULL}
};
      
    
#ifdef _PGPPATH
#include "pgp.h"
#endif /* _PGPPATH */



#define DISPOSITION(X) X==DISPATTACH?"attachment":"inline"

const char MimeSpecials[] = "@.,;<>[]\\\"()?/=";

char B64Chars[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
  'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
  't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', '+', '/'
};

static char MsgIdPfx = 'A';

static void transform_to_7bit (BODY *a, FILE *fpin);

static void encode_quoted (FILE * fin, FILE *fout, int istext, CHARSET_MAP *map)
{
  int c, linelen = 0;
  char line[77], savechar;

  while ((c = fgetc (fin)) != EOF)
  {
    if(istext && map)
      c = mutt_display_char(c, map);

    /* Escape lines that begin with "the message separator". */
    if (linelen == 5 && !mutt_strncmp ("From ", line, 5))
    {
      strfcpy (line, "=46rom ", sizeof (line));
      linelen = 7;
    }
    else if (linelen == 1 && line[0] == '.')
    {
      strfcpy (line, "=2E", sizeof (line));
      linelen = 3;
    }

    /* Wrap the line if needed. */
    if (linelen == 76 && ((istext && c != '\n') || !istext))
    {
      /* If the last character is "quoted", then be sure to move all three
       * characters to the next line.  Otherwise, just move the last
       * character...
       */
      if (line[linelen-3] == '=')
      {
        line[linelen-3] = 0;
        fputs (line, fout);      
        fputs ("=\n", fout);
        line[linelen] = 0;
        line[0] = '=';
        line[1] = line[linelen-2];
        line[2] = line[linelen-1];
        linelen = 3;
      }
      else
      {
        savechar = line[linelen-1];
        line[linelen-1] = '=';
        line[linelen] = 0;
        fputs (line, fout);
        fputc ('\n', fout);
        line[0] = savechar;
        linelen = 1;
      }
    }

    if (c == '\n' && istext)
    {
      /* Check to make sure there is no trailing space on this line. */
      if (linelen > 0 && (line[linelen-1] == ' ' || line[linelen-1] == '\t'))
      {
        if (linelen < 74)
	{
          sprintf (line+linelen-1, "=%2.2X", (unsigned char) line[linelen-1]);
          fputs (line, fout);
        }
        else
	{
          int savechar = line[linelen-1];

          line[linelen-1] = '=';
          line[linelen] = 0;
          fputs (line, fout);
          fprintf (fout, "\n=%2.2X", (unsigned char) savechar);
        }
      }
      else
      {
        line[linelen] = 0;
        fputs (line, fout);
      }
      fputc ('\n', fout);
      linelen = 0;
    }
    else if (c != 9 && (c < 32 || c > 126 || c == '='))
    {
      /* Check to make sure there is enough room for the quoted character.
       * If not, wrap to the next line.
       */
      if (linelen > 73)
      {
        line[linelen++] = '=';
        line[linelen] = 0;
        fputs (line, fout);
        fputc ('\n', fout);
        linelen = 0;
      }
      sprintf (line+linelen,"=%2.2X", (unsigned char) c);
      linelen += 3;
    }
    else
    {
      /* Don't worry about wrapping the line here.  That will happen during
       * the next iteration when I'll also know what the next character is.
       */
      line[linelen++] = c;
    }
  }

  /* Take care of anything left in the buffer */
  if (linelen > 0)
  {
    if (line[linelen-1] == ' ' || line[linelen-1] == '\t')
    {
      /* take care of trailing whitespace */
      if (linelen < 74)
        sprintf (line+linelen-1, "=%2.2X", (unsigned char) line[linelen-1]);
      else
      {
        savechar = line[linelen-1];
        line[linelen-1] = '=';
        line[linelen] = 0;
        fputs (line, fout);
        fputc ('\n', fout);
        sprintf (line, "=%2.2X", (unsigned char) savechar);
      }
    }
    else
      line[linelen] = 0;
    fputs (line, fout);
  }
}

static char b64_buffer[3];
static short b64_num;
static short b64_linelen;

static void b64_flush(FILE *fout)
{
  short i;

  if(!b64_num)
    return;

  if(b64_linelen >= 72)
  {
    fputc('\n', fout);
    b64_linelen = 0;
  }
  
  for(i = b64_num; i < 3; i++)
    b64_buffer[i] = '\0';
  
  fputc(B64Chars[(b64_buffer[0] >> 2) & 0x3f], fout); 
  b64_linelen++;
  fputc(B64Chars[((b64_buffer[0] & 0x3) << 4) | ((b64_buffer[1] >> 4) & 0xf) ], fout);
  b64_linelen++;
  
  if(b64_num > 1)
  {
    fputc(B64Chars[((b64_buffer[1] & 0xf) << 2) | ((b64_buffer[2] >> 6) & 0x3) ], fout);
    b64_linelen++;
    if(b64_num > 2)
    {
      fputc(B64Chars[b64_buffer[2] & 0x3f], fout);
      b64_linelen++;
    }
  }
  
  while(b64_linelen % 4)
  {
    fputc('=', fout);
    b64_linelen++;
  }
  
  b64_num = 0;
}
    
  
static void b64_putc(char c, FILE *fout)
{
  if(b64_num == 3)
    b64_flush(fout);
  
  b64_buffer[b64_num++] = c;
}
  
  
static void encode_base64 (FILE * fin, FILE *fout, int istext, CHARSET_MAP *map)
{
  int ch, ch1 = EOF;
  
  b64_num = b64_linelen = 0;
  
  while((ch = fgetc(fin)) != EOF)
  {
    if(istext && map)
      ch = mutt_display_char(ch, map);

    if(istext && ch == '\n' && ch1 != '\r')
      b64_putc('\r', fout);
    b64_putc(ch, fout);
    ch1 = ch;
  }
  b64_flush(fout);
  fputc('\n', fout);
}

static void encode_8bit(FILE *fin, FILE *fout, int istext, CHARSET_MAP *map)
{
  int ch;

  if(!istext || !map)
  {
    mutt_copy_stream(fin, fout);
    return;
  }
  
  while((ch = fgetc(fin)) != EOF)
  {
    fputc(mutt_display_char(ch, map), fout);
  }
}
  

int mutt_write_mime_header (BODY *a, FILE *f)
{
  PARAMETER *p;
  char buffer[STRING];
  char *t;
  char *fn;
  int len;
  int tmplen;
  
  fprintf (f, "Content-Type: %s/%s", TYPE (a), a->subtype);

  if (a->parameter)
  {
    len = 25 + mutt_strlen (a->subtype); /* approximate len. of content-type */

    for(p = a->parameter; p; p = p->next)
    {
      
      if(!p->value)
	continue;
      
      fputc (';', f);

      buffer[0] = 0;
      rfc822_cat (buffer, sizeof (buffer), p->value, MimeSpecials);

      tmplen = mutt_strlen (buffer) + mutt_strlen (p->attribute) + 1;

      if (len + tmplen + 2 > 76)
      {
	fputs ("\n\t", f);
	len = tmplen + 8;
      }
      else
      {
	fputc (' ', f);
	len += tmplen + 1;
      }

      fprintf (f, "%s=%s", p->attribute, buffer);

    }
  }

  fputc ('\n', f);

  if (a->description)
    fprintf(f, "Content-Description: %s\n", a->description);

  if (a->use_disp && (a->disposition == DISPATTACH || a->filename || a->d_filename))
  {
    fprintf (f, "Content-Disposition: %s", DISPOSITION (a->disposition));

    if(!(fn = a->d_filename))
      fn = a->filename;
    
    if (fn)
    {
      /* Strip off the leading path... */
      if ((t = strrchr (fn, '/')))
	t++;
      else
	t = fn;
      
      buffer[0] = 0;
      rfc822_cat (buffer, sizeof (buffer), t, MimeSpecials);
      fprintf (f, "; filename=%s", buffer);
    }

    fputc ('\n', f);
  }

  if (a->encoding != ENC7BIT)
    fprintf(f, "Content-Transfer-Encoding: %s\n", ENCODING (a->encoding));

  /* Do NOT add the terminator here!!! */
  return (ferror (f) ? -1 : 0);
}

int mutt_write_mime_body (BODY *a, FILE *f)
{
  char *p, boundary[SHORT_STRING];
  char send_charset[SHORT_STRING];
  FILE *fpin;
  BODY *t;
  CHARSET_MAP *map = NULL;
  
  if (a->type == TYPEMULTIPART)
  {
    /* First, find the boundary to use */
    if (!(p = mutt_get_parameter ("boundary", a->parameter)))
    {
      dprint (1, (debugfile, "mutt_write_mime_body(): no boundary parameter found!\n"));
      mutt_error _("No boundary parameter found! [report this error]");
      return (-1);
    }
    strfcpy (boundary, p, sizeof (boundary));

    for (t = a->parts; t ; t = t->next)
    {
      fprintf (f, "\n--%s\n", boundary);
      if (mutt_write_mime_header (t, f) == -1)
	return -1;
      fputc ('\n', f);
      if (mutt_write_mime_body (t, f) == -1)
	return -1;
    }
    fprintf (f, "\n--%s--\n", boundary);
    return (ferror (f) ? -1 : 0);
  }



#ifdef _PGPPATH
  /* This is pretty gross, but it's the best solution for now... */
  if (a->type == TYPEAPPLICATION && mutt_strcmp (a->subtype, "pgp-encrypted") == 0)
  {
    fputs ("Version: 1\n", f);
    return 0;
  }
#endif /* _PGPPATH */



  if ((fpin = fopen (a->filename, "r")) == NULL)
  {
    dprint(1,(debugfile, "write_mime_body: %s no longer exists!\n",a->filename));
    mutt_error (_("%s no longer exists!"), a->filename);
    return -1;
  }

  if (a->type == TYPETEXT)
    map = mutt_get_translation (Charset, mutt_get_send_charset (send_charset, sizeof(send_charset), a, 1));

  if (a->encoding == ENCQUOTEDPRINTABLE)
    encode_quoted (fpin, f, mutt_is_text_type (a->type, a->subtype), 
		   a->type == TYPETEXT && (!a->noconv) ? map : NULL);
  else if (a->encoding == ENCBASE64)
    encode_base64 (fpin, f, mutt_is_text_type (a->type, a->subtype), 
		   a->type == TYPETEXT && (!a->noconv) ? map : NULL);
  else
    encode_8bit (fpin, f, mutt_is_text_type (a->type, a->subtype),
		      a->type == TYPETEXT && (!a->noconv) ? map : NULL);

  fclose (fpin);

  return (ferror (f) ? -1 : 0);
}

#define BOUNDARYLEN 16
void mutt_generate_boundary (PARAMETER **parm)
{
  char rs[BOUNDARYLEN + 1];
  char *p = rs;
  int i;

  rs[BOUNDARYLEN] = 0;
  for (i=0;i<BOUNDARYLEN;i++) 
    *p++ = B64Chars[LRAND() % sizeof (B64Chars)];
  *p = 0;
  
  mutt_set_parameter ("boundary", rs, parm);
}

/* analyze the contents of a file to determine which MIME encoding to use */
static CONTENT *mutt_get_content_info (const char *fname, BODY *b)
{
  CONTENT *info;
  FILE *fp;
  int ch, from=0, whitespace=0, dot=0, linelen=0;

  if(b && !fname) fname = b->filename;
  
  if ((fp = fopen (fname, "r")) == NULL)
  {
    dprint (1, (debugfile, "mutt_get_content_info: %s: %s (errno %d).\n",
		fname, strerror (errno), errno));
    return (NULL);
  }

  info = safe_calloc (1, sizeof (CONTENT));
  while ((ch = fgetc (fp)) != EOF)
  {
    linelen++;
    if (ch == '\n')
    {
      if (whitespace) info->space = 1;
      if (dot) info->dot = 1;
      if (linelen > info->linemax) info->linemax = linelen;
      whitespace = 0;
      linelen = 0;
      dot = 0;
    }
    else if (ch == '\r')
    {
      if ((ch = fgetc (fp)) == EOF)
      {
        info->binary = 1;
        break;
      }
      else if (ch != '\n')
      {
        info->binary = 1;
	ungetc (ch, fp);
	continue;
      }
      else
      {
        if (whitespace) info->space = 1;
	if (dot) info->dot = 1;
        if (linelen > info->linemax) info->linemax = linelen;
        whitespace = 0;
	dot = 0;
        linelen = 0;
      }
    }
    else if (ch & 0x80)
      info->hibin++;
    else if (ch == '\t' || ch == '\f')
    {
      info->ascii++;
      whitespace++;
    }
    else if (ch < 32 || ch == 127)
      info->lobin++;
    else
    {
      if (linelen == 1)
      {
        if (ch == 'F')
          from = 1;
        else
          from = 0;
        if (ch == '.')
          dot = 1;
        else
          dot = 0;
      }
      else if (from)
      {
        if (linelen == 2 && ch != 'r') from = 0;
        else if (linelen == 3 && ch != 'o') from = 0;
        else if (linelen == 4)
	{
          if (ch == 'm') info->from = 1;
          from = 0;
        }
      }
      if (ch == ' ') whitespace++;
      info->ascii++;
    }

    if (linelen > 1) dot = 0;
    if (ch != ' ' && ch != '\t') whitespace = 0;
  }
  fclose (fp);
  return (info);
}

/* Given a file with path ``s'', see if there is a registered MIME type.
 * returns the major MIME type, and copies the subtype to ``d''.  First look
 * for ~/.mime.types, then look in a system mime.types if we can find one.
 * The longest match is used so that we can match `ps.gz' when `gz' also
 * exists.
 */

static int lookup_mime_type (char *d, const char *s)
{
  FILE *f;
  char *p, *ct,
  buf[LONG_STRING];
  int count;
  int szf, sze, cur_n, cur_sze;

  *d = 0;
  cur_n = TYPEOTHER;
  cur_sze = 0;
  szf = mutt_strlen (s);

  for (count = 0 ; count < 3 ; count++)
  {
    /*
     * can't use strtok() because we use it in an inner loop below, so use
     * a switch statement here instead.
     */
    switch (count)
    {
      case 0:
	snprintf (buf, sizeof (buf), "%s/.mime.types", NONULL(Homedir));
	break;
      case 1:
        strfcpy (buf, SYSCONFDIR"/mime.types", sizeof(buf));
        break;
      case 2:
	strfcpy (buf, SHAREDIR"/mime.types", sizeof (buf));
	break;
      default:
	return (cur_n);
    }

    if ((f = fopen (buf, "r")) != NULL)
    {
      while (fgets (buf, sizeof (buf) - 1, f) != NULL)
      {
	/* weed out any comments */
	if ((p = strchr (buf, '#')))
	  *p = 0;

	/* remove any leading space. */
	ct = buf;
	SKIPWS (ct);

	/* position on the next field in this line */
	if ((p = strpbrk (ct, " \t")) == NULL)
	  continue;
	*p++ = 0;
	SKIPWS (p);

	/* cycle through the file extensions */
	while ((p = strtok (p, " \t\n")))
	{
	  sze = mutt_strlen (p);
	  if ((sze > cur_sze) && (szf >= sze) &&
	      mutt_strcasecmp (s + szf - sze, p) == 0 &&
	      (szf == sze || s[szf - sze - 1] == '.'))
	  {
	    char *dc;

	    /* get the content-type */

	    if ((p = strchr (ct, '/')) == NULL)
	    {
	      /* malformed line, just skip it. */
	      break;
	    }
	    *p++ = 0;

	    dc = d;
	    while (*p && !ISSPACE (*p))
	      *dc++ = *p++;
	    *dc = 0;

	    cur_n = mutt_check_mime_type (ct);
	    cur_sze = sze;
	  }
	  p = NULL;
	}
      }
      fclose (f);
    }
  }
  return (cur_n);
}

void mutt_message_to_7bit (BODY *a, FILE *fp)
{
  char temp[_POSIX_PATH_MAX];
  char *line = NULL;
  FILE *fpin = NULL;
  FILE *fpout = NULL;
  struct stat sb;

  if (!a->filename && fp)
    fpin = fp;
  else if (!a->filename || !(fpin = fopen (a->filename, "r")))
  {
    mutt_error (_("Could not open %s"), a->filename ? a->filename : "(null)");
    return;
  } 
  else
  {
    a->offset = 0;
    if (stat (a->filename, &sb) == -1)
    {
      mutt_perror ("stat");
      fclose (fpin);
    }
    a->length = sb.st_size;
  }

  mutt_mktemp (temp);
  if (!(fpout = safe_fopen (temp, "w+")))
  {
    mutt_perror ("fopen");
    goto cleanup;
  }

  fseek (fpin, a->offset, 0);
  a->parts = mutt_parse_messageRFC822 (fpin, a);

  transform_to_7bit (a->parts, fpin);
  
  mutt_copy_hdr (fpin, fpout, a->offset, a->offset + a->length, 
		 CH_MIME | CH_NONEWLINE | CH_XMIT, NULL);

  fputs ("Mime-Version: 1.0\n", fpout);
  mutt_write_mime_header (a->parts, fpout);
  fputc ('\n', fpout);
  mutt_write_mime_body (a->parts, fpout);
  
 cleanup:
  safe_free ((void **) &line);

  if (fpin && !fp)
    fclose (fpin);
  if (fpout)
    fclose (fpout);
  else
    return;
  
  a->encoding = ENC7BIT;
  a->d_filename = a->filename;
  if (a->filename && a->unlink)
    unlink (a->filename);
  a->filename = safe_strdup (temp);
  a->unlink = 1;
  if(stat (a->filename, &sb) == -1) 
  {
    mutt_perror ("stat");
    return;
  }
  a->length = sb.st_size;
  mutt_free_body (&a->parts);
  a->hdr->content = NULL;
}

static void transform_to_7bit (BODY *a, FILE *fpin)
{
  char buff[_POSIX_PATH_MAX];
  STATE s;
  struct stat sb;

  memset (&s, 0, sizeof (s));
  for (; a; a = a->next)
  {
    if (a->type == TYPEMULTIPART)
    {
      if (a->encoding != ENC7BIT)
	a->encoding = ENC7BIT;

      transform_to_7bit (a->parts, fpin);
    } 
    else if (mutt_is_message_type(a->type, a->subtype))
    {
      mutt_message_to_7bit (a, fpin);
    }
    else 
    {
      mutt_mktemp (buff);
      if ((s.fpout = safe_fopen (buff, "w")) == NULL) 
      {
	mutt_perror ("fopen");
	return;
      }
      s.fpin = fpin;
      mutt_decode_attachment (a, &s);
      fclose (s.fpout);
      a->d_filename = a->filename;
      a->filename = safe_strdup (buff);
      a->unlink = 1;
      if (stat (a->filename, &sb) == -1) 
      {
	mutt_perror ("stat");
	return;
      }
      a->length = sb.st_size;

      mutt_update_encoding (a);
      if (a->encoding == ENC8BIT)
	a->encoding = ENCQUOTEDPRINTABLE;
      else if(a->encoding == ENCBINARY)
	a->encoding = ENCBASE64;
    }
  }
}

static const char *get_text_charset (BODY *b, CONTENT *info)
{
  char send_charset[SHORT_STRING];
  char *chsname;

  chsname = mutt_get_send_charset (send_charset, sizeof (send_charset), b, 1);
  
  /* if charset is unknown assume low bytes are ascii compatible */

  if ((chsname == NULL || mutt_strcasecmp (chsname, "us-ascii") == 0)
      && info->hibin)
    return ("unknown-8bit");

  if (info->hibin)
    return (chsname);

  return ("us-ascii");
}

/* determine which Content-Transfer-Encoding to use */
static void mutt_set_encoding (BODY *b, CONTENT *info)
{
  if (b->type == TYPETEXT)
  {
    if (info->lobin || (info->from && option (OPTENCODEFROM)))
      b->encoding = ENCQUOTEDPRINTABLE;
    else if (info->hibin)
      b->encoding = option (OPTALLOW8BIT) ? ENC8BIT : ENCQUOTEDPRINTABLE;
    else
      b->encoding = ENC7BIT;
  }
  else if (b->type == TYPEMESSAGE || b->type == TYPEMULTIPART)
  {
    if (info->lobin || info->hibin)
    {
      if (option (OPTALLOW8BIT) && !info->lobin)
	b->encoding = ENC8BIT; 
      else
	mutt_message_to_7bit (b, NULL);
    }
    else
      b->encoding = ENC7BIT;
  }
  else if (info->lobin || info->hibin || info->binary || info->linemax > 990
	   || (option (OPTENCODEFROM) && info->from))
  {
    /* Determine which encoding is smaller  */
    if (1.33 * (float)(info->lobin+info->hibin+info->ascii) < 3.0 * (float) (info->lobin + info->hibin) + (float)info->ascii)
      b->encoding = ENCBASE64;
    else
      b->encoding = ENCQUOTEDPRINTABLE;
  }
  else
    b->encoding = ENC7BIT;
}

void mutt_stamp_attachment(BODY *a)
{
  a->stamp = time(NULL);
}

/* Get the character set which is to be used for sending */

char *mutt_get_send_charset (char *d, size_t dlen, BODY *b, short f)
{
  char *p = NULL;

  if (b && b->type != TYPETEXT)
    return NULL;

  if (b) 
    p = mutt_get_parameter ("charset", b->parameter);

  /* override the special "us-ascii" and "unknown-8bit" character sets */
  if (!p || (f && (!mutt_strcasecmp (p, "us-ascii") || !mutt_strcasecmp (p, "unknown-8bit"))))
  {
    if (SendCharset && *SendCharset)
      p = SendCharset;
    else if (Charset)
      p = Charset;
  }

  if (p)
  {
    strfcpy (d, NONULL(p), dlen);
    return d;
  }

  /* something is seriously wrong. */
  return NULL;
}

/* set a body structure's character set */

void mutt_set_body_charset(BODY *b, const char *chs)
{
  char send_charset[SHORT_STRING];
  
  if(b->type != TYPETEXT)
    return;

  if(!chs && !(chs = mutt_get_send_charset(send_charset, sizeof(send_charset), NULL, 1)))
    return;

  mutt_set_parameter ("charset", chs, &b->parameter);
}


/* Assumes called from send mode where BODY->filename points to actual file */
void mutt_update_encoding (BODY *a)
{
  CONTENT *info;

  if ((info = mutt_get_content_info (a->filename, a)) == NULL)
    return;

  mutt_set_encoding (a, info);
  mutt_stamp_attachment(a);
  
  if (a->type == TYPETEXT)
    mutt_set_body_charset(a, get_text_charset(a, info));

#ifdef _PGPPATH
  /* save the info in case this message is signed.  we will want to do Q-P
   * encoding if any lines begin with "From " so the signature won't be munged,
   * for example.
   */ 
  safe_free ((void **) &a->content);
  a->content = info;
  info = NULL;
#endif



  safe_free ((void **) &info);
}

BODY *mutt_make_message_attach (CONTEXT *ctx, HEADER *hdr, int attach_msg)
{
  char buffer[LONG_STRING];
  BODY *body;
  FILE *fp;
  int cmflags, chflags;
#ifdef _PGPPATH
  int pgp = hdr->pgp;
#endif

#ifdef _PGPPATH
  if ((option(OPTMIMEFORWDECODE) || option(OPTFORWDECRYPT)) &&
      (hdr->pgp & PGPENCRYPT) && !pgp_valid_passphrase())
    return (NULL);
#endif /* _PGPPATH */

  mutt_mktemp (buffer);
  if ((fp = safe_fopen (buffer, "w+")) == NULL)
    return NULL;

  body = mutt_new_body ();
  body->type = TYPEMESSAGE;
  body->subtype = safe_strdup ("rfc822");
  body->filename = safe_strdup (buffer);
  body->unlink = 1;
  body->use_disp = 0;

  mutt_parse_mime_message (ctx, hdr);

  chflags = CH_XMIT;
  cmflags = 0;

  /* If we are attaching a message, ignore OPTMIMEFORWDECODE */
  if (!attach_msg && option (OPTMIMEFORWDECODE))
  {
    chflags |= CH_MIME | CH_TXTPLAIN;
    cmflags = M_CM_DECODE | M_CM_CHARCONV;
#ifdef _PGPPATH
    pgp &= ~PGPENCRYPT;
#endif
  }
#ifdef _PGPPATH
  else
    if (option (OPTFORWDECRYPT)
       && (hdr->pgp & PGPENCRYPT))
  {
    if (mutt_is_multipart_encrypted (hdr->content))
    {
      chflags |= CH_MIME | CH_NONEWLINE;
      cmflags = M_CM_DECODE_PGP;
      pgp &= ~PGPENCRYPT;
    }
    else if (mutt_is_application_pgp (hdr->content) & PGPENCRYPT)
    {
      chflags |= CH_MIME | CH_TXTPLAIN;
      cmflags = M_CM_DECODE | M_CM_CHARCONV;
      pgp &= ~PGPENCRYPT;
    }
  }
#endif

  mutt_copy_message (fp, ctx, hdr, cmflags, chflags);
  
  fflush(fp);
  rewind(fp);

  body->hdr = mutt_new_header();
  body->hdr->offset = 0;
  /* we don't need the user headers here */
  body->hdr->env = mutt_read_rfc822_header(fp, body->hdr, 0);
#ifdef _PGPPATH
  body->hdr->pgp = pgp;
#endif /* _PGPPATH */
  mutt_update_encoding (body);
  body->parts = body->hdr->content;

  fclose(fp);
  
  return (body);
}

BODY *mutt_make_file_attach (const char *path)
{
  BODY *att;
  CONTENT *info;
  char buf[SHORT_STRING];
  int n;
  
  if ((info = mutt_get_content_info (path, NULL)) == NULL)
    return NULL;

  att = mutt_new_body ();
  att->filename = safe_strdup (path);

  /* Attempt to determine the appropriate content-type based on the filename
   * suffix.
   */

  if ((n = lookup_mime_type (buf, path)) != TYPEOTHER)
  {
    att->type = n;
    att->subtype = safe_strdup (buf);
  }

  if (!att->subtype)
  {
    if (info->lobin == 0 || (info->lobin + info->hibin + info->ascii)/ info->lobin >= 10)
    {
      /*
       * Statistically speaking, there should be more than 10% "lobin" 
       * chars if this is really a binary file...
       */
      att->type = TYPETEXT;
      att->subtype = safe_strdup ("plain");
    }
    else
    {
      att->type = TYPEAPPLICATION;
      att->subtype = safe_strdup ("octet-stream");
    }
  } 

  /* XXX - just call mutt_update_encoding? -tlr */

  mutt_set_encoding (att, info);
  mutt_stamp_attachment(att);

  if (att->type == TYPETEXT)
    mutt_set_body_charset(att, get_text_charset(att, info));
  
#ifdef _PGPPATH
  /*
   * save the info in case this message is signed.  we will want to do Q-P
   * encoding if any lines begin with "From " so the signature won't be munged,
   * for example.
   */
  att->content = info;
  info = NULL;
#endif

  safe_free ((void **) &info);

  return (att);
}

static int get_toplevel_encoding (BODY *a)
{
  int e = ENC7BIT;

  for (; a; a = a->next)
  {
    if (a->encoding == ENCBINARY)
      return (ENCBINARY);
    else if (a->encoding == ENC8BIT)
      e = ENC8BIT;
  }

  return (e);
}

BODY *mutt_make_multipart (BODY *b)
{
  BODY *new;

  new = mutt_new_body ();
  new->type = TYPEMULTIPART;
  new->subtype = safe_strdup ("mixed");
  new->encoding = get_toplevel_encoding (b);
  mutt_generate_boundary (&new->parameter);
  new->use_disp = 0;  
  new->parts = b;

  return new;
}

/* remove the multipart body if it exists */
BODY *mutt_remove_multipart (BODY *b)
{
  BODY *t;

  if (b->parts)
  {
    t = b;
    b = b->parts;
    t->parts = NULL;
    mutt_free_body (&t);
  }
  return b;
}

char *mutt_make_date (char *s, size_t len)
{
  time_t t = time (NULL);
  struct tm *l = localtime (&t);
  time_t tz = mutt_local_tz (t);

  tz /= 60;

  snprintf (s, len,  "Date: %s, %d %s %d %02d:%02d:%02d %+03d%02d\n",
	    Weekdays[l->tm_wday], l->tm_mday, Months[l->tm_mon],
	    l->tm_year + 1900, l->tm_hour, l->tm_min, l->tm_sec,
	    tz / 60, abs (tz) % 60);
  return (s);
}

/* wrapper around mutt_write_address() so we can handle very large
   recipient lists without needing a huge temporary buffer in memory */
void mutt_write_address_list (ADDRESS *adr, FILE *fp, int linelen)
{
  ADDRESS *tmp;
  char buf[LONG_STRING];
  int count = 0;
  int len;

  while (adr)
  {
    tmp = adr->next;
    adr->next = NULL;
    buf[0] = 0;
    rfc822_write_address (buf, sizeof (buf), adr);
    len = mutt_strlen (buf);
    if (count && linelen + len > 74)
    {
      if (count)
      {
	fputs ("\n\t", fp);
	linelen = len + 8; /* tab is usually about 8 spaces... */
      }
    }
    else
    {
      if (count && adr->mailbox)
      {
	fputc (' ', fp);
	linelen++;
      }
      linelen += len;
    }
    fputs (buf, fp);
    adr->next = tmp;
    if (!adr->group && adr->next && adr->next->mailbox)
    {
      linelen++;
      fputc (',', fp);
    }
    adr = adr->next;
    count++;
  }
  fputc ('\n', fp);
}

/* arbitrary number of elements to grow the array by */
#define REF_INC 16

#define TrimRef 10

/* need to write the list in reverse because they are stored in reverse order
 * when parsed to speed up threading
 */
static void write_references (LIST *r, FILE *f)
{
  LIST **ref = NULL;
  int refcnt = 0, refmax = 0;

  for ( ; (TrimRef == 0 || refcnt < TrimRef) && r ; r = r->next)
  {
    if (refcnt == refmax)
      safe_realloc ((void **) &ref, (refmax += REF_INC) * sizeof (LIST *));
    ref[refcnt++] = r;
  }

  while (refcnt-- > 0)
  {
    fputc (' ', f);
    fputs (ref[refcnt]->data, f);
  }

  safe_free ((void **) &ref);
}

/* Note: all RFC2047 encoding should be done outside of this routine, except
 * for the "real name."  This will allow this routine to be used more than
 * once, if necessary.
 *
 * mode == 1  => "lite" mode (used for edit_hdrs)
 * mode == 0  => normal mode.  write full header + MIME headers
 * mode == -1 => write just the envelope info (used for postponing messages)
 * 
 * privacy != 0 => will omit any headers which may identify the user.
 *               Output generated is suitable for being sent through
 * 		 anonymous remailer chains.
 * 
 */

int mutt_write_rfc822_header (FILE *fp, ENVELOPE *env, BODY *attach, 
			      int mode, int privacy)
{
  char buffer[LONG_STRING];
  LIST *tmp = env->userhdrs;

  if (option(OPTUSEHEADERDATE) && !privacy)
  {
    if(env->date)
      fprintf(fp, "Date: %s\n", env->date);
    else
      fputs (mutt_make_date(buffer, sizeof(buffer)), fp);
  }
  else if (mode == 0 && !privacy)
    fputs (mutt_make_date (buffer, sizeof(buffer)), fp);



  /* OPTUSEFROM is not consulted here so that we can still write a From:
   * field if the user sets it with the `my_hdr' command
   */
  if (env->from && !privacy)
  {
    buffer[0] = 0;
    rfc822_write_address (buffer, sizeof (buffer), env->from);
    fprintf (fp, "From: %s\n", buffer);
  }

  if (env->to)
  {
    fputs ("To: ", fp);
    mutt_write_address_list (env->to, fp, 4);
  }
  else if (mode > 0)
    fputs ("To: \n", fp);

  if (env->cc)
  {
    fputs ("Cc: ", fp);
    mutt_write_address_list (env->cc, fp, 4);
  }
  else if (mode > 0)
    fputs ("Cc: \n", fp);

  if (env->bcc)
  {
    if(mode != 0 || option(OPTWRITEBCC))
    {
      fputs ("Bcc: ", fp);
      mutt_write_address_list (env->bcc, fp, 5);
    }
  }
  else if (mode > 0)
    fputs ("Bcc: \n", fp);

  if (env->subject)
    fprintf (fp, "Subject: %s\n", env->subject);
  else if (mode == 1)
    fputs ("Subject: \n", fp);

  /* save message id if the user has set it */
  if (env->message_id && !privacy)
    fprintf (fp, "Message-ID: %s\n", env->message_id);

  if (env->reply_to)
  {
    fputs ("Reply-To: ", fp);
    mutt_write_address_list (env->reply_to, fp, 10);
  }
  else if (mode > 0)
    fputs ("Reply-To: \n", fp);

  if (env->mail_followup_to)
  {
    fputs ("Mail-Followup-To: ", fp);
    mutt_write_address_list (env->mail_followup_to, fp, 18);
  }

  if (mode <= 0)
  {
    if (env->references)
    {
      fputs ("References:", fp);
      write_references (env->references, fp);
      fputc('\n', fp);
    }

    /* Add the MIME headers */
    fputs ("Mime-Version: 1.0\n", fp);
    mutt_write_mime_header (attach, fp);
  }

  if (mode == 0 && !privacy && option (OPTXMAILER))
  {
    /* Add a vanity header */
    fprintf (fp, "User-Agent: Mutt/%s\n", MUTT_VERSION);
  }

  /* Add any user defined headers */
  for (; tmp; tmp = tmp->next)
  {
    fputs (tmp->data, fp);
    fputc ('\n', fp);
  }

  return (ferror (fp) == 0 ? 0 : -1);
}

static void encode_headers (LIST *h)
{
  char tmp[LONG_STRING];
  char *p;
  size_t len;

  for (; h; h = h->next)
  {
    if ((p = strchr (h->data, ':')))
    {
      *p++ = 0;
      SKIPWS (p);
      snprintf (tmp, sizeof (tmp), "%s: ", h->data);
      len = mutt_strlen (tmp);
      rfc2047_encode_string (tmp + len, sizeof (tmp) - len, (unsigned char *) p);
      safe_free ((void **) &h->data);
      h->data = safe_strdup (tmp);
    }
  }
}

const char *mutt_fqdn(short may_hide_host)
{
  char *p = NULL, *q;
  
  if(Fqdn && Fqdn[0] != '@')
  {
    p = Fqdn;
    
    if(may_hide_host && option(OPTHIDDENHOST))
    {
      if((p = strchr(Fqdn, '.')))
	p++;

      /* sanity check: don't hide the host if
       * the fqdn is something like detebe.org.
       */
      
      if(!p || !(q = strchr(p, '.')))
	p = Fqdn;
    }
  }

  return p;
}

char *mutt_gen_msgid (void)
{
  char buf[SHORT_STRING];
  time_t now;
  struct tm *tm;
  const char *fqdn;

  now = time (NULL);
  tm = localtime (&now);
  if(!(fqdn = mutt_fqdn(0)))
    fqdn = NONULL(Hostname);

  snprintf (buf, sizeof (buf), "<%d%02d%02d%02d%02d%02d.%c%d@%s>",
	    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
	    tm->tm_min, tm->tm_sec, MsgIdPfx, getpid (), fqdn);
  MsgIdPfx = (MsgIdPfx == 'Z') ? 'A' : MsgIdPfx + 1;
  return (safe_strdup (buf));
}

static RETSIGTYPE alarm_handler (int sig)
{
  SigAlrm = 1;
}

/* invoke sendmail in a subshell
   path	(in)		path to program to execute
   args	(in)		arguments to pass to program
   msg (in)		temp file containing message to send
   tempfile (out)	if sendmail is put in the background, this points
   			to the temporary file containing the stdout of the
			child process */
static int
send_msg (const char *path, char **args, const char *msg, char **tempfile)
{
  sigset_t set;
  int fd, st;
  pid_t pid, ppid;

  mutt_block_signals_system ();

  sigemptyset (&set);
  /* we also don't want to be stopped right now */
  sigaddset (&set, SIGTSTP);
  sigprocmask (SIG_BLOCK, &set, NULL);

  if (SendmailWait >= 0)
  {
    char tmp[_POSIX_PATH_MAX];

    mutt_mktemp (tmp);
    *tempfile = safe_strdup (tmp);
  }

  if ((pid = fork ()) == 0)
  {
    struct sigaction act, oldalrm;

    /* save parent's ID before setsid() */
    ppid = getppid ();

    /* we want the delivery to continue even after the main process dies,
     * so we put ourselves into another session right away
     */
    setsid ();
  
    /* next we close all open files */
#if defined(OPEN_MAX)
    for (fd = 0; fd < OPEN_MAX; fd++)
      close (fd);
#elif defined(_POSIX_OPEN_MAX)
    for (fd = 0; fd < _POSIX_OPEN_MAX; fd++)
      close (fd);
#else
    close (0);
    close (1);
    close (2);
#endif

    /* now the second fork() */
    if ((pid = fork ()) == 0)
    {
      /* "msg" will be opened as stdin */
      if (open (msg, O_RDONLY, 0) < 0)
      {
	unlink (msg);
	_exit (S_ERR);
      }
      unlink (msg);

      if (SendmailWait >= 0)
      {
	/* *tempfile will be opened as stdout */
	if (open (*tempfile, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0600) < 0)
	  _exit (S_ERR);
	/* redirect stderr to *tempfile too */
	if (dup (1) < 0)
	  _exit (S_ERR);
      }

      execv (path, args);
      _exit (S_ERR);
    }
    else if (pid == -1)
    {
      unlink (msg);
      safe_free ((void **) tempfile);
      _exit (S_ERR);
    }

    /* SendmailWait > 0: interrupt waitpid() after SendmailWait seconds
     * SendmailWait = 0: wait forever
     * SendmailWait < 0: don't wait
     */
    if (SendmailWait > 0)
    {
      SigAlrm = 0;
      act.sa_handler = alarm_handler;
#ifdef SA_INTERRUPT
      /* need to make sure waitpid() is interrupted on SIGALRM */
      act.sa_flags = SA_INTERRUPT;
#else
      act.sa_flags = 0;
#endif
      sigemptyset (&act.sa_mask);
      sigaction (SIGALRM, &act, &oldalrm);
      alarm (SendmailWait);
    }
    else if (SendmailWait < 0)
      _exit (0xff & EX_OK);

    if (waitpid (pid, &st, 0) > 0)
    {
      st = WIFEXITED (st) ? WEXITSTATUS (st) : S_ERR;
      if (SendmailWait && st == (0xff & EX_OK))
      {
	unlink (*tempfile); /* no longer needed */
	safe_free ((void **) tempfile);
      }
    }
    else
    {
      st = (SendmailWait > 0 && errno == EINTR && SigAlrm) ?
	      S_BKG : S_ERR;
      if (SendmailWait > 0)
      {
	unlink (*tempfile);
	safe_free ((void **) tempfile);
      }
    }

    /* reset alarm; not really needed, but... */
    alarm (0);
    sigaction (SIGALRM, &oldalrm, NULL);

    if (kill (ppid, 0) == -1 && errno == ESRCH)
    {
      /* the parent is already dead */
      unlink (*tempfile);
      safe_free ((void **) tempfile);
    }

    _exit (st);
  }

  sigprocmask (SIG_UNBLOCK, &set, NULL);

  if (pid != -1 && waitpid (pid, &st, 0) > 0)
    st = WIFEXITED (st) ? WEXITSTATUS (st) : S_ERR; /* return child status */
  else
    st = S_ERR;	/* error */

  mutt_unblock_signals_system (1);

  return (st);
}

static char **
add_args (char **args, size_t *argslen, size_t *argsmax, ADDRESS *addr)
{
  for (; addr; addr = addr->next)
  {
    /* weed out group mailboxes, since those are for display only */
    if (addr->mailbox && !addr->group)
    {
      if (*argslen == *argsmax)
	safe_realloc ((void **) &args, (*argsmax += 5) * sizeof (char *));
      args[(*argslen)++] = addr->mailbox;
    }
  }
  return (args);
}

static char **
add_option (char **args, size_t *argslen, size_t *argsmax, char *s)
{
  if (*argslen == *argsmax)
    safe_realloc ((void **) &args, (*argsmax += 5) * sizeof (char *));
  args[(*argslen)++] = s;
  return (args);
}

static const char *
strsysexit(int e)
{
  int i;
  
  for(i = 0; sysexits_h[i].str; i++)
  {
    if(e == sysexits_h[i].v)
      break;
  }
  
  return sysexits_h[i].str;
}


int
mutt_invoke_sendmail (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc, /* recips */
		 const char *msg, /* file containing message */
		 int eightbit) /* message contains 8bit chars */
{
  char *ps = NULL, *path = NULL, *s = safe_strdup (Sendmail), *childout = NULL;
  char **args = NULL;
  size_t argslen = 0, argsmax = 0;
  int i;

  ps = s;
  i = 0;
  while ((ps = strtok (ps, " ")))
  {
    if (argslen == argsmax)
      safe_realloc ((void **) &args, sizeof (char *) * (argsmax += 5));

    if (i)
      args[argslen++] = ps;
    else
    {
      path = safe_strdup (ps);
      ps = strrchr (ps, '/');
      if (ps)
	ps++;
      else
	ps = path;
      args[argslen++] = ps;
    }
    ps = NULL;
    i++;
  }
  if (eightbit && option (OPTUSE8BITMIME))
    args = add_option (args, &argslen, &argsmax, "-B8BITMIME");
  if (DsnNotify)
  {
    args = add_option (args, &argslen, &argsmax, "-N");
    args = add_option (args, &argslen, &argsmax, DsnNotify);
  }
  if (DsnReturn)
  {
    args = add_option (args, &argslen, &argsmax, "-R");
    args = add_option (args, &argslen, &argsmax, DsnReturn);
  }
  args = add_option (args, &argslen, &argsmax, "--");
  args = add_args (args, &argslen, &argsmax, to);
  args = add_args (args, &argslen, &argsmax, cc);
  args = add_args (args, &argslen, &argsmax, bcc);

  if (argslen == argsmax)
    safe_realloc ((void **) &args, sizeof (char *) * (++argsmax));
  
  args[argslen++] = NULL;

  if ((i = send_msg (path, args, msg, &childout)) != (EX_OK & 0xff))
  {
    if (i != S_BKG)
    {
      const char *e = strsysexit (i);

      e = strsysexit (i);
      mutt_error (_("Error sending message, child exited %d (%s)."), i, NONULL (e));
      if (childout)
      {
	struct stat st;
	
	if (stat (childout, &st) == 0 && st.st_size > 0)
	  mutt_do_pager (_("Output of the delivery process"), childout, 0, NULL);
      }
    }
  }
  else
    unlink (childout);

  FREE (&childout);
  FREE (&path);
  FREE (&s);
  FREE (&args);

  if (i == (EX_OK & 0xff))
    i = 0;
  else if (i == S_BKG)
    i = 1;
  else
    i = -1;
  return (i);
}

/* appends string 'b' to string 'a', and returns the pointer to the new
   string. */
char *mutt_append_string (char *a, const char *b)
{
  size_t la = mutt_strlen (a);
  safe_realloc ((void **) &a, la + mutt_strlen (b) + 1);
  strcpy (a + la, b);
  return (a);
}

/* returns 1 if char `c' needs to be quoted to protect from shell
   interpretation when executing commands in a subshell */
#define INVALID_CHAR(c) (!isalnum ((unsigned char)c) && !strchr ("@.+-_,:", c))

/* returns 1 if string `s' contains characters which could cause problems
   when used on a command line to execute a command */
int mutt_needs_quote (const char *s)
{
  while (*s)
  {
    if (INVALID_CHAR (*s))
      return 1;
    s++;
  }
  return 0;
}

/* Quote a string to prevent shell escapes when this string is used on the
   command line to send mail. */
char *mutt_quote_string (const char *s)
{
  char *r, *pr;
  size_t rlen;

  rlen = mutt_strlen (s) + 3;
  pr = r = (char *) safe_malloc (rlen);
  *pr++ = '"';
  while (*s)
  {
    if (INVALID_CHAR (*s))
    {
      size_t o = pr - r;
      safe_realloc ((void **) &r, ++rlen);
      pr = r + o;
      *pr++ = '\\';
    }
    *pr++ = *s++;
  }
  *pr++ = '"';
  *pr = 0;
  return (r);
}

void mutt_prepare_envelope (ENVELOPE *env)
{
  char buffer[LONG_STRING];

  if (env->bcc && !(env->to || env->cc))
  {
    /* some MTA's will put an Apparently-To: header field showing the Bcc:
     * recipients if there is no To: or Cc: field, so attempt to suppress
     * it by using an empty To: field.
     */
    env->to = rfc822_new_address ();
    env->to->group = 1;
    env->to->next = rfc822_new_address ();

    buffer[0] = 0;
    rfc822_cat (buffer, sizeof (buffer), "undisclosed-recipients",
		RFC822Specials);

    env->to->mailbox = safe_strdup (buffer);
  }

  mutt_set_followup_to (env);

  /* Take care of 8-bit => 7-bit conversion. */
  rfc2047_encode_adrlist (env->to);
  rfc2047_encode_adrlist (env->cc);
  rfc2047_encode_adrlist (env->from);
  rfc2047_encode_adrlist (env->mail_followup_to);
  rfc2047_encode_adrlist (env->reply_to);

  if (env->subject)
  {
    rfc2047_encode_string (buffer, sizeof (buffer) - 1,
			   (unsigned char *) env->subject);
    safe_free ((void **) &env->subject);
    env->subject = safe_strdup (buffer);
  }
  encode_headers (env->userhdrs);

  if (!env->message_id)
    env->message_id = mutt_gen_msgid ();
}
  
void mutt_bounce_message (HEADER *h, ADDRESS *to)
{
  int i;
  FILE *f;
  char date[SHORT_STRING], tempfile[_POSIX_PATH_MAX];
  MESSAGE *msg;

  if (!h)
  {
    for (i=0; i<Context->msgcount; i++)
      if (Context->hdrs[i]->tagged)
	mutt_bounce_message (Context->hdrs[i], to);
    return;
  }

  if ((msg = mx_open_message (Context, h->msgno)) != NULL)
  {
    mutt_mktemp (tempfile);
    if ((f = safe_fopen (tempfile, "w")) != NULL)
    {
      const char *fqdn;
      int ch_flags = CH_XMIT | CH_NONEWLINE;
      
      if (!option (OPTBOUNCEDELIVERED))
	ch_flags |= CH_WEED_DELIVERED;
      
      fseek (msg->fp, h->offset, 0);
      mutt_copy_header (msg->fp, h, f, ch_flags, NULL);
      fprintf (f, "Resent-From: %s", NONULL(Username));
      if((fqdn = mutt_fqdn(1)))
	fprintf (f, "@%s", fqdn);
      fprintf (f, "\nResent-%s", mutt_make_date (date, sizeof(date)));
      fputs ("Resent-To: ", f);
      mutt_write_address_list (to, f, 11);
      fputc ('\n', f);
      mutt_copy_bytes (msg->fp, f, h->content->length);
      fclose (f);

      mutt_invoke_sendmail (to, NULL, NULL, tempfile, h->content->encoding == ENC8BIT);
    }
    mx_close_message (&msg);
  }
}

/* given a list of addresses, return a list of unique addresses */
ADDRESS *mutt_remove_duplicates (ADDRESS *addr)
{
  ADDRESS *top = NULL;
  ADDRESS *tmp;
  
  if ((top = addr) == NULL)
    return (NULL);
  addr = addr->next;
  top->next = NULL;
  while (addr)
  {
    tmp = top;
    do {
      if (addr->mailbox && tmp->mailbox &&
	  !mutt_strcasecmp (addr->mailbox, tmp->mailbox))
      {
	/* duplicate address, just ignore it */
	tmp = addr;
	addr = addr->next;
	tmp->next = NULL;
	rfc822_free_address (&tmp);
      }
      else if (!tmp->next)
      {
	/* unique address.  add it to the list */
	tmp->next = addr;
	addr = addr->next;
	tmp = tmp->next;
	tmp->next = NULL;
	tmp = NULL; /* so we exit the loop */
      }
      else
	tmp = tmp->next;
    } while (tmp);
  }

  return (top);
}

int mutt_write_fcc (const char *path, HEADER *hdr, const char *msgid, int post, char *fcc)
{
  CONTEXT f;
  MESSAGE *msg;
  char tempfile[_POSIX_PATH_MAX];
  FILE *tempfp = NULL;
  int r;

  if (mx_open_mailbox (path, M_APPEND | M_QUIET, &f) == NULL)
  {
    dprint (1, (debugfile, "mutt_write_fcc(): unable to open mailbox %s in append-mode, aborting.\n",
		path));
    return (-1);
  }

  /* We need to add a Content-Length field to avoid problems where a line in
   * the message body begins with "From "   
   */
  if (f.magic == M_MMDF || f.magic == M_MBOX || f.magic == M_KENDRA)
  {
    mutt_mktemp (tempfile);
    if ((tempfp = safe_fopen (tempfile, "w+")) == NULL)
    {
      mutt_perror (tempfile);
      mx_close_mailbox (&f);
      return (-1);
    }
  }

  hdr->read = !post; /* make sure to put it in the `cur' directory (maildir) */
  if ((msg = mx_open_new_message (&f, hdr, M_ADD_FROM)) == NULL)
  {
    mx_close_mailbox (&f);
    return (-1);
  }

  /* post == 1 => postpone message. Set mode = -1 in mutt_write_rfc822_header()
   * post == 0 => Normal mode. Set mode = 0 in mutt_write_rfc822_header() 
   * */
  mutt_write_rfc822_header (msg->fp, hdr->env, hdr->content, post ? -post : 0, 0);

  /* (postponment) if this was a reply of some sort, <msgid> contians the
   * Message-ID: of message replied to.  Save it using a special X-Mutt-
   * header so it can be picked up if the message is recalled at a later
   * point in time.  This will allow the message to be marked as replied if
   * the same mailbox is still open.
   */
  if (post && msgid)
    fprintf (msg->fp, "X-Mutt-References: %s\n", msgid);
  
  /* (postponment) save the Fcc: using a special X-Mutt- header so that
   * it can be picked up when the message is recalled 
   */
  if (post && fcc)
    fprintf (msg->fp, "X-Mutt-Fcc: %s\n", fcc);
  fprintf (msg->fp, "Status: RO\n");



#ifdef _PGPPATH
  /* (postponment) if the mail is to be signed or encrypted, save this info */
  if (post && (hdr->pgp & (PGPENCRYPT | PGPSIGN)))
  {
    fputs ("X-Mutt-PGP: ", msg->fp);
    if (hdr->pgp & PGPENCRYPT) 
      fputc ('E', msg->fp);
    if (hdr->pgp & PGPSIGN)
    {
      fputc ('S', msg->fp);
      if (PgpSignAs && *PgpSignAs)
        fprintf (msg->fp, "<%s>", PgpSignAs);
      if (*PgpSignMicalg)
	fprintf (msg->fp, "M<%s>", PgpSignMicalg);
    }
    fputc ('\n', msg->fp);
  }
#endif /* _PGPPATH */

#ifdef MIXMASTER
  /* (postponement) if the mail is to be sent through a mixmaster 
   * chain, save that information
   */
  
  if (post && hdr->chain && hdr->chain)
  {
    LIST *p;

    fputs ("X-Mutt-Mix:", msg->fp);
    for (p = hdr->chain; p; p = p->next)
      fprintf (msg->fp, " %s", (char *) p->data);
    
    fputc ('\n', msg->fp);
  }
#endif    

  if (tempfp)
  {
    char sasha[LONG_STRING];
    int lines = 0;

    mutt_write_mime_body (hdr->content, tempfp);

    /* make sure the last line ends with a newline.  Emacs doesn't ensure
     * this will happen, and it can cause problems parsing the mailbox   
     * later.
     */
    fseek (tempfp, -1, 2);
    if (fgetc (tempfp) != '\n')
    {
      fseek (tempfp, 0, 2);
      fputc ('\n', tempfp);
    }

    fflush (tempfp);
    if (ferror (tempfp))
    {
      dprint (1, (debugfile, "mutt_write_fcc(): %s: write failed.\n", tempfile));
      fclose (tempfp);
      unlink (tempfile);
      mx_close_message (&msg);
      mx_close_mailbox (&f);
      return -1;
    }

    /* count the number of lines */
    rewind (tempfp);
    while (fgets (sasha, sizeof (sasha), tempfp) != NULL)
      lines++;
    fprintf (msg->fp, "Content-Length: %ld\n", (long) ftell (tempfp));
    fprintf (msg->fp, "Lines: %d\n\n", lines);

    /* copy the body and clean up */
    rewind (tempfp);
    r = mutt_copy_stream (tempfp, msg->fp);
    if (fclose (tempfp) != 0)
      r = -1;
    /* if there was an error, leave the temp version */
    if (!r)
      unlink (tempfile);
  }
  else
  {
    fputc ('\n', msg->fp); /* finish off the header */
    r = mutt_write_mime_body (hdr->content, msg->fp);
  }

  mx_close_message (&msg);
  mx_close_mailbox (&f);

  return r;
}
