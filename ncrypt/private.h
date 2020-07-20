/**
 * @file
 * Shared constants/structs that are private to libconn
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_NCRYPT_PRIVATE_H
#define MUTT_NCRYPT_PRIVATE_H

#include "config.h"
#include <stdbool.h>

struct Address;
struct CryptKeyInfo;
struct PgpKeyInfo;
struct SmimeKey;

extern bool            C_CryptUsePka;
extern bool            C_CryptConfirmhook;
extern bool            C_CryptOpportunisticEncrypt;
extern bool            C_CryptOpportunisticEncryptStrongKeys;
extern bool            C_CryptProtectedHeadersRead;
extern bool            C_CryptProtectedHeadersSave;
extern bool            C_CryptProtectedHeadersWrite;
extern bool            C_SmimeIsDefault;
extern bool            C_PgpIgnoreSubkeys;
extern bool            C_PgpLongIds;
extern bool            C_PgpShowUnusable;
extern bool            C_PgpAutoinline;
extern char *          C_PgpDefaultKey;
extern char *          C_PgpSignAs;
extern char *          C_PgpEntryFormat;
extern char *          C_SmimeDefaultKey;
extern char *          C_SmimeSignAs;
extern char *          C_SmimeEncryptWith;
extern char *          C_CryptProtectedHeadersSubject;
extern struct Address *C_EnvelopeFromAddress;
extern bool            C_CryptTimestamp;
extern unsigned char   C_PgpEncryptSelf;
extern unsigned char   C_PgpMimeAuto;
extern bool            C_PgpRetainableSigs;
extern bool            C_PgpSelfEncrypt;
extern bool            C_PgpStrictEnc;
extern unsigned char   C_SmimeEncryptSelf;
extern bool            C_SmimeSelfEncrypt;
extern bool            C_CryptUseGpgme;
extern bool            C_PgpCheckExit;
extern bool            C_PgpCheckGpgDecryptStatusFd;
extern struct Regex *  C_PgpDecryptionOkay;
extern struct Regex *  C_PgpGoodSign;
extern long            C_PgpTimeout;
extern bool            C_PgpUseGpgAgent;
extern char *          C_PgpClearsignCommand;
extern char *          C_PgpDecodeCommand;
extern char *          C_PgpDecryptCommand;
extern char *          C_PgpEncryptOnlyCommand;
extern char *          C_PgpEncryptSignCommand;
extern char *          C_PgpExportCommand;
extern char *          C_PgpGetkeysCommand;
extern char *          C_PgpImportCommand;
extern char *          C_PgpListPubringCommand;
extern char *          C_PgpListSecringCommand;
extern char *          C_PgpSignCommand;
extern char *          C_PgpVerifyCommand;
extern char *          C_PgpVerifyKeyCommand;
extern short           C_PgpSortKeys;
extern bool            C_SmimeAskCertLabel;
extern char *          C_SmimeCaLocation;
extern char *          C_SmimeCertificates;
extern char *          C_SmimeDecryptCommand;
extern bool            C_SmimeDecryptUseDefaultKey;
extern char *          C_SmimeEncryptCommand;
extern char *          C_SmimeGetCertCommand;
extern char *          C_SmimeGetCertEmailCommand;
extern char *          C_SmimeGetSignerCertCommand;
extern char *          C_SmimeImportCertCommand;
extern char *          C_SmimeKeys;
extern char *          C_SmimePk7outCommand;
extern char *          C_SmimeSignCommand;
extern char *          C_SmimeSignDigestAlg;
extern long            C_SmimeTimeout;
extern char *          C_SmimeVerifyCommand;
extern char *          C_SmimeVerifyOpaqueCommand;
extern bool            C_PgpAutoDecode;
extern unsigned char   C_CryptVerifySig;

struct SmimeKey *smime_select_key(struct SmimeKey *keys, char *query);

#endif /* MUTT_NCRYPT_PRIVATE_H */
