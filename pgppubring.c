/*
 * Copyright (C) 1997 Thomas Roessler <roessler@guug.de>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "sha.h"
#include "mutt.h"
#include "pgp.h"

#define CHUNKSIZE 1024

static unsigned char *pbuf = NULL;
static size_t plen = 0;

enum packet_tags {
  PT_RES0 = 0,		/* reserved */
  PT_ESK,		/* Encrypted Session Key */
  PT_SIG,		/* Signature Packet */
  PT_CESK,		/* Conventionally Encrypted Session Key Packet */
  PT_OPS,		/* One-Pass Signature Packet */
  PT_SECKEY,		/* Secret Key Packet */
  PT_PUBKEY,		/* Public Key Packet */
  PT_SUBSECKEY,		/* Secret Subkey Packet */
  PT_COMPRESSED,	/* Compressed Data Packet */
  PT_SKE,		/* Symmetrically Encrypted Data Packet */
  PT_MARKER,		/* Marker Packet */
  PT_LITERAL,		/* Literal Data Packet */
  PT_TRUST,		/* Trust Packet */
  PT_NAME,		/* Name Packet */
  PT_SUBKEY,		/* Subkey Packet */
  PT_RES15,		/* Reserved */
  PT_COMMENT		/* Comment Packet */
};

const char *pgp_packet_name[] = {
  "reserved",
  "Encrypted Session Key",
  "Signature Packet",
  "Conventionally Encrypted Session Key Packet",
  "One-Pass Signature Packet",
  "Secret Key Packet",
  "Public Key Packet",
  "Secret Subkey Packet",
  "Compressed Data Packet",
  "Symmetrically Encrypted Data Packet",
  "Marker Packet",
  "Literal Data Packet",
  "Trust Packet",
  "Name Packet",
  "Subkey Packet",
  "Reserved",
  "Comment Packet"
};

const char *pgp_pkalgbytype(unsigned char type)
{
  switch(type)
  {
    case 1: return "RSA";
    case 2: return "RSA";
    case 3: return "RSA";
    case 16: return "ElG";
    case 17: return "DSA";
    case 20: return "ElG";
    default: return "unk";
  }
}


static struct
{
  char *pkalg;
  char *micalg;
}
pktomic[] = 
{
  { "RSA", "pgp-md5" },
  { "ElG", "pgp-rmd160" },
  { "DSA", "pgp-sha1" },
  { NULL, "x-unknown" }
};


const char *pgp_pkalg_to_mic(const char *alg)
{
  int i;
  
  for(i = 0; pktomic[i].pkalg; i++)
  {
    if(!strcasecmp(pktomic[i].pkalg, alg))
      break;
  }
  
  return pktomic[i].micalg;
}


/* unused */

#if 0

static const char *hashalgbytype(unsigned char type)
{
  switch(type)
  {
    case 1: return "MD5";
    case 2: return "SHA1";
    case 3: return "RIPE-MD/160";
    case 4: return "HAVAL";
    default: return "unknown";
  }
}

#endif

short pgp_canencrypt(unsigned char type)
{
  switch(type)
  {
    case 1:
    case 2:
    case 16:
    case 20:
    	return 1;
    default:
        return 0;
  }
}

short pgp_cansign(unsigned char type)
{
  switch(type)
  {
    case 1:
    case 3:
    case 16:
    case 17:
    case 20:
    	return 1;
    default:
    	return 0;
  }
}

/* return values: 
 *
 * 1 = sign only
 * 2 = encrypt only
 * 3 = both
 */

short pgp_get_abilities(unsigned char type)
{
  return (pgp_canencrypt(type) << 1) | pgp_cansign(type);
}

static int read_material(size_t material, size_t *used, FILE *fp)
{
  if(*used + material >= plen)
  {
    unsigned char *p;
    size_t nplen;
    
    nplen = *used + material + CHUNKSIZE;

    if(!(p = realloc(pbuf, nplen)))
    {
      mutt_perror("realloc");
      return -1;
    }
    plen = nplen;
    pbuf = p;
  }
      
  if(fread(pbuf + *used, 1, material, fp) < material)
  {
    mutt_perror("fread");
    return -1;
  }
  
  *used += material;
  return 0;
}

static unsigned char *pgp_read_packet(FILE *fp, size_t *len)
{
  size_t used = 0;
  long startpos;
  unsigned char ctb;
  unsigned char b;
  size_t material;
  
  startpos = ftell(fp);
  
  if(!plen)
  {
    plen = CHUNKSIZE;
    pbuf = safe_malloc(plen);
  }

  if(fread(&ctb, 1, 1, fp) < 1)
  {
    if(!feof(fp))
       mutt_perror("fread"); 
    goto bail;
  }

  if(!(ctb & 0x80))
  {
    goto bail;
  }

  if(ctb & 0x40)	/* handle PGP 5.0 packets. */
  {
    int partial = 0;
    pbuf[0] = ctb; used++;
    
    do {
      if(fread(&b, 1, 1, fp) < 1)
      {
	mutt_perror("fread");
	goto bail;
      }
      
      if(b < 192)
      {
	material = b;
	partial = 0;
	material -= 1;
      } 
      else if(192 <= b && b <= 223)
      {
	material = (b - 192) * 256;
	if(fread(&b, 1, 1, fp) < 1)
	{
	  mutt_perror("fread");
	  goto bail;
	}
	material += b + 192;
	partial = 0;
	material -= 2;
      }
      else if(b < 255)
      {
	material = 1 << (b & 0x1f);
	partial = 1;
	material -= 1;
      }
      else /* b == 255 */
      {
	unsigned char buf[4];
	if( fread( buf, 4, 1, fp ) < 1)
	{
	   mutt_perror("fread");
	   goto bail;
	}
	/*assert( sizeof(material) >= 4 );*/
	material  = buf[0] << 24;
	material |= buf[1] << 16;
	material |= buf[2] << 8;
	material |= buf[3];
	partial = 0;
	material -= 5;
      }
    
      if(read_material(material, &used, fp) == -1)
	goto bail;

    } while (partial);
  }
  else		/* Old-Style PGP */
  {
    int bytes = 0;
    pbuf[0] = 0x80 | ((ctb >> 2) & 0x0f);
    used++;
    
    switch(ctb & 0x03)
    {
      case 0:
      {
        if(fread(&b, 1, 1, fp) < 1)
	{
	  mutt_perror("fread");
	  goto bail;
	}
	
	material = b;
	break;
      }
    
      case 1:
      	bytes = 2;
      
      case 2:
      {
	int i;

	if(!bytes) bytes = 4;
	
	material = 0;
	
	for(i = 0; i < bytes; i++)
	{
	  if(fread(&b, 1, 1, fp) < 1)
	  {
	    mutt_perror("fread");
	    goto bail;
	  }
	  
	  material = (material << 8) + b;
	}
	break;
      }
      
      default:
        goto bail;
    }
    
    if(read_material(material, &used, fp) == -1)
      goto bail;
  }
  
  if(len)
    *len = used;
  
  return pbuf;
  
  bail:
  
  fseek(fp, startpos, SEEK_SET);
  return NULL;
}

static KEYINFO *pgp_new_keyinfo(void)
{
  KEYINFO *p;

  p = safe_malloc(sizeof(KEYINFO));
  p->keyid = NULL;
  p->address = NULL;
  p->flags = 0;
  p->next = NULL;
  p->keylen = 0;

  return p;
}

static KEYINFO *pgp_parse_pgp2_key(unsigned char *buff, size_t l)
{
  KEYINFO *p;
  unsigned char alg;
  size_t expl;
  unsigned long id;
  unsigned long gen_time = 0;
  unsigned short exp_days = 0;
  size_t j;
  int i, k;
  unsigned char scratch[LONG_STRING];
  
  if(l < 12)
    return NULL;
  
  p = pgp_new_keyinfo();
  
  for(i = 0, j = 2; i < 4; i++)
    gen_time = (gen_time << 8) + buff[j++];
  
  for(i = 0; i < 2; i++)
    exp_days = (exp_days << 8) + buff[j++];
  
  if(exp_days && time(NULL) > gen_time + exp_days * 24 * 3600)
    p->flags |= KEYFLAG_EXPIRED;
  
  alg = buff[j++];
	  
  p->algorithm = pgp_pkalgbytype(alg);
  p->flags |= pgp_get_abilities(alg);
  
  expl = 0;
  for(i = 0; i < 2; i++)
    expl = (expl << 8) + buff[j++];
  
  p->keylen = expl;
  
  expl = (expl + 7)/ 8;
  if(expl < 4)
    goto bailout;

  
  j += expl - 8;
  
  for(k = 0; k < 2; k++)
  {
    for(id = 0, i = 0; i < 4; i++)
      id = (id << 8) + buff[j++];
    
    snprintf((char *)scratch + k * 8, sizeof(scratch) - k * 8, 
	     "%08lX", id);
  }
  
  p->keyid = safe_strdup((char *)scratch);
  
  return p;
  
  bailout:
  
  safe_free((void **)&p);
  return NULL;
}

static void pgp_make_pgp3_fingerprint(unsigned char *buff, size_t l,
				      unsigned char *digest)
{
  unsigned char dummy;
  SHA_CTX context;

  SHA1_Init(&context);
  
  dummy = buff[0] & 0x3f;

  if(dummy == PT_SUBSECKEY || dummy == PT_SUBKEY || dummy == PT_SECKEY)
    dummy = PT_PUBKEY;

  dummy = (dummy << 2) | 0x81;
  SHA1_Update(&context, &dummy, 1);
  dummy = ((l - 1) >> 8) & 0xff;
  SHA1_Update(&context, &dummy, 1);
  dummy = (l - 1) & 0xff; 
  SHA1_Update(&context, &dummy, 1);
  SHA1_Update(&context, buff + 1, l - 1);
  SHA1_Final(digest, &context);

}

static void skip_bignum(unsigned char *buff, size_t l, size_t j,
			size_t *toff, size_t n)
{
  size_t len;
  
  do
  {
    len = (buff[j] << 8) + buff[j+1];
    j += (len + 7) / 8 + 2;
  } while(j <= l && --n > 0);
  
  if(toff) *toff = j;
}
      

static KEYINFO *pgp_parse_pgp3_key(unsigned char *buff, size_t l)
{
  KEYINFO *p;
  unsigned char alg;
  unsigned char digest[SHA_DIGEST_LENGTH];
  unsigned char scratch[LONG_STRING];
  unsigned long gen_time = 0;
  unsigned long id;
  int i, k;
  short len;
  size_t j;
  
  p = pgp_new_keyinfo();
  j = 2;
  
  for(i = 0; i < 4; i++)
    gen_time = (gen_time << 8) + buff[j++];

  p->gen_time = gen_time;
  
  alg = buff[j++];
  
  p->algorithm = pgp_pkalgbytype(alg);
  p->flags |= pgp_get_abilities(alg);

  if (alg == 17)
    skip_bignum(buff, l, j, &j, 3);
  else if(alg == 16 || alg == 20 )
    skip_bignum(buff, l, j, &j, 2);
  
  len = (buff[j] << 8) + buff[j+1];
  p->keylen = len;

  if (alg >=1 && alg <= 3)
    skip_bignum(buff, l, j, &j, 2);
  else if(alg == 17 || alg == 16)
    skip_bignum(buff, l, j, &j, 1);
  
  pgp_make_pgp3_fingerprint(buff, j, digest);

  for(k = 0; k < 2; k++)
  {
    for(id = 0, i = SHA_DIGEST_LENGTH - 8 + k*4; 
	i < SHA_DIGEST_LENGTH + (k - 1) * 4; i++)
      id = (id << 8) + digest[i];
  
    snprintf((char *)scratch + k * 8, sizeof(scratch) - k * 8, "%08lX", id);
  }
  
  p->keyid = safe_strdup((char *)scratch);

  
  return p;
}

static KEYINFO *pgp_parse_keyinfo(unsigned char *buff, size_t l)
{
  if(!buff || l < 2)
    return NULL;
  
  switch(buff[1])
  {
    case 2:
    case 3:
      return pgp_parse_pgp2_key(buff, l);
    case 4:
      return pgp_parse_pgp3_key(buff, l);
    default:
      return NULL;
  }
}

static int pgp_parse_pgp2_sig(unsigned char *buff, size_t l, KEYINFO *p)
{
  unsigned char sigtype;
  long sig_gen_time;
  unsigned long signerid;
  size_t j;
  int i;
  
  if(l < 22)
    return -1;
	  
  j = 3;
  sigtype = buff[j++];
	  
  sig_gen_time = 0;
  for(i = 0; i < 4; i++)
    sig_gen_time = (sig_gen_time << 8) + buff[j++];
  
  j += 4;
  signerid = 0;
  for(i = 0; i < 4; i++)
    signerid = (signerid << 8) + buff[j++];
  
  if(sigtype == 0x20 || sigtype == 0x28)
    p->flags |= KEYFLAG_REVOKED;
  
  return 0;
}

static int pgp_parse_pgp3_sig(unsigned char *buff, size_t l, KEYINFO *p)
{
  unsigned char sigtype;
  unsigned char pkalg;
  unsigned char hashalg;
  unsigned char skt;
  long sig_gen_time = -1;
  long validity = -1;
  long key_validity = -1;
  long signerid = 0;
  size_t ml;
  size_t j;
  int i;
  short ii;
  short have_critical_spks=0;
  
  if(l < 7)
    return -1;
  
  j = 2;
  
  sigtype = buff[j++];
  pkalg = buff[j++];
  hashalg = buff[j++];
  
  for(ii = 0; ii < 2; ii++)
  {
    size_t skl;
    size_t nextone;
    
    ml = (buff[j] << 8) + buff[j+1];
    j += 2;
    
    if(j + ml > l) break;
    
    nextone = j;
    while(ml)
    {
      j = nextone;
      skl = buff[j++];
      if(!--ml) break;
      
      if(skl >= 192)
      {
	skl = (skl - 192) * 256 + buff[j++] + 192;
	if(!--ml) break;
      }
      
      if((int) ml - (int) skl < 0)
	break;
      ml -= skl;
      
      nextone = j + skl;
      skt = buff[j++];
      
      switch(skt & 0x7f)
      {
	case 2: /* creation time */ 
	{
	  if(skl < 4)
	    break;
	  sig_gen_time = 0;
	  for(i = 0; i < 4; i++)
	    sig_gen_time = (sig_gen_time << 8) + buff[j++];
	  
	  break;
	}
	case 3: /* expiration time */ 
	{
	  if(skl < 4)
	    break;
	  validity = 0;
	  for(i = 0; i < 4; i++)
	    validity = (validity << 8) + buff[j++];
	  break;
	}
	case 9: /* key expiration time */ 
	{
	  if(skl < 4)
	    break;
	  key_validity = 0;
	  for(i = 0; i < 4; i++)
	    key_validity = (key_validity << 8) + buff[j++];
	  break;
	}
	case 16: /* issuer key ID */
	{
	  if(skl < 8)
	    break;
	  j += 4;
	  signerid = 0;
	  for(i = 0; i < 4; i++)
	    signerid = (signerid << 8) + buff[j++];
	  break;
	}
	case 10: /* CMR key */	break;
	case 4: /* exportable */ 
	case 5: /* trust */
	case 6: /* regexp */
	case 7: /* revocable */
	case 11: /* Pref. symm. alg. */
	case 12: /* revocation key */
	case 20: /* notation data */
	case 21: /* pref. hash */ 
	case 22: /* pref. comp.alg. */
	case 23: /* key server prefs. */ 
	case 24: /* pref. key server */ 
	default:
	{
	  if(skt & 0x80)
	    have_critical_spks = 1;
	}
      }
    }
    j = nextone;
  }
  
  if(sigtype == 0x20 || sigtype == 0x28)
    p->flags |= KEYFLAG_REVOKED;
  if(key_validity != -1 && time(NULL) > p->gen_time + key_validity)
    p->flags |= KEYFLAG_EXPIRED;
  if(have_critical_spks)
    p->flags |= KEYFLAG_CRITICAL;

  return 0;
  
}
  

static int pgp_parse_sig(unsigned char *buff, size_t l, KEYINFO *p)
{
  if(!buff || l < 2 || !p)
    return -1;
  
  switch(buff[1])
  {
    case 2:
    case 3:
      return pgp_parse_pgp2_sig(buff, l, p);
    case 4:
      return pgp_parse_pgp3_sig(buff, l, p);
    default:
    return -1;
  }
}

  
static KEYINFO *pgp_read_keyring(const char *fname)
{
  FILE *fp;
  unsigned char *buff;
  unsigned char pt = 0;
  unsigned char last_pt;
  size_t l;
  KEYINFO *db = NULL, **end, *p = NULL;
  KEYINFO *supkey = NULL;
  PGPUID *uid = NULL;
  LIST **addr = NULL;
  
  if(!(fp = fopen(fname, "r")))
  {
    mutt_perror("fopen");
    return NULL;
      
  }

  end = &db;
  
  while((buff = pgp_read_packet(fp, &l)) != NULL)
  {
    last_pt = pt;
    pt = buff[0] & 0x3f;

    if(l < 1)
      continue;

    switch(pt)
    {
      case PT_SECKEY:
      case PT_PUBKEY:
      case PT_SUBKEY:
      case PT_SUBSECKEY:
      {
	if(p)
	  end = &(p->next);
	
	if(!(*end = p = pgp_parse_keyinfo(buff, l)))
	   break;
	
	addr = &p->address;

	if(pt == PT_SUBKEY || pt == PT_SUBSECKEY)
	{
	  p->flags |= KEYFLAG_SUBKEY;
	  p->mainkey = supkey;
	}
	else
	  supkey = p;
	
	break;
      }
      
      case PT_SIG:
      {
	pgp_parse_sig(buff, l, p);
	break;
      }
      case PT_TRUST:
      {
	if(last_pt == PT_SECKEY || last_pt == PT_PUBKEY ||
	   last_pt == PT_SUBKEY || last_pt == PT_SUBSECKEY)
	{
	  if(buff[1] & 0x20)
	    p->flags |= KEYFLAG_DISABLED;
	}
	else if(last_pt == PT_NAME)
	  uid->trust = buff[1];
	break;
      }
      case PT_NAME:
      {
	char *chr;
	
	if(!addr) break;
	
	chr = safe_malloc(l);
	memcpy(chr, buff + 1, l - 1);
	chr[l-1] = '\0';
	*addr = mutt_new_list();
	(*addr)->data = safe_malloc(sizeof(PGPUID));
	uid = (PGPUID *) (*addr)->data;
	uid->addr = chr;
	uid->trust = 0;
	addr = &(*addr)->next;
	
	/* the following tags are generated by
	 * pgp 2.6.3in.
	 */
	
	if(strstr(chr, "ENCR"))
	  p->flags |= KEYFLAG_PREFER_ENCRYPTION;
	if(strstr(chr, "SIGN"))
	  p->flags |= KEYFLAG_PREFER_SIGNING;
	
	break;
      }
    }
  }
  fclose(fp);
  return db;
}

KEYINFO *pgp_read_pubring(struct pgp_vinfo *pgp)
{
  return pgp_read_keyring(NONULL(*pgp->pubring));
}

KEYINFO *pgp_read_secring(struct pgp_vinfo *pgp)
{
  return pgp_read_keyring(NONULL(*pgp->secring));
}

void pgp_close_keydb (KEYINFO **ki)
{
  KEYINFO *tmp, *k = *ki;
  PGPUID *uid;
  LIST *p, *q;
  
  while (k)
  {
    if (k->keyid) safe_free ((void **)&k->keyid);
    for(q = k->address; q; q = p)
    {
      uid = (PGPUID *) q->data;
      p = q->next;

      safe_free((void **)&uid->addr);
      safe_free((void **)&q->data);
      safe_free((void **)&q);
    }
    tmp = k;
    k = k->next;
    safe_free ((void **)&tmp);
  }
  *ki = NULL;
}
