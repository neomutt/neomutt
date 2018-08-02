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
WHERE struct Address *From;  ///< Config: Default 'From' address to use, if isn't otherwise set

WHERE char *AliasFile; ///< Config: Save new aliases to this file
WHERE char *Attribution;  ///< Config: Message to start a reply, "On DATE, PERSON wrote:"
WHERE char *AttributionLocale;  ///< Config: Locale for dates in the attribution message
WHERE char *AttachFormat; ///< Config: printf-like format string for the attachment menu
WHERE char *ConfigCharset; ///< Config: Character set that the config files are in
WHERE char *DateFormat; ///< Config: strftime format string for the `%d` expando
WHERE char *DsnNotify;
WHERE char *DsnReturn;
WHERE char *Editor; ///< Config: External command to use as an email editor
WHERE char *Hostname; ///< Config: Fully-qualified domain name of this machine
WHERE char *IndexFormat; ///< Config: printf-like format string for the index menu (emails)

#ifdef USE_IMAP
WHERE char *ImapUser; ///< Config: (imap) Username for the IMAP server
#endif
WHERE char *Mbox; ///< Config: Folder that receives read emails (see Move)
WHERE char *MailcapPath;  ///< Config: Colon-separated list of mailcap files
WHERE char *Folder; ///< Config: Base folder for a set of mailboxes
#ifdef USE_HCACHE
WHERE char *HeaderCache;  ///< Config: (hcache) Directory/file for the header cache database
#if defined(HAVE_GDBM) || defined(HAVE_BDB)
WHERE char *HeaderCachePagesize;  ///< Config: (hcache) Database page size (gdbm,bdb4)
#endif /* HAVE_GDBM || HAVE_BDB */
#endif /* USE_HCACHE */

#ifdef USE_SOCKET
WHERE short NetInc; ///< Config: (socket) Update the progress bar after this many KB sent/received (0 to disable)
#endif /* USE_SOCKET */

#ifdef USE_NNTP
WHERE char *NewsServer; ///< Config: (nntp) Url of the news server
#endif
WHERE char *Record;  ///< Config: Folder to save 'sent' messages
WHERE char *Pager;  ///< Config: External command for viewing messages, or 'builtin' to use NeoMutt's
WHERE char *PagerFormat; ///< Config: printf-like format string for the pager's status bar
WHERE char *Postponed;  ///< Config: Folder to store postponed messages
WHERE char *IndentString; ///< Config: String used to indent 'reply' text
WHERE char *PrintCommand; ///< Config: External command to print a message
WHERE char *NewMailCommand; ///< Config: External command to run when new mail arrives
WHERE char *Realname;  ///< Config: Real name of the user
WHERE char *Shell;  ///< Config: External command to run subshells in
WHERE char *SimpleSearch;  ///< Config: Pattern to search for when search doesn't contain ~'s
#ifdef USE_SMTP
WHERE char *SmtpUrl;  ///< Config: (smtp) Url of the SMTP server
#endif /* USE_SMTP */
WHERE char *Spoolfile;
WHERE char *StatusFormat; ///< Config: printf-like format string for the index's status line
WHERE char *TsStatusFormat; ///< Config: printf-like format string for the terminal's status (window title)
WHERE char *TsIconFormat; ///< Config: printf-like format string for the terminal's icon title
WHERE char *Visual; ///< Config: Editor to use when '~v' is given in the built-in editor

WHERE short ReadInc; ///< Config: Update the progress bar after this many records read (0 to disable)
WHERE short SleepTime;  ///< Config: Time to pause after certain info messages
WHERE short Timeout;
WHERE short Wrap;
WHERE short WriteInc; ///< Config: Update the progress bar after this many records written (0 to disable)

#ifdef USE_SIDEBAR
WHERE short SidebarWidth;  ///< Config: (sidebar) Width of the sidebar
#endif
#ifdef USE_IMAP
WHERE short ImapKeepalive;
WHERE short ImapPollTimeout;
#endif

/* -- formerly in pgp.h -- */
WHERE char *PgpDefaultKey;
WHERE char *PgpSignAs;
WHERE char *PgpEntryFormat; ///< Config: printf-like format string for the PGP key selection menu

/* -- formerly in smime.h -- */
WHERE char *SmimeDefaultKey;
WHERE char *SmimeSignAs;
WHERE char *SmimeEncryptWith;

#ifdef USE_NOTMUCH
WHERE int NmQueryWindowDuration;
WHERE char *NmQueryWindowCurrentSearch;
#endif

/* These variables are backing for config items */
WHERE struct Regex *Mask;  ///< Config: Only display files/dirs matching this regex in the browser
WHERE struct Regex *QuoteRegex;  ///< Config: Regex to match quoted text in a reply

/* Quad-options */
WHERE unsigned char Bounce;
WHERE unsigned char Copy; ///< Config: Save outgoing emails to $record
WHERE unsigned char Delete;  ///< Config: Really delete messages, when the mailbox is closed
WHERE unsigned char MimeForward;
WHERE unsigned char Print;  ///< Config: Confirm before printing a message
WHERE unsigned char Quit; ///< Config: Prompt before exiting NeoMutt
#ifdef USE_SSL
WHERE unsigned char SslStarttls;
#endif
#ifdef USE_NNTP
WHERE unsigned char PostModerated;
WHERE unsigned char FollowupToPoster;
#endif

WHERE bool ArrowCursor; ///< Config: Use an arrow '->' instead of highlighting in the index
WHERE bool AsciiChars; ///< Config: Use plain ASCII characters, when drawing email threads
WHERE bool Askbcc; ///< Config: Ask the user for the blind-carbon-copy recipients
WHERE bool Askcc; ///< Config: Ask the user for the carbon-copy recipients
WHERE bool Autoedit;
WHERE bool AutoTag;
WHERE bool Beep; ///< Config: Make a noise when an error occurs
WHERE bool BeepNew; ///< Config: Make a noise when new mail arrives
WHERE bool BrailleFriendly;  ///< Config: Move the cursor to the beginning of the line
WHERE bool CheckMboxSize; ///< Config: (mbox,mmdf) Use mailbox size as an indicator of new mail
WHERE bool Confirmappend; ///< Config: Confirm before appending emails to a mailbox
WHERE bool Confirmcreate; ///< Config: Confirm before creating a new mailbox
WHERE bool DeleteUntag;
WHERE bool EditHeaders; ///< Config: Let the user edit the email headers whilst editing an email
WHERE bool FlagSafe; ///< Config: Protect flagged messages from deletion
WHERE bool ForwardDecode;  ///< Config: Decode the message when forwarding it
WHERE bool ForwardQuote;  ///< Config: Automatically quote a forwarded message using IndentString
#ifdef USE_HCACHE
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
WHERE bool HeaderCacheCompress;  ///< Config: (hcache) Enable database compression (qdbm,tokyocabinet,kyotocabinet)
#endif /* HAVE_QDBM */
#endif
WHERE bool Header; ///< Config: Include the message headers in the reply email (Weed applies)
WHERE bool Help; ///< Config: Display a help line with common key bindings
#ifdef USE_IMAP
WHERE bool ImapCheckSubscribed; ///< Config: (imap) Ask the IMAP server for a list of subscribed folders
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
WHERE bool MaildirTrash;  ///< Config: Use the maildir 'trashed' flag, rather than deleting
WHERE bool Markers;  ///< Config: Display a '+' at the beginning of wrapped lines in the pager
#if defined(USE_IMAP) || defined(USE_POP)
WHERE bool MessageCacheClean;  ///< Config: (imap/pop) Clean out obsolete entries from the message cache
#endif
WHERE bool ReadOnly;  ///< Config: Open folders in read-only mode
WHERE bool Resolve;
WHERE bool ResumeDraftFiles;
WHERE bool SaveAddress;
WHERE bool SaveEmpty; ///< Config: (mbox,mmdf) Preserve empty mailboxes
WHERE bool Score;  ///< Config: Use message scoring
#ifdef USE_SIDEBAR
WHERE bool SidebarVisible;  ///< Config: (sidebar) Show the sidebar
WHERE bool SidebarOnRight;  ///< Config: (sidebar) Display the sidebar on the right
#endif
WHERE bool StatusOnTop;  ///< Config: Display the status bar at the top
WHERE bool Suspend;
WHERE bool TextFlowed;
WHERE bool TsEnabled;
WHERE bool UseDomain;
WHERE bool WaitKey;  ///< Config: Prompt to press a key after running external commands
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
