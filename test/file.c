#define TEST_NO_MAIN
#include "acutest.h"
#include "mutt/file.h"
#include <ftw.h>

static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb)
{
  if(remove(pathname) < 0)
  {
    perror("ERROR: remove");
    return -1;
  }
  return 0;
}

/* This will test both the non-symlink-resolution and symlink-resolving
 * code paths of file_tidy_path()
 *
 * For the symlink resolving portion, a temporary directory is created in
 * /tmp to construct test assets of directories and symlinks. This
 * directory is deleted when the tests are finished. All functions used to
 * construct and destruct the test assets are POSIX compliant.
 * mkdtemp : POSIX.1-2008
 * remove  : POSIX.1-2001
 * nftw    : POSIX.1-2001
 */
void test_file_tidy_path(void)
{
  size_t len = 0;
  //
  ///
  //// The No Symlink Resolution Testing Portion
  //
  { /* empty */
    len = mutt_file_tidy_path("", false);
    if (!TEST_CHECK(len == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", len);
    }
  }

  ///// Absolute Paths
  { // Basic
    char basic[] = "/a/b/c";
    len = mutt_file_tidy_path(basic, false);
    if (!TEST_CHECK(len == 6))
    {
      TEST_MSG("Expected: %zu", 6);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(basic, "/a/b/c") == 0))
    {
      TEST_MSG("Expected: %s", "/a/b/c");
      TEST_MSG("Actual  : %s", basic);
    }
  }

  { // Basic trailing slash
    char basic[] = "/a/b/c/";
    len = mutt_file_tidy_path(basic, false);
    if (!TEST_CHECK(len == 6))
    {
      TEST_MSG("Expected: %zu", 6);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(basic, "/a/b/c") == 0))
    {
      TEST_MSG("Expected: %s", "/a/b/c");
      TEST_MSG("Actual  : %s", basic);
    }
  }

  { // Basic Trailing Parent
    char bp[] = "/a/b/c/..";
    len = mutt_file_tidy_path(bp, false);
    if (!TEST_CHECK(len == 4))
    {
      TEST_MSG("Expected: %zu", 4);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/a/b") == 0))
    {
      TEST_MSG("Expected: %s", "/a/b");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Double trailing parent
    char bp[] = "/a/b/c/../..";
    len = mutt_file_tidy_path(bp, false);
    if (!TEST_CHECK(len == 2))
    {
      TEST_MSG("Expected: %zu", 2);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/a") == 0))
    {
      TEST_MSG("Expected: %s", "/a");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Double trailing parent trailing slash
    char bp[] = "/a/b/c/../../";
    len = mutt_file_tidy_path(bp, false);
    if (!TEST_CHECK(len == 2))
    {
      TEST_MSG("Expected: %zu", 2);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/a") == 0))
    {
      TEST_MSG("Expected: %s", "/a");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Too many parents
    char bp[] = "/a/../../..";
    len = mutt_file_tidy_path(bp, false);
    if (!TEST_CHECK(len == 1))
    {
      TEST_MSG("Expected: %zu", 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/") == 0))
    {
      TEST_MSG("Expected: %s", "/");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Too many parents
    char bp[] = "/..";
    len = mutt_file_tidy_path(bp, false);
    if (!TEST_CHECK(len == 1))
    {
      TEST_MSG("Expected: %zu", 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/") == 0))
    {
      TEST_MSG("Expected: %s", "/");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { /* nuts */
    char trial[] = "/apple/butterfly/../custard/../../dirty";
    const char expected[] = "/dirty";
    len = mutt_file_tidy_path(trial, false);
    if (!TEST_CHECK(len == strlen(expected)))
    {
      TEST_MSG("Expected: %zu", strlen(expected));
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(expected, trial) == 0))
    {
      TEST_MSG("Expected: %s", expected);
      TEST_MSG("Actual  : %s", trial);
    }
  }

  //
  ///
  //// Symlink Resolution
  //
  /* These tests consist of making a temporary working directory in /tmp
   * For constructing temporary known directory locations and symlinks.
   */
  char basedir[100];
  char subdir[100];
  char link[100];
  //
  // SETUP
  //
  strcpy(basedir, "/tmp/neomutt-test_file_tidy_path-XXXXXX");
  if (mkdtemp(basedir) == NULL)
  {
    TEST_CHECK(0); // Setup failed, force Test failure
    TEST_MSG("Couldn't make tmpdir");
    return;
  }
  strcpy(subdir, basedir);
  strcpy(link, basedir);
  if (mkdir(strcat(subdir, "/a"), 0777) != 0)
  {
    TEST_CHECK(0);
    TEST_MSG("mkdir '%s' failed", subdir);
    return;
  }
  if (symlink(subdir, strcat(link, "/b")) != 0)
  {
    TEST_CHECK(0);
    TEST_MSG("symlink '%s' -> '%s' failed", link, subdir);
    return;
  }

  // START THE TESTS~

  { /* empty */
    char test[] = "";
    len = mutt_file_tidy_path(test, true);
    if (!TEST_CHECK(len == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(test, "\0") == 0))
    {
      TEST_MSG("Expected: %s", "");
      TEST_MSG("Actual  : %s", test);
    }
  }

  ///// Absolute Paths
  { // non-existent
    char noexist[] = "/nonexistent/path/for/sure/1q2w3e";
    len = mutt_file_tidy_path(noexist, true);
    if (!TEST_CHECK(len == strlen("/nonexistent/path/for/sure/1q2w3e")))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(noexist, "/nonexistent/path/for/sure/1q2w3e") == 0))
    {
      TEST_MSG("Expected: %s", "");
      TEST_MSG("Actual  : %s", noexist);
    }
  }/* Absolute symlink test */
  {
    char buf[100];
    strcpy(buf, link); // ${basedir}/b -> ${basedir}/a
    len = mutt_file_tidy_path(buf, true);
    if (!TEST_CHECK(len == strlen(subdir)))
    {
      TEST_MSG("Expected: %zu", strlen(subdir));
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(buf, subdir) == 0))
    {
      TEST_MSG("Expected: %s", subdir);
      TEST_MSG("Actual  : %s", buf);
    }
  }

  /* Relative symlink test -- this kind of symlink is relative to the
   * directory it is placed in
   **/
  {
    char buf[100];
    strcpy(buf, link); // ${basedir}/c -> a
    buf[strlen(buf) - 1] = 'c';

    if (symlink("a", buf) != 0)
    {
      TEST_CHECK(0);
      TEST_MSG("symlink '%s' -> '%s' failed", buf, "a");
      return;
    }

    len = mutt_file_tidy_path(buf, true);
    if (!TEST_CHECK(len == strlen(subdir)))
    {
      TEST_MSG("Expected: %zu", strlen(subdir));
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(buf, subdir) == 0))
    {
      TEST_MSG("Expected: %s", subdir);
      TEST_MSG("Actual  : %s", buf);
    }
  }
  /* Double Symlink test */
  {
    char buf[100];
    strcpy(buf, link); // ${basedir}/d -> b -> ${basedir}/a
    buf[strlen(buf) - 1] = 'd';

    if (symlink("b", buf) != 0)
    {
      TEST_CHECK(0);
      TEST_MSG("symlink '%s' -> '%s' failed", buf, "b");
      return;
    }

    len = mutt_file_tidy_path(buf, true);
    if (!TEST_CHECK(len == strlen(subdir)))
    {
      TEST_MSG("Expected: %zu", strlen(subdir));
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(buf, subdir) == 0))
    {
      TEST_MSG("Expected: %s", subdir);
      TEST_MSG("Actual  : %s", buf);
    }
  }
  /* Parent paths present */
  {
    char buf[100];
    strcpy(buf, basedir); // ${basedir}/a/../b -> a
    strcat(buf, "/a/../b");

    len = mutt_file_tidy_path(buf, true);
    if (!TEST_CHECK(len == strlen(subdir)))
    {
      TEST_MSG("Expected: %zu", strlen(subdir));
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(buf, subdir) == 0))
    {
      TEST_MSG("Expected: %s", subdir);
      TEST_MSG("Actual  : %s", buf);
    }
  }

// TODO: convert below tests
#if 0

  { // Basic trailing slash
    char basic[100] = strcpy(basic, link);
    len = mutt_file_tidy_path(basic, true);
    if (!TEST_CHECK(len == 6))
    {
      TEST_MSG("Expected: %zu", 6);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(basic, "/a/b/c") == 0))
    {
      TEST_MSG("Expected: %s", "/a/b/c");
      TEST_MSG("Actual  : %s", basic);
    }
  }

  { // Basic Trailing Parent
    char bp[] = "/a/b/c/..";
    len = mutt_file_tidy_path(bp, true);
    if (!TEST_CHECK(len == 4))
    {
      TEST_MSG("Expected: %zu", 4);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/a/b") == 0))
    {
      TEST_MSG("Expected: %s", "/a/b");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Double trailing parent
    char bp[] = "/a/b/c/../..";
    len = mutt_file_tidy_path(bp, true);
    if (!TEST_CHECK(len == 2))
    {
      TEST_MSG("Expected: %zu", 2);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/a") == 0))
    {
      TEST_MSG("Expected: %s", "/a");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Double trailing parent trailing slash
    char bp[] = "/a/b/c/../../";
    len = mutt_file_tidy_path(bp, true);
    if (!TEST_CHECK(len == 2))
    {
      TEST_MSG("Expected: %zu", 2);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/a") == 0))
    {
      TEST_MSG("Expected: %s", "/a");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Too many parents
    char bp[] = "/a/../../..";
    len = mutt_file_tidy_path(bp, true);
    if (!TEST_CHECK(len == 1))
    {
      TEST_MSG("Expected: %zu", 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/") == 0))
    {
      TEST_MSG("Expected: %s", "/");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { // Too many parents
    char bp[] = "/..";
    len = mutt_file_tidy_path(bp, true);
    if (!TEST_CHECK(len == 1))
    {
      TEST_MSG("Expected: %zu", 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(bp, "/") == 0))
    {
      TEST_MSG("Expected: %s", "/");
      TEST_MSG("Actual  : %s", bp);
    }
  }

  { /* nuts */
    char trial[] = "/apple/butterfly/../custard/../../dirty";
    const char expected[] = "/dirty";
    len = mutt_file_tidy_path(trial, true);
    if (!TEST_CHECK(len == strlen(expected)))
    {
      TEST_MSG("Expected: %zu", strlen(expected));
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(expected, trial) == 0))
    {
      TEST_MSG("Expected: %s", expected);
      TEST_MSG("Actual  : %s", trial);
    }
  }
#endif

  // CLEANUP (POSIX Compliant)
  // https://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c#answer-42978529
  // Delete the directory and its contents by traversing the tree in
  // reverse order, without crossing mount boundaries and symbolic links
  if (nftw(basedir, rmFiles, 10, (FTW_DEPTH | FTW_MOUNT | FTW_PHYS)) < 0)
  {
    TEST_CHECK(0);
    perror("ERROR: ntfw");
    return;
  }
}


