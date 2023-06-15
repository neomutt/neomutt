/**
 * @file
 * Shared test code for compression
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "compress/lib.h"

// ~4 KiB of Coleridge
static const char *compress_test_data =
    "It is an ancient Mariner, And he stoppeth one of three.  'By thy long "
    "grey beard and glittering eye, Now wherefore stopp'st thou me?  The "
    "Bridegroom's doors are opened wide, And I am next of kin; The guests are "
    "met, the feast is set: May'st hear the merry din.' He holds him with his "
    "skinny hand, 'There was a ship,' quoth he.  'Hold off! unhand me, "
    "grey-beard loon!' Eftsoons his hand dropt he.  He holds him with his "
    "glittering eye— The Wedding-Guest stood still, And listens like a three "
    "years' child: The Mariner hath his will.  The Wedding-Guest sat on a "
    "stone: He cannot choose but hear; And thus spake on that ancient man, The "
    "bright-eyed Mariner.  'The ship was cheered, the harbour cleared, Merrily "
    "did we drop Below the kirk, below the hill, Below the lighthouse top.  "
    "The Sun came up upon the left, Out of the sea came he!  And he shone "
    "bright, and on the right Went down into the sea.  Higher and higher every "
    "day, Till over the mast at noon—' The Wedding-Guest here beat his breast, "
    "For he heard the loud bassoon.  The bride hath paced into the hall, Red "
    "as a rose is she; Nodding their heads before her goes The merry "
    "minstrelsy.  The Wedding-Guest he beat his breast, Yet he cannot choose "
    "but hear; And thus spake on that ancient man, The bright-eyed Mariner.  "
    "And now the STORM-BLAST came, and he Was tyrannous and strong: He struck "
    "with his o'ertaking wings, And chased us south along.  With sloping masts "
    "and dipping prow, As who pursued with yell and blow Still treads the "
    "shadow of his foe, And forward bends his head, The ship drove fast, loud "
    "roared the blast, And southward aye we fled.  And now there came both "
    "mist and snow, And it grew wondrous cold: And ice, mast-high, came "
    "floating by, As green as emerald.  And through the drifts the snowy "
    "clifts Did send a dismal sheen: Nor shapes of men nor beasts we ken— The "
    "ice was all between.  The ice was here, the ice was there, The ice was "
    "all around: It cracked and growled, and roared and howled, Like noises in "
    "a swound!  At length did cross an Albatross, Thorough the fog it came; As "
    "if it had been a Christian soul, We hailed it in God's name.  It ate the "
    "food it ne'er had eat, And round and round it flew.  The ice did split "
    "with a thunder-fit; The helmsman steered us through!  And a good south "
    "wind sprung up behind; The Albatross did follow, And every day, for food "
    "or play, Came to the mariner's hollo!  In mist or cloud, on mast or "
    "shroud, It perched for vespers nine; Whiles all the night, through "
    "fog-smoke white, Glimmered the white Moon-shine.' 'God save thee, ancient "
    "Mariner!  From the fiends, that plague thee thus!— Why look'st thou "
    "so?'—With my cross-bow I shot the ALBATROSS.  The Sun now rose upon the "
    "right: Out of the sea came he, Still hid in mist, and on the left Went "
    "down into the sea.  And the good south wind still blew behind, But no "
    "sweet bird did follow, Nor any day for food or play Came to the mariner's "
    "hollo!  And I had done a hellish thing, And it would work 'em woe: For "
    "all averred, I had killed the bird That made the breeze to blow.  Ah "
    "wretch! said they, the bird to slay, That made the breeze to blow!  Nor "
    "dim nor red, like God's own head, The glorious Sun uprist: Then all "
    "averred, I had killed the bird That brought the fog and mist.  'Twas "
    "right, said they, such birds to slay, That bring the fog and mist.  The "
    "fair breeze blew, the white foam flew, The furrow followed free; We were "
    "the first that ever burst Into that silent sea.  Down dropt the breeze, "
    "the sails dropt down, 'Twas sad as sad could be; And we did speak only to "
    "break The silence of the sea!  All in a hot and copper sky, The bloody "
    "Sun, at noon, Right up above the mast did stand, No bigger than the Moon. "
    " Day after day, day after day, We stuck, nor breath nor motion; As idle "
    "as a painted ship Upon a painted ocean.  Water, water, every where, And "
    "all the boards did shrink; Water, water, every where, Nor any drop to "
    "drink.  The very deep did rot: O Christ!  That ever this should be!  Yea, "
    "slimy things did crawl with legs Upon the slimy sea.";

void test_compress_common(void)
{
  const char *list = compress_list();
  TEST_CHECK(list != NULL);
  FREE(&list);

  const struct ComprOps *compr_ops = compress_get_ops(NULL);
  TEST_CHECK(compr_ops != NULL);

  compr_ops = compress_get_ops("");
  TEST_CHECK(compr_ops != NULL);

  compr_ops = compress_get_ops("foobar");
  TEST_CHECK(compr_ops == NULL);
}

static void one_test(const struct ComprOps *compr_ops, short level, size_t size)
{
  if (!TEST_CHECK(size < strlen(compress_test_data)))
    return;

  ComprHandle *compr_handle = compr_ops->open(level);

  size_t clen = 0;
  void *cdata = compr_ops->compress(compr_handle, compress_test_data, size, &clen);
  if (!TEST_CHECK(cdata != NULL))
    return;
  if (!TEST_CHECK(clen != 0))
    return;

  void *copy = mutt_mem_malloc(clen);
  memcpy(copy, cdata, clen);

  void *ddata = compr_ops->decompress(compr_handle, copy, clen);
  FREE(&copy);

  if (!TEST_CHECK(ddata != NULL))
    return;

  if (!TEST_CHECK(memcmp(compress_test_data, ddata, size) == 0))
    return;

  compr_ops->close(&compr_handle);
}

void compress_data_tests(const struct ComprOps *compr_ops, short min_level, short max_level)
{
  static const size_t sizes[] = { 63,   64,   65,   127,  128,  129,
                                  255,  256,  257,  511,  512,  513,
                                  1023, 1024, 1024, 2047, 2048, 2049 };
  char case_name[64] = { 0 };
  for (short level = min_level; level <= max_level; level++)
  {
    snprintf(case_name, sizeof(case_name), "level %d", level);
    TEST_CASE(case_name);
    for (size_t size = 1; size < 33; size++)
    {
      snprintf(case_name, sizeof(case_name), "    size %zu", size);
      TEST_CASE(case_name);
      one_test(compr_ops, level, size);
    }

    for (size_t i = 0; i < mutt_array_size(sizes); i++)
    {
      snprintf(case_name, sizeof(case_name), "    size %zu", sizes[i]);
      TEST_CASE(case_name);
      one_test(compr_ops, level, sizes[i]);
    }
  }
}
