/*
 SHA-1 in C

 By Steve Reid <steve@edmweb.com>, with small changes to make it
 fit into mutt by Thomas Roessler <roessler@does-not-exist.org>.

*/

#ifndef _SHA1_H
# define _SHA1_H

# include "types.h"

typedef struct {
  UINT4 state[5];
  UINT4 count[2];
  unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(UINT4 state[5], const unsigned char buffer[64]);
void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, const unsigned char* data, UINT4 len);
void SHA1Final(unsigned char digest[20], SHA1_CTX* context);

# define SHA1_Transform SHA1Transform
# define SHA1_Init SHA1Init
# define SHA1_Update SHA1Update
# define SHA1_Final SHA1Final

# define SHA_DIGEST_LENGTH 20

#endif

