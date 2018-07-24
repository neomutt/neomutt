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

#include <stdbool.h>

struct Address *EnvelopeFromAddress;
struct Address *From;

char *AliasFile;
char *AliasFormat;
char *AttachSep;
char *Attribution;
char *AttributionLocale;
char *AttachCharset;
char *AttachFormat;
struct Regex *AttachKeyword;
char *ComposeFormat;
char *ConfigCharset;
char *ContentType;
char *DefaultHook;
char *DateFormat;
char *DisplayFilter;
char *DsnNotify;
char *DsnReturn;
char *Editor;
char *EmptySubject;
char *Escape;
char *FolderFormat;
char *ForwardAttributionIntro;
char *ForwardAttributionTrailer;
char *ForwardFormat;
char *Hostname;
struct MbTable *FromChars;
char *IndexFormat;
char *HistoryFile;

char *ImapAuthenticators;
char *ImapDelimChars;
char *ImapHeaders;
char *ImapLogin;
char *ImapPass;
char *ImapUser;
char *Mbox;
char *Ispell;
char *MailcapPath;
char *Folder;
char *MessageCachedir;
char *HeaderCache;
char *HeaderCacheBackend;
char *HeaderCachePageSize;
char *MarkMacroPrefix;
char *MhSeqFlagged;
char *MhSeqReplied;
char *MhSeqUnseen;
char *MimeTypeQueryCommand;
char *MessageFormat;

short NetInc;

char *Mixmaster;
char *MixEntryFormat;

char *GroupIndexFormat;
char *Inews;
char *NewsCacheDir;
char *NewsServer;
char *NewsgroupsCharset;
char *NewsRc;
char *NntpAuthenticators;
char *NntpUser;
char *NntpPass;
char *Record;
char *Pager;
char *PagerFormat;
char *PipeSep;
char *PopAuthenticators;
short PopCheckinterval;
char *PopHost;
char *PopPass;
char *PopUser;
char *PostIndentString;
char *Postponed;
char *PostponeEncryptAs;
char *IndentString;
char *PrintCommand;
char *NewMailCommand;
char *QueryCommand;
char *QueryFormat;
char *RealName;
short SearchContext;
char *SendCharset;
char *Sendmail;
char *Shell;
char *ShowMultipartAlternative;
char *SidebarDelimChars;
char *SidebarDividerChar;
char *SidebarFormat;
char *SidebarIndentString;
char *Signature;
char *SimpleSearch;
char *SmtpAuthenticators;
char *SmtpPass;
char *SmtpUrl;
char *SpoolFile;
char *SpamSeparator;
struct MbTable *StatusChars;
char *StatusFormat;
char *Tmpdir;
struct MbTable *ToChars;
struct MbTable *FlagChars;
char *Trash;
char *TSStatusFormat;
char *TSIconFormat;
char *Visual;

char *HiddenTags;

short NntpPoll;
short NntpContext;

short DebugLevel;
char *DebugFile;

short History;
short MenuContext;
short PagerContext;
short PagerIndexLines;
short ReadInc;
short ReflowWrap;
short SaveHistory;
short SendmailWait;
short SleepTime;
short SkipQuotedOffset;
short TimeInc;
short Timeout;
short Wrap;
short WrapHeaders;
short WriteInc;

short ScoreThresholdDelete;
short ScoreThresholdRead;
short ScoreThresholdFlag;

short SidebarComponentDepth;
short SidebarWidth;
short ImapKeepalive;
short ImapPipelineDepth;
short ImapPollTimeout;

struct Regex *PgpGoodSign;
struct Regex *PgpDecryptionOkay;
char *PgpSignAs;
short PgpTimeout;
char *PgpEntryFormat;
char *PgpClearSignCommand;
char *PgpDecodeCommand;
char *PgpVerifyCommand;
char *PgpDecryptCommand;
char *PgpSignCommand;
char *PgpEncryptSignCommand;
char *PgpEncryptOnlyCommand;
char *PgpImportCommand;
char *PgpExportCommand;
char *PgpVerifyKeyCommand;
char *PgpListSecringCommand;
char *PgpListPubringCommand;
char *PgpGetkeysCommand;
char *PgpSelfEncryptAs;

char *SmimeDefaultKey;
short SmimeTimeout;
char *SmimeCertificates;
char *SmimeKeys;
char *SmimeEncryptWith;
char *SmimeCALocation;
char *SmimeVerifyCommand;
char *SmimeVerifyOpaqueCommand;
char *SmimeDecryptCommand;
char *SmimeSignCommand;
char *SmimeSignDigestAlg;
char *SmimeEncryptCommand;
char *SmimeGetSignerCertCommand;
char *SmimePk7outCommand;
char *SmimeGetCertCommand;
char *SmimeImportCertCommand;
char *SmimeGetCertEmailCommand;
char *SmimeSelfEncryptAs;

int NmOpenTimeout;
char *NmDefaultUri;
char *NmExcludeTags;
char *NmUnreadTag;
char *VfolderFormat;
int NmDbLimit;
char *NmQueryType;
char *NmRecordTags;
int NmQueryWindowDuration;
char *NmQueryWindowTimebase;
int NmQueryWindowCurrentPosition;
char *NmQueryWindowCurrentSearch;

bool Allow8bit;
bool AllowAnsi;
bool ArrowCursor;
bool AsciiChars;
bool Askbcc;
bool Askcc;
bool AskFollowUp;
bool AskXCommentTo;
bool AttachSplit;
bool Autoedit;
bool AutoTag;
bool Beep;
bool BeepNew;
bool BounceDelivered;
bool BrailleFriendly;
bool ChangeFolderNext;
bool CheckMboxSize;
bool CheckNew;
bool CollapseAll;
bool CollapseFlagged;
bool CollapseUnread;
bool Confirmappend;
bool Confirmcreate;
bool CryptAutoencrypt;
bool CryptAutopgp;
bool CryptAutosign;
bool CryptAutosmime;
bool CryptConfirmhook;
bool CryptOpportunisticEncrypt;
bool CryptReplyencrypt;
bool CryptReplysign;
bool CryptReplysignencrypted;
bool CryptTimestamp;
bool CryptUseGpgme;
bool CryptUsePka;
bool DeleteUntag;
bool DigestCollapse;
bool DuplicateThreads;
bool EditHeaders;
bool EncodeFrom;
bool FastReply;
bool FccClear;
bool FlagSafe;
bool FollowupTo;
bool ForceName;
bool ForwardDecode;
bool ForwardDecrypt;
bool ForwardQuote;
bool ForwardReferences;
bool Hdrs;
bool Header;
bool HeaderCacheCompress;
bool HeaderColorPartial;
bool Help;
bool HiddenHost;
bool HideLimited;
bool HideMissing;
bool HideThreadSubject;
bool HideTopLimited;
bool HideTopMissing;
bool HistoryRemoveDups;
bool HonorDisposition;
bool IdnDecode;
bool IdnEncode;
bool IgnoreLinearWhiteSpace;
bool IgnoreListReplyTo;
bool ImapCheckSubscribed;
bool ImapIdle;
bool ImapListSubscribed;
bool ImapPassive;
bool ImapPeek;
bool ImapServernoise;
bool ImplicitAutoview;
bool IncludeOnlyfirst;
bool KeepFlagged;
bool MailcapSanitize;
bool MailCheckRecent;
bool MailCheckStats;
bool MaildirCheckCur;
bool MaildirHeaderCacheVerify;
bool MaildirTrash;
bool Markers;
bool MarkOld;
bool MenuMoveOff; /**< allow menu to scroll past last entry */
bool MenuScroll;  /**< scroll menu instead of implicit next-page */
bool MessageCacheClean;
bool MetaKey; /**< interpret ALT-x as ESC-x */
bool Metoo;
bool MhPurge;
bool MimeForwardDecode;
bool MimeSubject; /**< encode subject line with RFC2047 */
bool MimeTypeQueryFirst;
bool NarrowTree;
bool NmRecord;
bool NntpListgroup;
bool NntpLoadDescription;
bool PagerStop;
bool PgpAutoDecode;
bool PgpAutoinline;
bool PgpCheckExit;
bool PgpIgnoreSubkeys;
bool PgpLongIds;
bool PgpReplyinline;
bool PgpRetainableSigs;
bool PgpSelfEncrypt;
bool PgpShowUnusable;
bool PgpStrictEnc;
bool PgpUseGpgAgent;
bool PipeDecode;
bool PipeSplit;
bool PopAuthTryAll;
bool PopLast;
bool PostponeEncrypt;
bool PrintDecode;
bool PrintSplit;
bool PromptAfter;
bool ReadOnly;
bool ReflowSpaceQuotes;
bool ReflowText;
bool ReplySelf;
bool ReplyWithXorig;
bool Resolve;
bool ResumeDraftFiles;
bool ResumeEditedDraftFiles;
bool ReverseAlias;
bool ReverseName;
bool ReverseRealname;
bool Rfc2047Parameters;
bool SaveAddress;
bool SaveEmpty;
bool SaveName;
bool SaveUnsubscribed;
bool Score;
bool ShowNewNews;
bool ShowOnlyUnread;
bool SidebarFolderIndent;
bool SidebarNewMailOnly;
bool SidebarNextNewWrap;
bool SidebarOnRight;
bool SidebarShortPath;
bool SidebarVisible;
bool SigDashes;
bool SigOnTop;
bool SmartWrap;
bool SmimeAskCertLabel;
bool SmimeDecryptUseDefaultKey;
bool SmimeIsDefault;
bool SmimeSelfEncrypt;
bool SortRe;
bool SslForceTls;
bool SslUseSslv2;
bool SslUseSslv3;
bool SslUsesystemcerts;
bool SslUseTlsv11;
bool SslUseTlsv12;
bool SslUseTlsv1;
bool SslVerifyDates;
bool SslVerifyHost;
bool SslVerifyPartialChains;
bool StatusOnTop;
bool StrictThreads;
bool Suspend;
bool TextFlowed;
bool ThoroughSearch;
bool ThreadReceived;
bool Tilde;
bool TsEnabled;
bool UncollapseJump;
bool UncollapseNew;
bool Use8bitmime;
bool UseDomain;
bool UseEnvelopeFrom;
bool UseFrom;
bool UseIpv6;
bool UserAgent;
bool VirtualSpoolfile;
bool WaitKey;
bool Weed;
bool WrapSearch;
bool WriteBcc;
bool XCommentTo;

unsigned char AbortNoattach;
unsigned char AbortNosubject;
unsigned char AbortUnmodified;
unsigned char Bounce;
unsigned char CatchupNewsgroup;
unsigned char Copy;
unsigned char CryptVerifySig;
unsigned char Delete;
unsigned char FccAttach;
unsigned char FollowupToPoster;
unsigned char ForwardEdit;
unsigned char HonorFollowupTo;
unsigned char Include;
unsigned char MimeForward;
unsigned char MimeForwardRest;
unsigned char Move;
unsigned char PgpEncryptSelf;
unsigned char PgpMimeAuto;
unsigned char PopDelete;
unsigned char PopReconnect;
unsigned char PostModerated;
unsigned char Postpone;
unsigned char Print;
unsigned char Quit;
unsigned char Recall;
unsigned char ReplyTo;
unsigned char SmimeEncryptSelf;
unsigned char SslStarttls;

char *AssumedCharset;
char *Charset;

short ConnectTimeout;
const char *CertificateFile;
const char *EntropyFile;
const char *SslCiphers;
const char *SslClientCert;
const char *SslCaCertificatesFile;
short SslMinDhPrimeBits;
const char *Preconnect;
const char *Tunnel;

struct Regex *GecosMask;
struct Regex *Mask;
struct Regex *QuoteRegexp;
struct Regex *ReplyRegexp;
struct Regex *Smileys;

short MailCheck;
short MailCheckStatsInterval;
short MboxType;

short PgpSortKeys;
short SidebarSortMethod;
short Sort;
short SortAlias;
short SortAux;
short SortBrowser;
