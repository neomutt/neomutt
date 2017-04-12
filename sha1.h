/**
 * SHA-1 in C
 *
 * By Steve Reid <steve@edmweb.com>, with small changes to make it
 * fit into mutt by Thomas Roessler <roessler@does-not-exist.org>.
 */

#ifndef _MUTT_SHA1_H
#define _MUTT_SHA1_H 1

#include "crypthash.h"

typedef struct
{
  uint32_t state[5];
  uint32_t count[2];
  unsigned char buffer[64];
} SHA1_CTX;

void sha1_transform(uint32_t state[5], const unsigned char buffer[64]);
void sha1_init(SHA1_CTX *context);
void sha1_update(SHA1_CTX *context, const unsigned char *data, uint32_t len);
void sha1_final(unsigned char digest[20], SHA1_CTX *context);

#define SHA1_Transform sha1_transform
#define SHA1_Init sha1_init
#define SHA1_Update sha1_update
#define SHA1_Final sha1_final

#define SHA_DIGEST_LENGTH 20

#endif /* _MUTT_SHA1_H */
