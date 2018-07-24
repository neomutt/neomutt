/**
 * @file
 * Hundreds of global variables to back the user variables
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

#ifndef _DUMP_VARS_H
#define _DUMP_VARS_H

#include <stdbool.h>

extern struct Address *EnvelopeFromAddress;
extern struct Address *From;

extern char *AliasFile;
extern char *AliasFormat;
extern char *AttachSep;
extern char *Attribution;
extern char *AttributionLocale;
extern char *AttachCharset;
extern char *AttachFormat;
extern struct Regex *AttachKeyword;
extern char *ComposeFormat;
extern char *ConfigCharset;
extern char *ContentType;
extern char *DefaultHook;
extern char *DateFormat;
extern char *DisplayFilter;
extern char *DsnNotify;
extern char *DsnReturn;
extern char *Editor;
extern char *EmptySubject;
extern char *Escape;
extern char *FolderFormat;
extern char *ForwardAttributionIntro;
extern char *ForwardAttributionTrailer;
extern char *ForwardFormat;
extern char *Hostname;
extern struct MbTable *FromChars;
extern char *IndexFormat;
extern char *HistoryFile;

extern char *ImapAuthenticators;
extern char *ImapDelimChars;
extern char *ImapHeaders;
extern char *ImapLogin;
extern char *ImapPass;
extern char *ImapUser;
extern char *Mbox;
extern char *Ispell;
extern char *MailcapPath;
extern char *Folder;
extern char *MessageCachedir;
extern char *HeaderCache;
extern char *HeaderCacheBackend;
extern char *HeaderCachePageSize;
extern char *MarkMacroPrefix;
extern char *MhSeqFlagged;
extern char *MhSeqReplied;
extern char *MhSeqUnseen;
extern char *MimeTypeQueryCommand;
extern char *MessageFormat;

extern short NetInc;

extern char *Mixmaster;
extern char *MixEntryFormat;

extern char *GroupIndexFormat;
extern char *Inews;
extern char *NewsCacheDir;
extern char *NewsServer;
extern char *NewsgroupsCharset;
extern char *NewsRc;
extern char *NntpAuthenticators;
extern char *NntpUser;
extern char *NntpPass;
extern char *Record;
extern char *Pager;
extern char *PagerFormat;
extern char *PipeSep;
extern char *PopAuthenticators;
extern short PopCheckinterval;
extern char *PopHost;
extern char *PopPass;
extern char *PopUser;
extern char *PostIndentString;
extern char *Postponed;
extern char *PostponeEncryptAs;
extern char *IndentString;
extern char *PrintCommand;
extern char *NewMailCommand;
extern char *QueryCommand;
extern char *QueryFormat;
extern char *RealName;
extern short SearchContext;
extern char *SendCharset;
extern char *Sendmail;
extern char *Shell;
extern char *ShowMultipartAlternative;
extern char *SidebarDelimChars;
extern char *SidebarDividerChar;
extern char *SidebarFormat;
extern char *SidebarIndentString;
extern char *Signature;
extern char *SimpleSearch;
extern char *SmtpAuthenticators;
extern char *SmtpPass;
extern char *SmtpUrl;
extern char *SpoolFile;
extern char *SpamSeparator;
extern struct MbTable *StatusChars;
extern char *StatusFormat;
extern char *Tmpdir;
extern struct MbTable *ToChars;
extern struct MbTable *FlagChars;
extern char *Trash;
extern char *TSStatusFormat;
extern char *TSIconFormat;
extern char *Visual;

extern char *HiddenTags;

extern short NntpPoll;
extern short NntpContext;

extern short DebugLevel;
extern char *DebugFile;

extern short History;
extern short MenuContext;
extern short PagerContext;
extern short PagerIndexLines;
extern short ReadInc;
extern short ReflowWrap;
extern short SaveHistory;
extern short SendmailWait;
extern short SleepTime;
extern short SkipQuotedOffset;
extern short TimeInc;
extern short Timeout;
extern short Wrap;
extern short WrapHeaders;
extern short WriteInc;

extern short ScoreThresholdDelete;
extern short ScoreThresholdRead;
extern short ScoreThresholdFlag;

extern short SidebarComponentDepth;
extern short SidebarWidth;
extern short ImapKeepalive;
extern short ImapPipelineDepth;
extern short ImapPollTimeout;

extern struct Regex *PgpGoodSign;
extern struct Regex *PgpDecryptionOkay;
extern char *PgpSignAs;
extern short PgpTimeout;
extern char *PgpEntryFormat;
extern char *PgpClearSignCommand;
extern char *PgpDecodeCommand;
extern char *PgpVerifyCommand;
extern char *PgpDecryptCommand;
extern char *PgpSignCommand;
extern char *PgpEncryptSignCommand;
extern char *PgpEncryptOnlyCommand;
extern char *PgpImportCommand;
extern char *PgpExportCommand;
extern char *PgpVerifyKeyCommand;
extern char *PgpListSecringCommand;
extern char *PgpListPubringCommand;
extern char *PgpGetkeysCommand;
extern char *PgpSelfEncryptAs;

extern char *SmimeDefaultKey;
extern short SmimeTimeout;
extern char *SmimeCertificates;
extern char *SmimeKeys;
extern char *SmimeEncryptWith;
extern char *SmimeCALocation;
extern char *SmimeVerifyCommand;
extern char *SmimeVerifyOpaqueCommand;
extern char *SmimeDecryptCommand;
extern char *SmimeSignCommand;
extern char *SmimeSignDigestAlg;
extern char *SmimeEncryptCommand;
extern char *SmimeGetSignerCertCommand;
extern char *SmimePk7outCommand;
extern char *SmimeGetCertCommand;
extern char *SmimeImportCertCommand;
extern char *SmimeGetCertEmailCommand;
extern char *SmimeSelfEncryptAs;

extern int NmOpenTimeout;
extern char *NmDefaultUri;
extern char *NmExcludeTags;
extern char *NmUnreadTag;
extern char *VfolderFormat;
extern int NmDbLimit;
extern char *NmQueryType;
extern char *NmRecordTags;
extern int NmQueryWindowDuration;
extern char *NmQueryWindowTimebase;
extern int NmQueryWindowCurrentPosition;
extern char *NmQueryWindowCurrentSearch;

extern bool Allow8bit;
extern bool AllowAnsi;
extern bool ArrowCursor;
extern bool AsciiChars;
extern bool Askbcc;
extern bool Askcc;
extern bool AskFollowUp;
extern bool AskXCommentTo;
extern bool AttachSplit;
extern bool Autoedit;
extern bool AutoTag;
extern bool Beep;
extern bool BeepNew;
extern bool BounceDelivered;
extern bool BrailleFriendly;
extern bool ChangeFolderNext;
extern bool CheckMboxSize;
extern bool CheckNew;
extern bool CollapseAll;
extern bool CollapseFlagged;
extern bool CollapseUnread;
extern bool Confirmappend;
extern bool Confirmcreate;
extern bool CryptAutoencrypt;
extern bool CryptAutopgp;
extern bool CryptAutosign;
extern bool CryptAutosmime;
extern bool CryptConfirmhook;
extern bool CryptOpportunisticEncrypt;
extern bool CryptReplyencrypt;
extern bool CryptReplysign;
extern bool CryptReplysignencrypted;
extern bool CryptTimestamp;
extern bool CryptUseGpgme;
extern bool CryptUsePka;
extern bool DeleteUntag;
extern bool DigestCollapse;
extern bool DuplicateThreads;
extern bool EditHeaders;
extern bool EncodeFrom;
extern bool FastReply;
extern bool FccClear;
extern bool FlagSafe;
extern bool FollowupTo;
extern bool ForceName;
extern bool ForwardDecode;
extern bool ForwardDecrypt;
extern bool ForwardQuote;
extern bool ForwardReferences;
extern bool Hdrs;
extern bool Header;
extern bool HeaderCacheCompress;
extern bool HeaderColorPartial;
extern bool Help;
extern bool HiddenHost;
extern bool HideLimited;
extern bool HideMissing;
extern bool HideThreadSubject;
extern bool HideTopLimited;
extern bool HideTopMissing;
extern bool HistoryRemoveDups;
extern bool HonorDisposition;
extern bool IdnDecode;
extern bool IdnEncode;
extern bool IgnoreLinearWhiteSpace;
extern bool IgnoreListReplyTo;
extern bool ImapCheckSubscribed;
extern bool ImapIdle;
extern bool ImapListSubscribed;
extern bool ImapPassive;
extern bool ImapPeek;
extern bool ImapServernoise;
extern bool ImplicitAutoview;
extern bool IncludeOnlyfirst;
extern bool KeepFlagged;
extern bool MailcapSanitize;
extern bool MailCheckRecent;
extern bool MailCheckStats;
extern bool MaildirCheckCur;
extern bool MaildirHeaderCacheVerify;
extern bool MaildirTrash;
extern bool Markers;
extern bool MarkOld;
extern bool MenuMoveOff; /**< allow menu to scroll past last entry */
extern bool MenuScroll;  /**< scroll menu instead of implicit next-page */
extern bool MessageCacheClean;
extern bool MetaKey; /**< interpret ALT-x as ESC-x */
extern bool Metoo;
extern bool MhPurge;
extern bool MimeForwardDecode;
extern bool MimeSubject; /**< encode subject line with RFC2047 */
extern bool MimeTypeQueryFirst;
extern bool NarrowTree;
extern bool NmRecord;
extern bool NntpListgroup;
extern bool NntpLoadDescription;
extern bool PagerStop;
extern bool PgpAutoDecode;
extern bool PgpAutoinline;
extern bool PgpCheckExit;
extern bool PgpIgnoreSubkeys;
extern bool PgpLongIds;
extern bool PgpReplyinline;
extern bool PgpRetainableSigs;
extern bool PgpSelfEncrypt;
extern bool PgpShowUnusable;
extern bool PgpStrictEnc;
extern bool PgpUseGpgAgent;
extern bool PipeDecode;
extern bool PipeSplit;
extern bool PopAuthTryAll;
extern bool PopLast;
extern bool PostponeEncrypt;
extern bool PrintDecode;
extern bool PrintSplit;
extern bool PromptAfter;
extern bool ReadOnly;
extern bool ReflowSpaceQuotes;
extern bool ReflowText;
extern bool ReplySelf;
extern bool ReplyWithXorig;
extern bool Resolve;
extern bool ResumeDraftFiles;
extern bool ResumeEditedDraftFiles;
extern bool ReverseAlias;
extern bool ReverseName;
extern bool ReverseRealname;
extern bool Rfc2047Parameters;
extern bool SaveAddress;
extern bool SaveEmpty;
extern bool SaveName;
extern bool SaveUnsubscribed;
extern bool Score;
extern bool ShowNewNews;
extern bool ShowOnlyUnread;
extern bool SidebarFolderIndent;
extern bool SidebarNewMailOnly;
extern bool SidebarNextNewWrap;
extern bool SidebarOnRight;
extern bool SidebarShortPath;
extern bool SidebarVisible;
extern bool SigDashes;
extern bool SigOnTop;
extern bool SmartWrap;
extern bool SmimeAskCertLabel;
extern bool SmimeDecryptUseDefaultKey;
extern bool SmimeIsDefault;
extern bool SmimeSelfEncrypt;
extern bool SortRe;
extern bool SslForceTls;
extern bool SslUseSslv2;
extern bool SslUseSslv3;
extern bool SslUsesystemcerts;
extern bool SslUseTlsv11;
extern bool SslUseTlsv12;
extern bool SslUseTlsv1;
extern bool SslVerifyDates;
extern bool SslVerifyHost;
extern bool SslVerifyPartialChains;
extern bool StatusOnTop;
extern bool StrictThreads;
extern bool Suspend;
extern bool TextFlowed;
extern bool ThoroughSearch;
extern bool ThreadReceived;
extern bool Tilde;
extern bool TsEnabled;
extern bool UncollapseJump;
extern bool UncollapseNew;
extern bool Use8bitmime;
extern bool UseDomain;
extern bool UseEnvelopeFrom;
extern bool UseFrom;
extern bool UseIpv6;
extern bool UserAgent;
extern bool VirtualSpoolfile;
extern bool WaitKey;
extern bool Weed;
extern bool WrapSearch;
extern bool WriteBcc;
extern bool XCommentTo;

extern unsigned char AbortNoattach;
extern unsigned char AbortNosubject;
extern unsigned char AbortUnmodified;
extern unsigned char Bounce;
extern unsigned char CatchupNewsgroup;
extern unsigned char Copy;
extern unsigned char CryptVerifySig;
extern unsigned char Delete;
extern unsigned char FccAttach;
extern unsigned char FollowupToPoster;
extern unsigned char ForwardEdit;
extern unsigned char HonorFollowupTo;
extern unsigned char Include;
extern unsigned char MimeForward;
extern unsigned char MimeForwardRest;
extern unsigned char Move;
extern unsigned char PgpEncryptSelf;
extern unsigned char PgpMimeAuto;
extern unsigned char PopDelete;
extern unsigned char PopReconnect;
extern unsigned char PostModerated;
extern unsigned char Postpone;
extern unsigned char Print;
extern unsigned char Quit;
extern unsigned char Recall;
extern unsigned char ReplyTo;
extern unsigned char SmimeEncryptSelf;
extern unsigned char SslStarttls;

extern char *AssumedCharset;
extern char *Charset;      

extern short ConnectTimeout;
extern const char *CertificateFile;
extern const char *EntropyFile;
extern const char *SslCiphers;
extern const char *SslClientCert;
extern const char *SslCaCertificatesFile;
extern short SslMinDhPrimeBits;
extern const char *Preconnect;
extern const char *Tunnel;

extern struct Regex *GecosMask;
extern struct Regex *Mask;
extern struct Regex *QuoteRegexp;
extern struct Regex *ReplyRegexp;
extern struct Regex *Smileys;

extern short MailCheck;
extern short MailCheckStatsInterval;
extern short MboxType;

extern short PgpSortKeys;
extern short SidebarSortMethod;
extern short Sort;
extern short SortAlias;
extern short SortAux;
extern short SortBrowser;

#endif /* _DUMP_VARS_H */
