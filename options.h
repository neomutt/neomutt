/**
 * @file
 * Handling of global boolean variables
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_OPTIONS_H_
#define _MUTT_OPTIONS_H_

#include <stdbool.h>
#include "where.h"

/* pseudo options */

WHERE bool OptAttachMsg;           /**< (pseudo) used by attach-message */
WHERE bool OptAuxSort;             /**< (pseudo) using auxiliary sort function */
WHERE bool OptDontHandlePgpKeys; /**< (pseudo) used to extract PGP keys */
WHERE bool OptForceRefresh;        /**< (pseudo) refresh even during macros */
WHERE bool OptHideRead;            /**< (pseudo) whether or not hide read messages */
WHERE bool OptIgnoreMacroEvents;  /**< (pseudo) don't process macro/push/exec events while set */
WHERE bool OptKeepQuiet;           /**< (pseudo) shut up the message and refresh functions while we are executing an external program.  */
WHERE bool OptMenuCaller;          /**< (pseudo) tell menu to give caller a take */
WHERE bool OptMsgErr;              /**< (pseudo) used by mutt_error/mutt_message */
WHERE bool OptNeedRescore;         /**< (pseudo) set when the `score' command is used */
WHERE bool OptNeedResort;          /**< (pseudo) used to force a re-sort */
#ifdef USE_NNTP
WHERE bool OptNews;                 /**< (pseudo) used to change reader mode */
WHERE bool OptNewsSend;            /**< (pseudo) used to change behavior when posting */
#endif
WHERE bool OptNoCurses;            /**< (pseudo) when sending in batch mode */
WHERE bool OptPgpCheckTrust;      /**< (pseudo) used by pgp_select_key () */
WHERE bool OptRedrawTree;          /**< (pseudo) redraw the thread tree */
WHERE bool OptResortInit;          /**< (pseudo) used to force the next resort to be from scratch */
WHERE bool OptSearchInvalid;       /**< (pseudo) used to invalidate the search pat */
WHERE bool OptSearchReverse;       /**< (pseudo) used by ci_search_command */
WHERE bool OptSignalsBlocked;      /**< (pseudo) using by mutt_block_signals () */
WHERE bool OptSortSubthreads;      /**< (pseudo) used when $sort_aux changes */
WHERE bool OptSysSignalsBlocked;  /**< (pseudo) using by mutt_block_signals_system () */
WHERE bool OptViewAttach;          /**< (pseudo) signals that we are viewing attachments */

#define mutt_bit_set(v, n)    v[n / 8] |= (1 << (n % 8))
#define mutt_bit_unset(v, n)  v[n / 8] &= ~(1 << (n % 8))
#define mutt_bit_toggle(v, n) v[n / 8] ^= (1 << (n % 8))
#define mutt_bit_isset(v, n)  (v[n / 8] & (1 << (n % 8)))

/* All the variables below are backing for config items */

WHERE bool Allow8bit;
WHERE bool AllowAnsi;
WHERE bool ArrowCursor;
WHERE bool AsciiChars;
WHERE bool Askbcc;
WHERE bool Askcc;
WHERE bool AskFollowUp;
WHERE bool AskXCommentTo;
WHERE bool AttachSplit;
WHERE bool Autoedit;
WHERE bool AutoTag;
WHERE bool Beep;
WHERE bool BeepNew;
WHERE bool BounceDelivered;
WHERE bool BrailleFriendly;
WHERE bool ChangeFolderNext;
WHERE bool CheckMboxSize;
WHERE bool CheckNew;
WHERE bool CollapseAll;
WHERE bool CollapseUnread;
WHERE bool CollapseFlagged;
WHERE bool Confirmappend;
WHERE bool Confirmcreate;
WHERE bool DeleteUntag;
WHERE bool DigestCollapse;
WHERE bool DuplicateThreads;
WHERE bool EditHeaders;
WHERE bool EncodeFrom;
WHERE bool UseEnvelopeFrom;
WHERE bool FastReply;
WHERE bool FccClear;
WHERE bool FlagSafe;
WHERE bool FollowupTo;
WHERE bool ForceName;
WHERE bool ForwardDecode;
WHERE bool ForwardQuote;
WHERE bool ForwardReferences;
#ifdef USE_HCACHE
WHERE bool MaildirHeaderCacheVerify;
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
WHERE bool HeaderCacheCompress;
#endif /* HAVE_QDBM */
#endif
WHERE bool Hdrs;
WHERE bool Header;
WHERE bool HeaderColorPartial;
WHERE bool Help;
WHERE bool HiddenHost;
WHERE bool HideLimited;
WHERE bool HideMissing;
WHERE bool HideThreadSubject;
WHERE bool HideTopLimited;
WHERE bool HideTopMissing;
WHERE bool HonorDisposition;
WHERE bool IgnoreListReplyTo;
#ifdef USE_IMAP
WHERE bool ImapCheckSubscribed;
WHERE bool ImapIdle;
WHERE bool ImapListSubscribed;
WHERE bool ImapPassive;
WHERE bool ImapPeek;
WHERE bool ImapServernoise;
#endif
#ifdef USE_SSL
#ifndef USE_SSL_GNUTLS
WHERE bool SslUsesystemcerts;
WHERE bool SslUseSslv2;
#endif /* USE_SSL_GNUTLS */
WHERE bool SslUseSslv3;
WHERE bool SslUseTlsv1;
WHERE bool SslUseTlsv11;
WHERE bool SslUseTlsv12;
WHERE bool SslForceTls;
WHERE bool SslVerifyDates;
WHERE bool SslVerifyHost;
#if defined(USE_SSL_OPENSSL) && defined(HAVE_SSL_PARTIAL_CHAIN)
WHERE bool SslVerifyPartialChains;
#endif /* USE_SSL_OPENSSL */
#endif /* defined(USE_SSL) */
WHERE bool ImplicitAutoview;
WHERE bool IncludeOnlyfirst;
WHERE bool KeepFlagged;
WHERE bool MailcapSanitize;
WHERE bool MailCheckRecent;
WHERE bool MailCheckStats;
WHERE bool MaildirTrash;
WHERE bool MaildirCheckCur;
WHERE bool Markers;
WHERE bool MarkOld;
WHERE bool MenuScroll;  /**< scroll menu instead of implicit next-page */
WHERE bool MenuMoveOff; /**< allow menu to scroll past last entry */
#if defined(USE_IMAP) || defined(USE_POP)
WHERE bool MessageCacheClean;
#endif
WHERE bool MetaKey; /**< interpret ALT-x as ESC-x */
WHERE bool Metoo;
WHERE bool MhPurge;
WHERE bool MimeForwardDecode;
WHERE bool MimeTypeQueryFirst;
#ifdef USE_NNTP
WHERE bool MimeSubject; /**< encode subject line with RFC2047 */
#endif
WHERE bool NarrowTree;
WHERE bool PagerStop;
WHERE bool PipeDecode;
WHERE bool PipeSplit;
#ifdef USE_POP
WHERE bool PopAuthTryAll;
WHERE bool PopLast;
#endif
WHERE bool PostponeEncrypt;
WHERE bool PrintDecode;
WHERE bool PrintSplit;
WHERE bool PromptAfter;
WHERE bool ReadOnly;
WHERE bool ReflowSpaceQuotes;
WHERE bool ReflowText;
WHERE bool ReplySelf;
WHERE bool ReplyWithXorig;
WHERE bool Resolve;
WHERE bool ResumeDraftFiles;
WHERE bool ResumeEditedDraftFiles;
WHERE bool ReverseAlias;
WHERE bool ReverseName;
WHERE bool ReverseRealname;
WHERE bool Rfc2047Parameters;
WHERE bool SaveAddress;
WHERE bool SaveEmpty;
WHERE bool SaveName;
WHERE bool Score;
#ifdef USE_SIDEBAR
WHERE bool SidebarVisible;
WHERE bool SidebarFolderIndent;
WHERE bool SidebarNewMailOnly;
WHERE bool SidebarNextNewWrap;
WHERE bool SidebarShortPath;
WHERE bool SidebarOnRight;
#endif
WHERE bool SigDashes;
WHERE bool SigOnTop;
WHERE bool SortRe;
WHERE bool StatusOnTop;
WHERE bool StrictThreads;
WHERE bool Suspend;
WHERE bool TextFlowed;
WHERE bool ThoroughSearch;
WHERE bool ThreadReceived;
WHERE bool Tilde;
WHERE bool TsEnabled;
WHERE bool UncollapseJump;
WHERE bool UncollapseNew;
WHERE bool Use8bitmime;
WHERE bool UseDomain;
WHERE bool UseFrom;
WHERE bool PgpUseGpgAgent;
#ifdef HAVE_GETADDRINFO
WHERE bool UseIpv6;
#endif
WHERE bool WaitKey;
WHERE bool Weed;
WHERE bool SmartWrap;
WHERE bool WrapSearch;
WHERE bool WriteBcc; /**< write out a bcc header? */
WHERE bool UserAgent;

WHERE bool CryptUseGpgme;
WHERE bool CryptUsePka;

/* PGP options */

WHERE bool CryptAutosign;
WHERE bool CryptAutoencrypt;
WHERE bool CryptAutopgp;
WHERE bool CryptAutosmime;
WHERE bool CryptConfirmhook;
WHERE bool CryptOpportunisticEncrypt;
WHERE bool CryptReplyencrypt;
WHERE bool CryptReplysign;
WHERE bool CryptReplysignencrypted;
WHERE bool CryptTimestamp;
WHERE bool SmimeIsDefault;
WHERE bool SmimeSelfEncrypt;
WHERE bool SmimeAskCertLabel;
WHERE bool SmimeDecryptUseDefaultKey;
WHERE bool PgpIgnoreSubkeys;
WHERE bool PgpCheckExit;
WHERE bool PgpLongIds;
WHERE bool PgpAutoDecode;
WHERE bool PgpRetainableSigs;
WHERE bool PgpSelfEncrypt;
WHERE bool PgpStrictEnc;
WHERE bool ForwardDecrypt;
WHERE bool PgpShowUnusable;
WHERE bool PgpAutoinline;
WHERE bool PgpReplyinline;

/* news options */

#ifdef USE_NNTP
WHERE bool ShowNewNews;
WHERE bool ShowOnlyUnread;
WHERE bool SaveUnsubscribed;
WHERE bool NntpListgroup;
WHERE bool NntpLoadDescription;
WHERE bool XCommentTo;
#endif

#ifdef USE_NOTMUCH
WHERE bool VirtualSpoolfile;
WHERE bool NmRecord;
#endif

#endif /* _MUTT_OPTIONS_H_ */
