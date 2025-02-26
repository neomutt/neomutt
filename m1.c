#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "pfile/lib.h"

struct PagedTextMarkup *paged_text_markup_new(struct PagedTextMarkupArray *ptma)
{
  if (!ptma)
    return NULL;

  struct PagedTextMarkup ptm = { 0 };
  ARRAY_ADD(ptma, ptm);

  return ARRAY_LAST(ptma);
}

int endwin(void)
{
  return 0;
}

void markup_dump(const struct PagedTextMarkupArray *ptma, int first, int last)
{
  if (!ptma)
    return;

  int count = 0;

  printf("M:");
  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    int mod = 0;
    printf("(");
    for (int i = 0; i < ptm->bytes; i++, count++)
    {
      if (ptm->first >= 100)
      {
        printf("\033[1;7;33m"); // Yellow
        mod = -100;
      }
      else if ((count < first) || (count > last))
      {
        printf("\033[1;32m"); // Green
      }
      else
      {
        printf("\033[1;7;31m"); // Red
      }

      printf("%d", ptm->first + i + mod);
      printf("\033[0m");

      if (i < (ptm->bytes - 1))
        printf(",");
    }
    printf(")");

    if (ARRAY_FOREACH_IDX_ptm < (ARRAY_SIZE(ptma) - 1))
      printf(",");
  }
  printf("\n");
}

bool markup_insert(struct PagedTextMarkupArray *ptma, const char *text, int position, int bytes)
{
  if (!ptma || !text)
    return false;

  // printf("insert: pos %d, '%s' %d bytes\n", position, text, bytes);
  // printf("\n");

  int total_pos = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    // printf("piece %d: %d - %d\n", ARRAY_FOREACH_IDX_ptm, total_pos, total_pos + ptm->bytes - 1);

    if ((position >= total_pos) && (position < (total_pos + ptm->bytes)))
    {
      int start = position - total_pos;
      // printf("    start = %d\n", start);

      if (start == 0)
      {
        struct PagedTextMarkup ptm_new = { 0 };
        ptm_new.first = 90;
        ptm_new.bytes = bytes;

        ARRAY_INSERT(ptma, ptm, &ptm_new);
      }
      else
      {
        struct PagedTextMarkup ptm_start = *ptm;
        struct PagedTextMarkup ptm_new = { 0 };
        ptm_new.first = 90;
        ptm_new.bytes = bytes;

        ptm->first += start;
        ptm->bytes -= start;

        // ptm_start.first
        ptm_start.bytes = start;

        ARRAY_INSERT(ptma, ptm, &ptm_new);
        ARRAY_INSERT(ptma, ptm, &ptm_start);
      }

      return true;
    }

    total_pos += ptm->bytes;
  }

  struct PagedTextMarkup ptm_new = { 0 };
  ptm_new.first = 90;
  ptm_new.bytes = bytes;

  ARRAY_ADD(ptma, ptm_new);

  return true;
}

bool markup_delete(struct PagedTextMarkupArray *ptma, int position, int bytes)
{
  if (!ptma)
    return false;
  if (bytes == 0)
    return true;

  // printf("delete: pos %d, %d bytes\n", position, bytes);
  // printf("\n");

  int total_pos = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    // printf("piece %d: %d - %d\n", ARRAY_FOREACH_IDX_ptm, total_pos, total_pos + ptm->bytes - 1);

    int start = -1;
    int last = -1;

    if ((position < total_pos) && ((position + bytes - 1) > total_pos))
    {
      start = 0;
    }
    else if ((position >= total_pos) && (position < (total_pos + ptm->bytes)))
    {
      start = position - total_pos;
    }

    if (((position + bytes - 1) >= total_pos) && ((position + bytes - 1) < (total_pos + ptm->bytes)))
    {
      last = position + bytes - 1 - total_pos;
    }
    else if ((start != -1) && (position + bytes) >= total_pos)
    {
      last = ptm->bytes - 1;
    }

    total_pos += ptm->bytes;

    if ((start == -1) && (last == -1))
    {
      // printf("\t\033[1;7;32mignore\033[0m\n");
      continue;
    }
    else if ((start == 0) && (last == (ptm->bytes - 1)))
    {
      // printf("\t\033[1;7;36mkill\033[0m\n");
      ptm->first = -1;
      ptm->bytes = -1;
    }
    else if ((start > 0) && (last < (ptm->bytes - 1)))
    {
      // printf("\t\033[1;7;35msplit\033[0m\n");

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first;
      ptm_new.bytes = start;

      ptm->first += (last + 1);
      ptm->bytes -= (last + 1);

      ARRAY_INSERT(ptma, ptm, &ptm_new);
    }
    else if (start <= 0)
    {
      // printf("\t\033[1;7;31mstart = %d\033[0m\n", start);
      // printf("\t\033[1;7;33mlast = %d\033[0m\n", last);

      int remainder = last + 1;
      ptm->first += remainder;
      ptm->bytes -= remainder;
    }
    else
    {
      // printf("\t\033[1;7;31mstart = %d\033[0m\n", start);
      // printf("\t\033[1;7;33mlast = %d\033[0m\n", last);

      int remainder = last - start + 1;
      ptm->bytes -= remainder;
    }
  }

  int i = 0;
  while (i < ARRAY_SIZE(ptma))
  {
    ptm = ARRAY_GET(ptma, i);
    if (!ptm)
      break;

    if (ptm->first == -1)
      ARRAY_REMOVE(ptma, ptm);
    else
      i++;
  }

  // printf("\n");
  return true;
}

bool markup_apply(struct PagedTextMarkupArray *ptma, int position, int bytes, int cid)
{
  if (!ptma)
    return false;

  // printf("markup: pos %d, %d bytes\n", position, bytes);
  // printf("\n");

  int total_pos = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    // printf("piece %d: %d - %d\n", ARRAY_FOREACH_IDX_ptm, total_pos, total_pos + ptm->bytes - 1);

    int start = -1;
    int last = -1;

    if ((position < total_pos) && ((position + bytes - 1) > total_pos))
    {
      start = 0;
    }
    else if ((position >= total_pos) && (position < (total_pos + ptm->bytes)))
    {
      start = position - total_pos;
    }

    if (((position + bytes - 1) >= total_pos) && ((position + bytes - 1) < (total_pos + ptm->bytes)))
    {
      last = position + bytes - 1 - total_pos;
    }
    else if ((start != -1) && (position + bytes) >= total_pos)
    {
      last = ptm->bytes - 1;
    }

    total_pos += ptm->bytes;

    if ((start == -1) && (last == -1))
    {
      // printf("\t\033[1;7;32mignore\033[0m\n");
      continue;
    }
    else if ((start == 0) && (last == (ptm->bytes - 1)))
    {
      // printf("\t\033[1;7;36mentire\033[0m\n");
      ptm->first += 100;
    }
    else if ((start > 0) && (last < (ptm->bytes - 1)))
    {
      // printf("\t\033[1;7;35msplit\033[0m\n");

      struct PagedTextMarkup ptm_start = { 0 };
      ptm_start.first = ptm->first;
      ptm_start.bytes = start;

      ptm->first += (last + 1);
      ptm->bytes -= (last + 1);

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first + start - last - 1 + 100;
      ptm_new.bytes = bytes;

      ARRAY_INSERT(ptma, ptm, &ptm_new);
      ARRAY_INSERT(ptma, ptm, &ptm_start);
      ARRAY_FOREACH_IDX_ptm += 2;
    }
    else if (start <= 0)
    {
      // printf("\t\033[1;7;31mstart = %d\033[0m\n", start);
      // printf("\t\033[1;7;33mlast = %d\033[0m\n", last);

      int remainder = last + 1;

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first + 100;
      ptm_new.bytes = remainder;

      ptm->first += remainder;
      ptm->bytes -= remainder;

      ARRAY_INSERT(ptma, ptm, &ptm_new);
      ARRAY_FOREACH_IDX_ptm++;
    }
    else
    {
      // printf("\t\033[1;7;31mstart = %d\033[0m\n", start);
      // printf("\t\033[1;7;33mlast = %d\033[0m\n", last);

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first;
      ptm_new.bytes = start;

      ptm->first += start + 100;
      ptm->bytes -= start;

      ARRAY_INSERT(ptma, ptm, &ptm_new);
      ARRAY_FOREACH_IDX_ptm++;
    }
  }

  // printf("\n");
  return true;

}

void test_delete(int argc, char *argv[])
{
  int start = 2;
  int bytes = 5;

  if (argc > 1)
    mutt_str_atoi(argv[1], &start);
  if (argc > 2)
    mutt_str_atoi(argv[2], &bytes);

  struct PagedTextMarkupArray ptma = ARRAY_HEAD_INITIALIZER;
  struct PagedTextMarkup *ptm = NULL;

  ptm = paged_text_markup_new(&ptma); ptm->first = 10; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 20; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 30; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 40; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 50; ptm->bytes = 5;

  // markup_dump(&ptma, start, start + bytes - 1);
  // printf("\n");

  markup_delete(&ptma, start, bytes);

  markup_dump(&ptma, -1, -1);
  // printf("\n");

  ARRAY_FREE(&ptma);
}

void test_insert(int argc, char *argv[])
{
  int position = 2;

  if (argc > 1)
    mutt_str_atoi(argv[1], &position);

  struct PagedTextMarkupArray ptma = ARRAY_HEAD_INITIALIZER;
  struct PagedTextMarkup *ptm = NULL;

  ptm = paged_text_markup_new(&ptma); ptm->first = 10; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 20; ptm->bytes = 5;

  // markup_dump(&ptma, -1, -1);
  // printf("\n");

  const char *text = "hello";
  const int bytes = strlen(text);
  markup_insert(&ptma, text, position, bytes);

  markup_dump(&ptma, -1, -1);
  // printf("\n");

  ARRAY_FREE(&ptma);
}

void test_markup(int argc, char *argv[])
{
  int start = 2;
  int bytes = 5;

  if (argc > 1)
    mutt_str_atoi(argv[1], &start);
  if (argc > 2)
    mutt_str_atoi(argv[2], &bytes);

  struct PagedTextMarkupArray ptma = ARRAY_HEAD_INITIALIZER;
  struct PagedTextMarkup *ptm = NULL;

  ptm = paged_text_markup_new(&ptma); ptm->first = 10; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 20; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 30; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 40; ptm->bytes = 5;
  ptm = paged_text_markup_new(&ptma); ptm->first = 50; ptm->bytes = 5;

  markup_dump(&ptma, start, start + bytes - 1);
  // printf("\n");

  markup_apply(&ptma, start, bytes, 42);

  markup_dump(&ptma, -1, -1);
  // printf("\n");

  ARRAY_FREE(&ptma);
}

int main(int argc, char *argv[])
{
  // test_delete(argc, argv);
  test_insert(argc, argv);
  // test_markup(argc, argv);
  return 0;
}
