/*	Copyright (C) 1995, 1996 Tom Lord
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */


/*
 * Tom Lord (lord@cygnus.com, lord@gnu.ai.mit.edu)
 */


#include "rxall.h"
#include "rxhash.h"



#ifdef __STDC__
static struct rx_hash *
default_hash_alloc (struct rx_hash_rules * rules)
#else
static struct rx_hash *
default_hash_alloc (rules)
     struct rx_hash_rules * rules;
#endif
{
  return (struct rx_hash *)malloc (sizeof (struct rx_hash));
}


#ifdef __STDC__
static struct rx_hash_item *
default_hash_item_alloc (struct rx_hash_rules * rules, void * value)
#else
static struct rx_hash_item *
default_hash_item_alloc (rules, value)
     struct rx_hash_rules * rules;
     void * value;
#endif
{
  struct rx_hash_item * it;
  it = (struct rx_hash_item *)malloc (sizeof (*it));
  if (it)
    {
      it->data = value;
      it->binding = 0;
    }
  return it;
}


#ifdef __STDC__
static void
default_free_hash (struct rx_hash * tab,
		    struct rx_hash_rules * rules)
#else
static void
default_free_hash (tab, rules)
     struct rx_hash * tab;
     struct rx_hash_rules * rules;
#endif
{
  free ((char *)tab);
}


#ifdef __STDC__
static void
default_free_hash_item (struct rx_hash_item * item,
			 struct rx_hash_rules * rules)
#else
static void
default_free_hash_item (item, rules)
     struct rx_hash_item * item;
     struct rx_hash_rules * rules;
#endif
{
  free ((char *)item);
}

#ifdef __STDC__
static int 
default_eq (void * va, void * vb)
#else
static int 
default_eq (va, vb)
     void * va;
     void * vb;
#endif
{
  return va == vb;
}



#define EQ(rules) ((rules && rules->eq) ? rules->eq : default_eq)
#define HASH_ALLOC(rules) ((rules && rules->hash_alloc) ? rules->hash_alloc : default_hash_alloc)
#define FREE_HASH(rules) ((rules && rules->free_hash) ? rules->free_hash : default_free_hash)
#define ITEM_ALLOC(rules) ((rules && rules->hash_item_alloc) ? rules->hash_item_alloc : default_hash_item_alloc)
#define FREE_HASH_ITEM(rules) ((rules && rules->free_hash_item) ? rules->free_hash_item : default_free_hash_item)


static unsigned long rx_hash_masks[4] =
{
  0x12488421,
  0x96699669,
  0xbe7dd7eb,
  0xffffffff
};

/* hash to bucket */
#define JOIN_BYTE(H, B)  (((H) + ((H) << 3) + (B)) & 0xf)

#define H2B(X) JOIN_BYTE (JOIN_BYTE (JOIN_BYTE ((X & 0xf), ((X>>8) & 0xf)), ((X>>16) & 0xf)), ((X>>24) & 0xf))

#define BKTS 16

/* Hash tables */
#ifdef __STDC__
struct rx_hash_item * 
rx_hash_find (struct rx_hash * table,
	      unsigned long hash,
	      void * value,
	      struct rx_hash_rules * rules)
#else
struct rx_hash_item * 
rx_hash_find (table, hash, value, rules)
     struct rx_hash * table;
     unsigned long hash;
     void * value;
     struct rx_hash_rules * rules;
#endif
{
  rx_hash_eq eq = EQ (rules);
  int maskc = 0;
  long mask = rx_hash_masks [0];
  int bucket = H2B(hash & mask);

  while (RX_bitset_member (&table->nested_p, bucket))
    {
      table = (struct rx_hash *)(table->children [bucket]);
      ++maskc;
      mask = rx_hash_masks[maskc];
      bucket = H2B (hash & mask);
    }

  {
    struct rx_hash_item * it;
    it = (struct rx_hash_item *)(table->children[bucket]);
    while (it)
      if (eq (it->data, value))
	return it;
      else
	it = it->next_same_hash;
  }

  return 0;
}


#ifdef __STDC__
static int 
listlen (struct rx_hash_item * bucket)
#else
static int 
listlen (bucket)
     struct rx_hash_item * bucket;
#endif
{
  int i;
  for (i = 0; bucket; ++i, bucket = bucket->next_same_hash)
    ;
  return i;
}

#ifdef __STDC__
static int
overflows (struct rx_hash_item * bucket)
#else
static int
overflows (bucket)
     struct rx_hash_item * bucket;
#endif
{
  return !(   bucket
	   && bucket->next_same_hash
	   && bucket->next_same_hash->next_same_hash
	   && bucket->next_same_hash->next_same_hash->next_same_hash);
}


#ifdef __STDC__
struct rx_hash_item *
rx_hash_store (struct rx_hash * table,
	       unsigned long hash,
	       void * value,
	       struct rx_hash_rules * rules)
#else
struct rx_hash_item *
rx_hash_store (table, hash, value, rules)
     struct rx_hash * table;
     unsigned long hash;
     void * value;
     struct rx_hash_rules * rules;
#endif
{
  rx_hash_eq eq = EQ (rules);
  int maskc = 0;
  long mask = rx_hash_masks [0];
  int bucket = H2B(hash & mask);
  int depth = 0;
  
  while (RX_bitset_member (&table->nested_p, bucket))
    {
      table = (struct rx_hash *)(table->children [bucket]);
      ++maskc;
      mask = rx_hash_masks[maskc];
      bucket = H2B(hash & mask);
      ++depth;
    }
  
  {
    struct rx_hash_item * it;
    it = (struct rx_hash_item *)(table->children[bucket]);
    while (it)
      if (eq (it->data, value))
	return it;
      else
	it = it->next_same_hash;
  }
  
  {
    if (   (depth < 3)
	&& (overflows ((struct rx_hash_item *)table->children [bucket])))
      {
	struct rx_hash * newtab;
	newtab = (struct rx_hash *) HASH_ALLOC(rules) (rules);
	if (!newtab)
	  goto add_to_bucket;
	rx_bzero ((char *)newtab, sizeof (*newtab));
	newtab->parent = table;
	{
	  struct rx_hash_item * them;
	  unsigned long newmask;
	  them = (struct rx_hash_item *)table->children[bucket];
	  newmask = rx_hash_masks[maskc + 1];
	  while (them)
	    {
	      struct rx_hash_item * save = them->next_same_hash;
	      int new_buck = H2B(them->hash & newmask);
	      them->next_same_hash = ((struct rx_hash_item *)
				      newtab->children[new_buck]);
	      ((struct rx_hash_item **)newtab->children)[new_buck] = them;
	      them->table = newtab;
	      them = save;
	      ++newtab->refs;
	      --table->refs;
	    }
	  ((struct rx_hash **)table->children)[bucket] = newtab;
	  RX_bitset_enjoin (&table->nested_p, bucket);
	  ++table->refs;
	  table = newtab;
	  bucket = H2B(hash & newmask);
	}
      }
  }
 add_to_bucket:
  {
    struct rx_hash_item  * it = ((struct rx_hash_item *)
				 ITEM_ALLOC(rules) (rules, value));
    if (!it)
      return 0;
    it->hash = hash;
    it->table = table;
    /* DATA and BINDING are to be set in hash_item_alloc */
    it->next_same_hash = (struct rx_hash_item *)table->children [bucket];
    ((struct rx_hash_item **)table->children)[bucket] = it;
    ++table->refs;
    return it;
  }
}


#ifdef __STDC__
void
rx_hash_free (struct rx_hash_item * it, struct rx_hash_rules * rules)
#else
void
rx_hash_free (it, rules)
     struct rx_hash_item * it;
     struct rx_hash_rules * rules;
#endif
{
  if (it)
    {
      struct rx_hash * table = it->table;
      unsigned long hash = it->hash;
      int depth = (table->parent
		   ? (table->parent->parent
		      ? (table->parent->parent->parent
			 ? 3
			 : 2)
		      : 1)
		   : 0);
      int bucket = H2B (hash & rx_hash_masks [depth]);
      struct rx_hash_item ** pos
	= (struct rx_hash_item **)&table->children [bucket];
      
      while (*pos != it)
	pos = &(*pos)->next_same_hash;
      *pos = it->next_same_hash;
      FREE_HASH_ITEM(rules) (it, rules);
      --table->refs;
      while (!table->refs && depth)
	{
	  struct rx_hash * save = table;
	  table = table->parent;
	  --depth;
	  bucket = H2B(hash & rx_hash_masks [depth]);
	  --table->refs;
	  table->children[bucket] = 0;
	  RX_bitset_remove (&table->nested_p, bucket);
	  FREE_HASH (rules) (save, rules);
	}
    }
}

#ifdef __STDC__
void
rx_free_hash_table (struct rx_hash * tab, rx_hash_freefn freefn,
		    struct rx_hash_rules * rules)
#else
void
rx_free_hash_table (tab, freefn, rules)
     struct rx_hash * tab;
     rx_hash_freefn freefn;
     struct rx_hash_rules * rules;
#endif
{
  int x;

  for (x = 0; x < BKTS; ++x)
    if (RX_bitset_member (&tab->nested_p, x))
      {
	rx_free_hash_table ((struct rx_hash *)tab->children[x],
			    freefn, rules);
	FREE_HASH (rules) ((struct rx_hash *)tab->children[x], rules);
      }
    else
      {
	struct rx_hash_item * them = (struct rx_hash_item *)tab->children[x];
	while (them)
	  {
	    struct rx_hash_item * that = them;
	    them = that->next_same_hash;
	    freefn (that);
	    FREE_HASH_ITEM (rules) (that, rules);
	  }
      }
}



#ifdef __STDC__
int 
rx_count_hash_nodes (struct rx_hash * st)
#else
int 
rx_count_hash_nodes (st)
     struct rx_hash * st;
#endif
{
  int x;
  int count = 0;
  for (x = 0; x < BKTS; ++x)
    count += ((RX_bitset_member (&st->nested_p, x))
	      ? rx_count_hash_nodes ((struct rx_hash *)st->children[x])
	      : listlen ((struct rx_hash_item *)(st->children[x])));
  
  return count;
}

