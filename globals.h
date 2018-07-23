/**
 * @file
 * Hundreds of global variables to back the user variables
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2016 Michael R. Elkins <me@mutt.org>
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

#ifndef _MUTT_GLOBALS_H
#define _MUTT_GLOBALS_H

#include <signal.h>
#include <stdbool.h>
#include "mutt/mutt.h"
#include "where.h"
#include "config/lib.h"

#ifdef MAIN_C
/* so that global vars get included */
#include "git_ver.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "sort.h"
#endif /* MAIN_C */

WHERE struct ConfigSet *Config;

WHERE struct Context *Context;

WHERE bool ErrorBufMessage;
WHERE char ErrorBuf[STRING];
WHERE char AttachmentMarker[STRING];

WHERE char *HomeDir;
WHERE char *ShortHostname;

WHERE char *Username;

WHERE char *CurrentFolder;
WHERE char *LastFolder;

extern const char *GitVer;

WHERE struct Hash *Groups;
WHERE struct Hash *ReverseAliases;
WHERE struct Hash *TagFormats;

/* Lists of strings */
WHERE struct ListHead AlternativeOrderList INITVAL(STAILQ_HEAD_INITIALIZER(AlternativeOrderList));
WHERE struct ListHead AutoViewList INITVAL(STAILQ_HEAD_INITIALIZER(AutoViewList));
WHERE struct ListHead HeaderOrderList INITVAL(STAILQ_HEAD_INITIALIZER(HeaderOrderList));
WHERE struct ListHead MailToAllow INITVAL(STAILQ_HEAD_INITIALIZER(MailToAllow));
WHERE struct ListHead MimeLookupList INITVAL(STAILQ_HEAD_INITIALIZER(MimeLookupList));
WHERE struct ListHead Muttrc INITVAL(STAILQ_HEAD_INITIALIZER(Muttrc));
#ifdef USE_SIDEBAR
WHERE struct ListHead SidebarWhitelist INITVAL(STAILQ_HEAD_INITIALIZER(SidebarWhitelist));
#endif
WHERE struct ListHead UserHeader INITVAL(STAILQ_HEAD_INITIALIZER(UserHeader));

/* Lists of AttachMatch */
WHERE struct ListHead AttachAllow INITVAL(STAILQ_HEAD_INITIALIZER(AttachAllow));
WHERE struct ListHead AttachExclude INITVAL(STAILQ_HEAD_INITIALIZER(AttachExclude));
WHERE struct ListHead InlineAllow INITVAL(STAILQ_HEAD_INITIALIZER(InlineAllow));
WHERE struct ListHead InlineExclude INITVAL(STAILQ_HEAD_INITIALIZER(InlineExclude));

WHERE struct RegexList *Alternates;
WHERE struct RegexList *UnAlternates;
WHERE struct RegexList *MailLists;
WHERE struct RegexList *UnMailLists;
WHERE struct RegexList *SubscribedLists;
WHERE struct RegexList *UnSubscribedLists;
WHERE struct ReplaceList *SubjectRegexList;

WHERE unsigned short Counter;

/* flags for received signals */
WHERE SIG_ATOMIC_VOLATILE_T SigAlrm;
WHERE SIG_ATOMIC_VOLATILE_T SigInt;
WHERE SIG_ATOMIC_VOLATILE_T SigWinch;

WHERE int CurrentMenu;

WHERE struct AliasList Aliases INITVAL(TAILQ_HEAD_INITIALIZER(Aliases));

/* All the variables below are backing for config items */

WHERE struct Address *EnvelopeFromAddress;
WHERE struct Address *From;

WHERE char *AliasFile;
WHERE char *Attribution;
WHERE char *AttributionLocale;
WHERE char *AttachFormat;
WHERE char *ConfigCharset;
WHERE char *DateFormat;
WHERE char *DsnNotify;
WHERE char *DsnReturn;
WHERE char *Editor;
WHERE char *Hostname;
WHERE char *IndexFormat;

#ifdef USE_IMAP
WHERE char *ImapUser;
#endif
WHERE char *Mbox;
WHERE char *MailcapPath;
WHERE char *Folder;
#ifdef USE_HCACHE
WHERE char *HeaderCache;
#if defined(HAVE_GDBM) || defined(HAVE_BDB)
WHERE char *HeaderCachePagesize;
#endif /* HAVE_GDBM || HAVE_BDB */
#endif /* USE_HCACHE */

#ifdef USE_SOCKET
WHERE short NetInc;
#endif /* USE_SOCKET */

#ifdef USE_NNTP
WHERE char *NewsServer;
#endif
WHERE char *Record;
WHERE char *Pager;
WHERE char *PagerFormat;
WHERE char *Postponed;
WHERE char *IndentString;
WHERE char *PrintCommand;
WHERE char *NewMailCommand;
WHERE char *Realname;
WHERE char *Shell;
WHERE char *SimpleSearch;
#ifdef USE_SMTP
WHERE char *SmtpUrl;
#endif /* USE_SMTP */
WHERE char *Spoolfile;
WHERE char *StatusFormat;
WHERE char *TsStatusFormat;
WHERE char *TsIconFormat;
WHERE char *Visual;

WHERE short ReadInc;
WHERE short SleepTime;
WHERE short Timeout;
WHERE short Wrap;
WHERE short WriteInc;

#ifdef USE_SIDEBAR
WHERE short SidebarWidth;
#endif
#ifdef USE_IMAP
WHERE short ImapKeepalive;
WHERE short ImapPollTimeout;
#endif

/* -- formerly in pgp.h -- */
WHERE char *PgpDefaultKey;
WHERE char *PgpSignAs;
WHERE char *PgpEntryFormat;

/* -- formerly in smime.h -- */
WHERE char *SmimeDefaultKey;
WHERE char *SmimeSignAs;
WHERE char *SmimeEncryptWith;

#ifdef USE_NOTMUCH
WHERE int NmQueryWindowDuration;
WHERE char *NmQueryWindowCurrentSearch;
#endif

/* These variables are backing for config items */
WHERE struct Regex *Mask;
WHERE struct Regex *QuoteRegex;

/* Quad-options */
WHERE unsigned char Bounce;
WHERE unsigned char Copy;
WHERE unsigned char Delete;
WHERE unsigned char MimeForward;
WHERE unsigned char Print;
WHERE unsigned char Quit;
#ifdef USE_SSL
WHERE unsigned char SslStarttls;
#endif
#ifdef USE_NNTP
WHERE unsigned char PostModerated;
WHERE unsigned char FollowupToPoster;
#endif

WHERE bool ArrowCursor;
WHERE bool AsciiChars;
WHERE bool Askbcc;
WHERE bool Askcc;
WHERE bool Autoedit;
WHERE bool AutoTag;
WHERE bool Beep;
WHERE bool BeepNew;
WHERE bool BrailleFriendly;
WHERE bool CheckMboxSize;
WHERE bool Confirmappend;
WHERE bool Confirmcreate;
WHERE bool DeleteUntag;
WHERE bool EditHeaders;
WHERE bool FlagSafe;
WHERE bool ForwardDecode;
WHERE bool ForwardQuote;
#ifdef USE_HCACHE
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
WHERE bool HeaderCacheCompress;
#endif /* HAVE_QDBM */
#endif
WHERE bool Header;
WHERE bool Help;
#ifdef USE_IMAP
WHERE bool ImapCheckSubscribed;
WHERE bool ImapListSubscribed;
WHERE bool ImapPassive;
WHERE bool ImapPeek;
#endif
#ifdef USE_SSL
#ifndef USE_SSL_GNUTLS
WHERE bool SslUsesystemcerts;
WHERE bool SslUseSslv2;
#endif /* USE_SSL_GNUTLS */
WHERE bool SslForceTls;
#if defined(USE_SSL_OPENSSL) && defined(HAVE_SSL_PARTIAL_CHAIN)
WHERE bool SslVerifyPartialChains;
#endif /* USE_SSL_OPENSSL */
#endif /* defined(USE_SSL) */
WHERE bool MailCheckRecent;
WHERE bool MaildirTrash;
WHERE bool Markers;
#if defined(USE_IMAP) || defined(USE_POP)
WHERE bool MessageCacheClean;
#endif
WHERE bool ReadOnly;
WHERE bool Resolve;
WHERE bool ResumeDraftFiles;
WHERE bool SaveAddress;
WHERE bool SaveEmpty;
WHERE bool Score;
#ifdef USE_SIDEBAR
WHERE bool SidebarVisible;
WHERE bool SidebarOnRight;
#endif
WHERE bool StatusOnTop;
WHERE bool Suspend;
WHERE bool TextFlowed;
WHERE bool TsEnabled;
WHERE bool UseDomain;
WHERE bool WaitKey;
WHERE bool WrapSearch;
WHERE bool WriteBcc; /**< write out a bcc header? */

WHERE bool CryptUsePka;

/* PGP options */

WHERE bool CryptConfirmhook;
WHERE bool CryptOpportunisticEncrypt;
WHERE bool SmimeIsDefault;
WHERE bool PgpIgnoreSubkeys;
WHERE bool PgpLongIds;
WHERE bool PgpShowUnusable;
WHERE bool PgpAutoinline;

/* news options */

#ifdef USE_NNTP
WHERE bool SaveUnsubscribed;
WHERE bool XCommentTo;
#endif

#ifdef USE_NOTMUCH
WHERE bool VirtualSpoolfile;
#endif

#endif /* _MUTT_GLOBALS_H */
