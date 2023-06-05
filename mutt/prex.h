/**
 * @file
 * Manage precompiled / predefined regular expressions
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_MUTT_PREX_H
#define MUTT_MUTT_PREX_H

#include "regex3.h"

/**
 * enum Prex - Predefined list of regular expressions
 */
enum Prex
{
  PREX_URL,                   ///< `[imaps://user:pass@example.com/INBOX?foo=bar]`
  PREX_URL_QUERY_KEY_VAL,     ///< `https://example.com/?[q=foo]`
  PREX_RFC2047_ENCODED_WORD,  ///< `[=?utf-8?Q?=E8=81=AA=E6=98=8E=E7=9A=84?=]`
  PREX_GNUTLS_CERT_HOST_HASH, ///<\c [\#H foo.com A76D 954B EB79 1F49 5B3A 0A0E 0681 65B1]
  PREX_RFC5322_DATE_LAX,      ///< `[Mon, (Comment) 16 Mar 2020 15:09:35 -0700]`
  PREX_IMAP_DATE,             ///< `[16-MAR-2020 15:09:35 -0700]`
  PREX_MBOX_FROM,             ///< `[From god@heaven.af.mil Sat Jan  3 01:05:34 1996]`
  PREX_MBOX_FROM_LAX,         ///< `[From god@heaven.af.mil Sat Jan  3 01:05:34 1996]`
  PREX_ACCOUNT_CMD,           ///< `key: value`
  PREX_MAX
};

/**
 * enum PrexUrlSchemeMatch - Regex Matches for #PREX_URL
 *
 * @note The []s show the matching path of the URL
 */
enum PrexUrlSchemeMatch
{
  PREX_URL_MATCH_FULL,           ///< `[imaps://user:pass@host.comInbox?foo=bar]`
  PREX_URL_MATCH_SCHEME,         ///< `[imaps]://...`
  PREX_URL_MATCH_REST,           ///< `imaps:[//...]`
  PREX_URL_MATCH_AUTH_OR_PATH,   ///< `imaps:[somepath]|[//me@example.com/Inbox]?foo=bar`
  PREX_URL_MATCH_AUTHORITY_PATH, ///< `imaps:[//me@example.com/Inbox]?foo=bar`
  PREX_URL_MATCH_USERINFO,       ///< `...//[user:pass@]...`
  PREX_URL_MATCH_USER,           ///< `...//[user]:pass@...`
  PREX_URL_MATCH_COLONPASS,      ///< `...//user[:pass]@...`
  PREX_URL_MATCH_PASS,           ///< `...//user:[pass]@...`
  PREX_URL_MATCH_HOST,           ///< `imaps://...[host.com]...`
  PREX_URL_MATCH_HOSTNAME,       ///< `imaps://...[host.com]...`
  PREX_URL_MATCH_HOSTIPVX,       ///< `imaps://...[127.0.0.1]...`
  PREX_URL_MATCH_COLONPORT,      ///< `imaps://host.com[:993]/...`
  PREX_URL_MATCH_PORT,           ///< `imaps://host.com:[993]/...`
  PREX_URL_MATCH_SLASHPATH,      ///< `...:993[/Inbox]`
  PREX_URL_MATCH_PATH,           ///< `...:993/[Inbox]`
  PREX_URL_MATCH_PATH_ONLY,      ///< `mailto:[me@example.com]?foo=bar`
  PREX_URL_MATCH_QUESTIONQUERY,  ///< `...Inbox[?foo=bar&baz=value]`
  PREX_URL_MATCH_QUERY,          ///< `...Inbox?[foo=bar&baz=value]`
  PREX_URL_MATCH_MAX,
};

/**
 * enum PrexUrlQueryKeyValMatch - Regex Matches for #PREX_URL_QUERY_KEY_VAL
 *
 * @note The []s show the matching path of the URL Query
 */
enum PrexUrlQueryKeyValMatch
{
  PREX_URL_QUERY_KEY_VAL_MATCH_FULL, ///< `[key=val]`
  PREX_URL_QUERY_KEY_VAL_MATCH_KEY,  ///< `[key]=val`
  PREX_URL_QUERY_KEY_VAL_MATCH_VAL,  ///< `key=[val]`
  PREX_URL_QUERY_KEY_VAL_MATCH_MAX
};

/**
 * enum PrexRfc2047EncodedWordMatch - Regex Matches for #PREX_RFC2047_ENCODED_WORD
 *
 * @note The []s show the matching path of the RFC2047-encoded word
 */
enum PrexRfc2047EncodedWordMatch
{
  PREX_RFC2047_ENCODED_WORD_MATCH_FULL,     ///< `[=?utf-8?Q?=E8=81...?=]`
  PREX_RFC2047_ENCODED_WORD_MATCH_CHARSET,  ///< `=?[utf-8]?Q?=E8=81...?=`
  PREX_RFC2047_ENCODED_WORD_MATCH_ENCODING, ///< `=?utf-8?[Q]?=E8=81...?=`
  PREX_RFC2047_ENCODED_WORD_MATCH_TEXT,     ///< `=?utf-8?Q?[=E8=81...]?=`
  PREX_RFC2047_ENCODED_WORD_MATCH_MAX
};

/**
 * enum PrexGnuTlsCertHostnameMatch - Regex Matches for a TLS Certificate Hostname
 *
 * @note The []s show the matching path of the TLS Certificate Hostname
 */
enum PrexGnuTlsCertHostnameMatch
{
  PREX_GNUTLS_CERT_HOST_HASH_MATCH_FULL,      ///<\c [\#H foo.com A76D ... 65B1]
  PREX_GNUTLS_CERT_HOST_HASH_MATCH_HOST,      ///<\c \#H [foo.com] A76D ... 65B1
  PREX_GNUTLS_CERT_HOST_HASH_MATCH_HASH,      ///<\c \#H foo.com [A76D ... 65B1]
  PREX_GNUTLS_CERT_HOST_HASH_MATCH_HASH_LAST, ///<\c \#H foo.com A76D ... [65B1]
  PREX_GNUTLS_CERT_HOST_HASH_MATCH_MAX
};

/**
 * enum PrexRfc5322DateLax - Regex Matches for a RFC5322 date, including
 * obsolete comments in parentheses
 *
 * The reason we provide a regex for RFC5322 dates is that the regular parser
 * is faster, while this one is more complete.
 *
 * @note The []s show the matching path of the RFC5322 date
 * @note The `*_CFWS*` constants match `()`d comments with whitespace
 */
enum PrexRfc5322DateLax
{
  PREX_RFC5322_DATE_LAX_MATCH_FULL,        ///< `[Mon, 2 Mar 2020 14:32:55 +0200]`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS1,
  PREX_RFC5322_DATE_LAX_MATCH_MAYBE_DOW,   ///< `[Mon, ]2 Mar 2020 14:32:55 +0200`
  PREX_RFC5322_DATE_LAX_MATCH_DOW,         ///< `[Mon], 2 Mar 2020 14:32:55 +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS2,
  PREX_RFC5322_DATE_LAX_MATCH_CFWS3,
  PREX_RFC5322_DATE_LAX_MATCH_DAY,         ///< `Tue, [3] Mar 2020 14:32:55 +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS4,
  PREX_RFC5322_DATE_LAX_MATCH_MONTH,       ///< `Tue, 3 [Jan] 2020 14:32:55 +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS5,
  PREX_RFC5322_DATE_LAX_MATCH_YEAR,        ///< `Tue, 3 Mar [2020] 14:32:55 +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS6,
  PREX_RFC5322_DATE_LAX_MATCH_HOUR,        ///< `Tue, 3 Mar 2020 [14]:32:55 +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS7,
  PREX_RFC5322_DATE_LAX_MATCH_MINUTE,      ///< `Tue, 3 Mar 2020 14:[32]:55 +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS8,
  PREX_RFC5322_DATE_LAX_MATCH_COLONSECOND, ///< `Tue, 3 Mar 2020 14:32[:55] +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS9,
  PREX_RFC5322_DATE_LAX_MATCH_SECOND,      ///< `Tue, 3 Mar 2020 14:32:[55] +0200`
  PREX_RFC5322_DATE_LAX_MATCH_CFWS10,
  PREX_RFC5322_DATE_LAX_MATCH_TZFULL,      ///< `Tue, 3 Mar 2020 14:32:55[CET]`
  PREX_RFC5322_DATE_LAX_MATCH_TZ,          ///< `Tue, 3 Mar 2020 14:32:55 [+0200]`
  PREX_RFC5322_DATE_LAX_MATCH_TZ_OBS,      ///< `Tue, 3 Mar 2020 14:32:55[UT]`
  PREX_RFC5322_DATE_LAX_MATCH_MAX
};

/**
 * enum PrexImapDate - Regex matches for an IMAP `INTERNALDATE`
 *
 * @note The []s show the matching path of the IMAP date
 */
enum PrexImapDate
{
  PREX_IMAP_DATE_MATCH_FULL,   ///< `[16-MAR-2020 15:09:35 -0700]`
  PREX_IMAP_DATE_MATCH_DAY,    ///< `[ 4]-MAR-2020 15:09:35 -0700`
  PREX_IMAP_DATE_MATCH_DAY1,   ///< ` [4]-MAR-2020 15:09:35 -0700`
  PREX_IMAP_DATE_MATCH_DAY2,   ///< `[15]-MAR-2020 15:09:35 -0700`
  PREX_IMAP_DATE_MATCH_MONTH,  ///< `15-[MAR]-2020 15:09:35 -0700`
  PREX_IMAP_DATE_MATCH_YEAR,   ///< `15-MAR-[2020] 15:09:35 -0700`
  PREX_IMAP_DATE_MATCH_TIME,   ///< `15-MAR-2020 [15:09:35] -0700`
  PREX_IMAP_DATE_MATCH_TZ,     ///< `15-MAR-2020 15:09:35 [-0700]`
  PREX_IMAP_DATE_MATCH_MAX
};

/**
 * enum PrexMboxFrom - Regex matches for an mbox-style From line
 *
 * @note The []s show the matching path of the IMAP date
 */
enum PrexMboxFrom
{
  PREX_MBOX_FROM_MATCH_FULL,      ///< `[From god@heaven.af.mil Sat Jan  3 01:05:34 1996]`
  PREX_MBOX_FROM_MATCH_ENVSENDER, ///< `From [god@heaven.af.mil] Sat Jan  3 01:05:34 1996`
  PREX_MBOX_FROM_MATCH_DOW,       ///< `From god@heaven.af.mil [Sat] Jan  3 01:05:34 1996`
  PREX_MBOX_FROM_MATCH_MONTH,     ///< `From god@heaven.af.mil Sat [Jan]  3 01:05:34 1996`
  PREX_MBOX_FROM_MATCH_DAY,       ///< `From god@heaven.af.mil Sat Jan [ 3] 01:05:34 1996`
  PREX_MBOX_FROM_MATCH_DAY1,      ///< `From god@heaven.af.mil Sat Jan  [3] 01:05:34 1996`
  PREX_MBOX_FROM_MATCH_DAY2,      ///< `From god@heaven.af.mil Sat Jan [10] 01:05:34 1996`
  PREX_MBOX_FROM_MATCH_TIME,      ///< `From god@heaven.af.mil Sat Jan 10 [01:05:34] 1996`
  PREX_MBOX_FROM_MATCH_YEAR,      ///< `From god@heaven.af.mil Sat Jan 10 01:05:34 [1996]`
  PREX_MBOX_FROM_MATCH_MAX
};

/**
 * enum PrexMboxFromLax - Regex matches for an mbox-style From line, lax mode
 *
 * @note The []s show the matching path of the IMAP date
 */
enum PrexMboxFromLax
{
  PREX_MBOX_FROM_LAX_MATCH_FULL,            ///< `[From god@heaven.af.mil Sat Jan  3 01:05:34 1996]`
  PREX_MBOX_FROM_LAX_MATCH_ENVSENDER,       ///< `From [god at heaven.af.mil] Sat Jan  3 01:05:34 1996`
  PREX_MBOX_FROM_LAX_MATCH_ENVSENDER_PIPER, ///< `From [god@heaven.af.mil] Sat Jan  3 01:05:34 1996`
  PREX_MBOX_FROM_LAX_MATCH_DOW,             ///< `From god@heaven.af.mil [Sat] Jan  3 01:05:34 1996`
  PREX_MBOX_FROM_LAX_MATCH_MONTH,           ///< `From god@heaven.af.mil Sat [Jan]  3 01:05:34 1996`
  PREX_MBOX_FROM_LAX_MATCH_DAY,             ///< `From god@heaven.af.mil Sat Jan [ 3] 01:05:34 1996`
  PREX_MBOX_FROM_LAX_MATCH_DAY1,            ///< `From god@heaven.af.mil Sat Jan  [3] 01:05:34 1996`
  PREX_MBOX_FROM_LAX_MATCH_DAY2,            ///< `From god@heaven.af.mil Sat Jan [10] 01:05:34 1996`
  PREX_MBOX_FROM_LAX_MATCH_TIME,            ///< `From god@heaven.af.mil Sat Jan 10 [01:05:34] 1996`
  PREX_MBOX_FROM_LAX_MATCH_TIME_SEC,        ///< `From god@heaven.af.mil Sat Jan 10 [01:05:34] 1996`
  PREX_MBOX_FROM_LAX_MATCH_TIME_NOSEC,      ///< `From god@heaven.af.mil Sat Jan 10 [01:05] 1996`
  PREX_MBOX_FROM_LAX_MATCH_TZ,              ///< `From god@heaven.af.mil Sat Jan 10 01:05:34 [MET DST] 1996`
  PREX_MBOX_FROM_LAX_MATCH_TZ1,             ///< `From god@heaven.af.mil Sat Jan 10 01:05:34 [MET DST] 1996`
  PREX_MBOX_FROM_LAX_MATCH_TZ2,             ///< `From god@heaven.af.mil Sat Jan 10 01:05:34 [+0000] 1996`
  PREX_MBOX_FROM_LAX_MATCH_YEAR,            ///< `From god@heaven.af.mil Sat Jan 10 01:05:34 [1996]`
  PREX_MBOX_FROM_LAX_MATCH_YEAR_4DIG,       ///< `From god@heaven.af.mil Sat Jan 10 01:05:34 [1996]`
  PREX_MBOX_FROM_LAX_MATCH_YEAR_2DIG,       ///< `From god@heaven.af.mil Sat Jan 10 01:05:34 [96]`
  PREX_MBOX_FROM_LAX_MATCH_MAX
};

/**
 * enum PrexAccountCmd - Regex matches for the output lines of account_command
 */
enum PrexAccountCmd
{
  PREX_ACCOUNT_CMD_MATCH_FULL,  ///< `[key: value]`
  PREX_ACCOUNT_CMD_MATCH_KEY,   ///< `[key]: value`
  PREX_ACCOUNT_CMD_MATCH_VALUE, ///< `key: [value]`
  PREX_ACCOUNT_CMD_MATCH_MAX
};

regmatch_t *mutt_prex_capture(enum Prex which, const char *str);
void mutt_prex_cleanup(void);

#endif /* MUTT_MUTT_PREX_H */
