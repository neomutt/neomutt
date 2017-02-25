#include <config.h>
#include <mutt.h>

#include <bdd-for-c.h>

describe("Testing linked lists backend")
{
  it("for which list API handles insertion")
  {
    struct List *l = mutt_new_list();
    mutt_add_list(l, "fubar1");
    check(strcmp(l->next->data, "fubar1") == 0);
    mutt_add_list(l, "fubar2");
    check(strcmp(l->next->next->data, "fubar2") == 0);
    mutt_add_list(l, "fubar3");
    check(strcmp(l->next->next->next->data, "fubar3") == 0);
  }

  it("for which list API handles find")
  {
    struct List *l = mutt_new_list();
    mutt_add_list(l, "fubar1");
    check(strcmp(l->next->data, "fubar1") == 0);
    mutt_add_list(l, "fubar2");
    check(strcmp(l->next->next->data, "fubar2") == 0);
    mutt_add_list(l, "fubar3");
    check(strcmp(l->next->next->next->data, "fubar3") == 0);

    check(mutt_find_list(l, "fubar1") != NULL);
    check(mutt_find_list(l, "fubar2") != NULL);
    check(mutt_find_list(l, "fubar3") != NULL);
    check(mutt_find_list(l, "fubar4") == NULL);
  }

  it("for which stack API handles push")
  {
    struct List *l = mutt_new_list();
    mutt_push_list(&l, "fubar1");
    check(strcmp(mutt_front_list(l), "fubar1") == 0);
    mutt_push_list(&l, "fubar2");
    check(strcmp(mutt_front_list(l), "fubar2") == 0);
    mutt_push_list(&l, "fubar3");
    check(strcmp(mutt_front_list(l), "fubar3") == 0);
  }

  it("for which stack API handles find")
  {
    struct List *l = mutt_new_list();
    mutt_push_list(&l, "fubar1");
    check(strcmp(mutt_front_list(l), "fubar1") == 0);
    mutt_push_list(&l, "fubar2");
    check(strcmp(mutt_front_list(l), "fubar2") == 0);
    mutt_push_list(&l, "fubar3");
    check(strcmp(mutt_front_list(l), "fubar3") == 0);

    check(mutt_find_list(l, "fubar1") != NULL);
    check(mutt_find_list(l, "fubar2") != NULL);
    check(mutt_find_list(l, "fubar3") != NULL);
    check(mutt_find_list(l, "fubar4") == NULL);
  }

  it("for which stack API handles pop")
  {
    struct List *l = mutt_new_list();
    mutt_push_list(&l, "fubar1");
    check(strcmp(mutt_front_list(l), "fubar1") == 0);
    mutt_push_list(&l, "fubar2");
    check(strcmp(mutt_front_list(l), "fubar2") == 0);
    mutt_push_list(&l, "fubar3");
    check(strcmp(mutt_front_list(l), "fubar3") == 0);

    check(mutt_pop_list(&l) == true);
    check(strcmp(mutt_front_list(l), "fubar2") == 0);
    check(mutt_pop_list(&l) == true);
    check(strcmp(mutt_front_list(l), "fubar1") == 0);
    check(mutt_pop_list(&l) == true);
    check(strcmp(mutt_front_list(l), "") == 0);
    check(mutt_pop_list(&l) == true);
    check(strcmp(mutt_front_list(l), "") == 0);
    check(mutt_pop_list(&l) == false);
    check(strcmp(mutt_front_list(l), "") == 0);
  }
}
