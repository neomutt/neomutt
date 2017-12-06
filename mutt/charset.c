/**
 * @file
 * Conversion between different character encodings
 *
 * @authors
 * Copyright (C) 1999-2002,2007 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page charset Conversion between different character encodings
 *
 * Conversion between different character encodings
 *
 * | Data                | Description
 * | :------------------ | :--------------------------------------------------
 * | #PreferredMIMENames | Lookup table of preferred charsets
 *
 * | Function                       | Description
 * | :----------------------------- | :---------------------------------------------------------
 * | mutt_cs_canonical_charset()    | Canonicalise the charset of a string
 * | mutt_cs_chscmp()               | Are the names of two character sets equivalent?
 * | mutt_cs_fgetconv()             | Convert a file's character set
 * | mutt_cs_fgetconvs()            | Convert a file's charset into a string buffer
 * | mutt_cs_fgetconv_close()       | Close an fgetconv handle
 * | mutt_cs_get_default_charset()  | Get the default character set
 * | mutt_cs_iconv()                | Change the encoding of a string
 * | mutt_cs_set_langinfo_charset() | Set the user's choice of character set
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "charset.h"
#include "memory.h"
#include "string2.h"

#ifndef EILSEQ
#define EILSEQ EINVAL
#endif

char *AssumedCharset; /**< Encoding schemes for messages without indication */
char *Charset;        /**< User's choice of character set */

// clang-format off
/**
 * PreferredMIMENames - Lookup table of preferred charsets
 *
 * The following list has been created manually from the data under:
 * http://www.isi.edu/in-notes/iana/assignments/character-sets
 * Last update: 2000-09-07
 *
 * @note It includes only the subset of character sets for which a preferred
 * MIME name is given.
 */
const struct MimeNames PreferredMIMENames[] =
{
  { "ansi_x3.4-1968",        "us-ascii"      },
  { "iso-ir-6",              "us-ascii"      },
  { "iso_646.irv:1991",      "us-ascii"      },
  { "ascii",                 "us-ascii"      },
  { "iso646-us",             "us-ascii"      },
  { "us",                    "us-ascii"      },
  { "ibm367",                "us-ascii"      },
  { "cp367",                 "us-ascii"      },
  { "csASCII",               "us-ascii"      },

  { "csISO2022KR",           "iso-2022-kr"   },
  { "csEUCKR",               "euc-kr"        },
  { "csISO2022JP",           "iso-2022-jp"   },
  { "csISO2022JP2",          "iso-2022-jp-2" },

  { "ISO_8859-1:1987",       "iso-8859-1"    },
  { "iso-ir-100",            "iso-8859-1"    },
  { "iso_8859-1",            "iso-8859-1"    },
  { "latin1",                "iso-8859-1"    },
  { "l1",                    "iso-8859-1"    },
  { "IBM819",                "iso-8859-1"    },
  { "CP819",                 "iso-8859-1"    },
  { "csISOLatin1",           "iso-8859-1"    },

  { "ISO_8859-2:1987",       "iso-8859-2"    },
  { "iso-ir-101",            "iso-8859-2"    },
  { "iso_8859-2",            "iso-8859-2"    },
  { "latin2",                "iso-8859-2"    },
  { "l2",                    "iso-8859-2"    },
  { "csISOLatin2",           "iso-8859-2"    },

  { "ISO_8859-3:1988",       "iso-8859-3"    },
  { "iso-ir-109",            "iso-8859-3"    },
  { "ISO_8859-3",            "iso-8859-3"    },
  { "latin3",                "iso-8859-3"    },
  { "l3",                    "iso-8859-3"    },
  { "csISOLatin3",           "iso-8859-3"    },

  { "ISO_8859-4:1988",       "iso-8859-4"    },
  { "iso-ir-110",            "iso-8859-4"    },
  { "ISO_8859-4",            "iso-8859-4"    },
  { "latin4",                "iso-8859-4"    },
  { "l4",                    "iso-8859-4"    },
  { "csISOLatin4",           "iso-8859-4"    },

  { "ISO_8859-6:1987",       "iso-8859-6"    },
  { "iso-ir-127",            "iso-8859-6"    },
  { "iso_8859-6",            "iso-8859-6"    },
  { "ECMA-114",              "iso-8859-6"    },
  { "ASMO-708",              "iso-8859-6"    },
  { "arabic",                "iso-8859-6"    },
  { "csISOLatinArabic",      "iso-8859-6"    },

  { "ISO_8859-7:1987",       "iso-8859-7"    },
  { "iso-ir-126",            "iso-8859-7"    },
  { "ISO_8859-7",            "iso-8859-7"    },
  { "ELOT_928",              "iso-8859-7"    },
  { "ECMA-118",              "iso-8859-7"    },
  { "greek",                 "iso-8859-7"    },
  { "greek8",                "iso-8859-7"    },
  { "csISOLatinGreek",       "iso-8859-7"    },

  { "ISO_8859-8:1988",       "iso-8859-8"    },
  { "iso-ir-138",            "iso-8859-8"    },
  { "ISO_8859-8",            "iso-8859-8"    },
  { "hebrew",                "iso-8859-8"    },
  { "csISOLatinHebrew",      "iso-8859-8"    },

  { "ISO_8859-5:1988",       "iso-8859-5"    },
  { "iso-ir-144",            "iso-8859-5"    },
  { "ISO_8859-5",            "iso-8859-5"    },
  { "cyrillic",              "iso-8859-5"    },
  { "csISOLatinCyrillic",    "iso-8859-5"    },

  { "ISO_8859-9:1989",       "iso-8859-9"    },
  { "iso-ir-148",            "iso-8859-9"    },
  { "ISO_8859-9",            "iso-8859-9"    },
  { "latin5",                "iso-8859-9"    },  /* this is not a bug */
  { "l5",                    "iso-8859-9"    },
  { "csISOLatin5",           "iso-8859-9"    },

  { "ISO_8859-10:1992",      "iso-8859-10"   },
  { "iso-ir-157",            "iso-8859-10"   },
  { "latin6",                "iso-8859-10"   },  /* this is not a bug */
  { "l6",                    "iso-8859-10"   },
  { "csISOLatin6",           "iso-8859-10"   },

  { "csKOI8r",               "koi8-r"        },

  { "MS_Kanji",              "Shift_JIS"     },  /* Note the underscore! */
  { "csShiftJis",            "Shift_JIS"     },

  { "Extended_UNIX_Code_Packed_Format_for_Japanese",
                             "euc-jp"        },
  { "csEUCPkdFmtJapanese",   "euc-jp"        },

  { "csGB2312",              "gb2312"        },
  { "csbig5",                "big5"          },

  /*
   * End of official brain damage. What follows has been taken from glibc's
   * localedata files.
   */

  { "iso_8859-13",           "iso-8859-13"   },
  { "iso-ir-179",            "iso-8859-13"   },
  { "latin7",                "iso-8859-13"   },  /* this is not a bug */
  { "l7",                    "iso-8859-13"   },

  { "iso_8859-14",           "iso-8859-14"   },
  { "latin8",                "iso-8859-14"   },  /* this is not a bug */
  { "l8",                    "iso-8859-14"   },

  { "iso_8859-15",           "iso-8859-15"   },
  { "latin9",                "iso-8859-15"   },  /* this is not a bug */

  /* Suggested by Ionel Mugurel Ciobica <tgakic@sg10.chem.tue.nl> */
  { "latin0",                "iso-8859-15"   },  /* this is not a bug */

  { "iso_8859-16",           "iso-8859-16"   },
  { "latin10",               "iso-8859-16"   },  /* this is not a bug */

  { "646",                   "us-ascii"      },

  /*
   * http://www.sun.com/software/white-papers/wp-unicode/
   */

  { "eucJP",                 "euc-jp"        },
  { "PCK",                   "Shift_JIS"     },
  { "ko_KR-euc",             "euc-kr"        },
  { "zh_TW-big5",            "big5"          },

  /* seems to be common on some systems */

  { "sjis",                  "Shift_JIS"     },
  { "euc-jp-ms",             "eucJP-ms"      },

  /*
   * If you happen to encounter system-specific brain-damage with respect to
   * character set naming, please add it above this comment, and submit a patch
   * to <neomutt-devel@neomutt.org>.
   */

  /* End of aliases.  Please keep this line last. */

  { NULL,                     NULL           },
};
// clang-format on

/**
 * mutt_cs_fgetconv_close - Close an fgetconv handle
 * @param handle fgetconv handle
 */
void mutt_cs_fgetconv_close(FGETCONV **handle)
{
  struct FgetConv *fc = (struct FgetConv *) *handle;

  if (fc->cd != (iconv_t) -1)
    iconv_close(fc->cd);
  FREE(handle);
}

/**
 * mutt_cs_fgetconv - Convert a file's character set
 * @param handle fgetconv handle
 * @retval num Next character in the converted file
 * @retval EOF Error
 *
 * A file is read into a buffer and its character set is converted.
 * Each call to this function will return one converted character.
 * The buffer is refilled automatically when empty.
 */
int mutt_cs_fgetconv(FGETCONV *handle)
{
  struct FgetConv *fc = (struct FgetConv *) handle;

  if (!fc)
    return EOF;
  if (fc->cd == (iconv_t) -1)
    return fgetc(fc->file);
  if (!fc->p)
    return EOF;
  if (fc->p < fc->ob)
    return (unsigned char) *(fc->p)++;

  /* Try to convert some more */
  fc->p = fc->ob = fc->bufo;
  if (fc->ibl)
  {
    size_t obl = sizeof(fc->bufo);
    iconv(fc->cd, (ICONV_CONST char **) &fc->ib, &fc->ibl, &fc->ob, &obl);
    if (fc->p < fc->ob)
      return (unsigned char) *(fc->p)++;
  }

  /* If we trusted iconv a bit more, we would at this point
   * ask why it had stopped converting ... */

  /* Try to read some more */
  if (fc->ibl == sizeof(fc->bufi) ||
      (fc->ibl && fc->ib + fc->ibl < fc->bufi + sizeof(fc->bufi)))
  {
    fc->p = 0;
    return EOF;
  }
  if (fc->ibl)
    memcpy(fc->bufi, fc->ib, fc->ibl);
  fc->ib = fc->bufi;
  fc->ibl += fread(fc->ib + fc->ibl, 1, sizeof(fc->bufi) - fc->ibl, fc->file);

  /* Try harder this time to convert some */
  if (fc->ibl)
  {
    size_t obl = sizeof(fc->bufo);
    mutt_cs_iconv(fc->cd, (const char **) &fc->ib, &fc->ibl, &fc->ob, &obl, fc->inrepls, 0);
    if (fc->p < fc->ob)
      return (unsigned char) *(fc->p)++;
  }

  /* Either the file has finished or one of the buffers is too small */
  fc->p = 0;
  return EOF;
}

/**
 * mutt_cs_fgetconvs - Convert a file's charset into a string buffer
 * @param buf    Buffer for result
 * @param l      Length of buffer
 * @param handle fgetconv handle
 * @retval ptr  Result buffer on success
 * @retval NULL Error
 *
 * Read a file into a buffer, converting the character set as it goes.
 */
char *mutt_cs_fgetconvs(char *buf, size_t l, FGETCONV *handle)
{
  int c;
  size_t r;

  for (r = 0; r + 1 < l;)
  {
    c = mutt_cs_fgetconv(handle);
    if (c == EOF)
      break;
    buf[r++] = (char) c;
    if (c == '\n')
      break;
  }
  buf[r] = '\0';

  if (r)
    return buf;
  else
    return NULL;
}

/**
 * mutt_cs_canonical_charset - Canonicalise the charset of a string
 * @param dest Buffer for canonical character set name
 * @param dlen Length of buffer
 * @param name Name to be canonicalised
 *
 * This first ties off any charset extension such as "//TRANSLIT",
 * canonicalizes the charset and re-adds the extension
 */
void mutt_cs_canonical_charset(char *dest, size_t dlen, const char *name)
{
  char *p = NULL, *ext = NULL;
  char in[LONG_STRING], scratch[LONG_STRING];

  mutt_str_strfcpy(in, name, sizeof(in));
  if ((ext = strchr(in, '/')))
    *ext++ = '\0';

  if ((mutt_str_strcasecmp(in, "utf-8") == 0) ||
      (mutt_str_strcasecmp(in, "utf8") == 0))
  {
    mutt_str_strfcpy(dest, "utf-8", dlen);
    goto out;
  }

  /* catch some common iso-8859-something misspellings */
  if ((mutt_str_strncasecmp(in, "8859", 4) == 0) && in[4] != '-')
    snprintf(scratch, sizeof(scratch), "iso-8859-%s", in + 4);
  else if (mutt_str_strncasecmp(in, "8859-", 5) == 0)
    snprintf(scratch, sizeof(scratch), "iso-8859-%s", in + 5);
  else if ((mutt_str_strncasecmp(in, "iso8859", 7) == 0) && in[7] != '-')
    snprintf(scratch, sizeof(scratch), "iso_8859-%s", in + 7);
  else if (mutt_str_strncasecmp(in, "iso8859-", 8) == 0)
    snprintf(scratch, sizeof(scratch), "iso_8859-%s", in + 8);
  else
    mutt_str_strfcpy(scratch, in, sizeof(scratch));

  for (size_t i = 0; PreferredMIMENames[i].key; i++)
  {
    if ((mutt_str_strcasecmp(scratch, PreferredMIMENames[i].key) == 0) ||
        (mutt_str_strcasecmp(scratch, PreferredMIMENames[i].key) == 0))
    {
      mutt_str_strfcpy(dest, PreferredMIMENames[i].pref, dlen);
      goto out;
    }
  }

  mutt_str_strfcpy(dest, scratch, dlen);

  /* for cosmetics' sake, transform to lowercase. */
  for (p = dest; *p; p++)
    *p = tolower(*p);

out:
  if (ext && *ext)
  {
    mutt_str_strcat(dest, dlen, "/");
    mutt_str_strcat(dest, dlen, ext);
  }
}

/**
 * mutt_cs_chscmp - Are the names of two character sets equivalent?
 * @param s   First character set
 * @param chs Second character set
 * @retval num true if the names are equivalent
 *
 * Charsets may have extensions that mutt_cs_canonical_charset() leaves intact;
 * we expect 'chs' to originate from neomutt code, not user input (i.e. 'chs'
 * does _not_ have any extension) we simply check if the shorter string is a
 * prefix for the longer.
 */
int mutt_cs_chscmp(const char *s, const char *chs)
{
  if (!s || !chs)
    return 0;

  char buffer[STRING];

  mutt_cs_canonical_charset(buffer, sizeof(buffer), s);
  int a = mutt_str_strlen(buffer);
  int b = mutt_str_strlen(chs);
  return (mutt_str_strncasecmp(a > b ? buffer : chs, a > b ? chs : buffer, MIN(a, b)) == 0);
}

/**
 * mutt_cs_get_default_charset - Get the default character set
 * @retval ptr Name of the default character set
 *
 * @note This returns a pointer to a static buffer.  Do not free it.
 */
char *mutt_cs_get_default_charset(void)
{
  static char fcharset[SHORT_STRING];
  const char *c = AssumedCharset;
  const char *c1 = NULL;

  if (c && *c)
  {
    c1 = strchr(c, ':');
    mutt_str_strfcpy(fcharset, c, c1 ? (c1 - c + 1) : sizeof(fcharset));
    return fcharset;
  }
  return strcpy(fcharset, "us-ascii");
}

/**
 * mutt_cs_iconv - Change the encoding of a string
 * @param[in]     cd           Iconv conversion descriptor
 * @param[in,out] inbuf        Buffer to convert
 * @param[in,out] inbytesleft  Length of buffer to convert
 * @param[in,out] outbuf       Buffer for the result
 * @param[in,out] outbytesleft Length of result buffer
 * @param[in]     inrepls      Input replacement characters
 * @param[in]     outrepl      Output replacement characters
 * @retval num Number of characters converted
 *
 * Like iconv, but keeps going even when the input is invalid
 * If you're supplying inrepls, the source charset should be stateless;
 * if you're supplying an outrepl, the target charset should be.
 */
size_t mutt_cs_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char **outbuf,
                     size_t *outbytesleft, const char **inrepls, const char *outrepl)
{
  size_t rc = 0, ret1;
  const char *ib = *inbuf;
  size_t ibl = *inbytesleft;
  char *ob = *outbuf;
  size_t obl = *outbytesleft;

  while (true)
  {
    ret1 = iconv(cd, (ICONV_CONST char **) &ib, &ibl, &ob, &obl);
    if (ret1 != (size_t) -1)
      rc += ret1;
    if (ibl && obl && errno == EILSEQ)
    {
      if (inrepls)
      {
        /* Try replacing the input */
        const char **t = NULL;
        for (t = inrepls; *t; t++)
        {
          const char *ib1 = *t;
          size_t ibl1 = strlen(*t);
          char *ob1 = ob;
          size_t obl1 = obl;
          iconv(cd, (ICONV_CONST char **) &ib1, &ibl1, &ob1, &obl1);
          if (!ibl1)
          {
            ib++;
            ibl--;
            ob = ob1;
            obl = obl1;
            rc++;
            break;
          }
        }
        if (*t)
          continue;
      }
      /* Replace the output */
      if (!outrepl)
        outrepl = "?";
      iconv(cd, NULL, NULL, &ob, &obl);
      if (obl)
      {
        int n = strlen(outrepl);
        if (n > obl)
        {
          outrepl = "?";
          n = 1;
        }
        memcpy(ob, outrepl, n);
        ib++;
        ibl--;
        ob += n;
        obl -= n;
        rc++;
        iconv(cd, NULL, NULL, NULL, NULL); /* for good measure */
        continue;
      }
    }
    *inbuf = ib;
    *inbytesleft = ibl;
    *outbuf = ob;
    *outbytesleft = obl;
    return rc;
  }
}

/**
 * mutt_cs_set_langinfo_charset - Set the user's choice of character set
 *
 * Lookup the character map used by the user's locale and store it in Charset.
 */
void mutt_cs_set_langinfo_charset(void)
{
  char buf[LONG_STRING];
  char buf2[LONG_STRING];

  mutt_str_strfcpy(buf, nl_langinfo(CODESET), sizeof(buf));
  mutt_cs_canonical_charset(buf2, sizeof(buf2), buf);

  /* finally, set $charset */
  Charset = mutt_str_strdup(buf2);
  if (!Charset)
    Charset = mutt_str_strdup("iso-8859-1");
}
