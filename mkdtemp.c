/* taken from XFCE's Xarchiver, made to work without glib for mutt */

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>

/* mkdtemp fuction for systems which don't have one */
char *mkdtemp (char *tmpl)
{
    static const char LETTERS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static long       value = 0;
    long              v;
    int               len;
    int               i, j;

    len = strlen (tmpl);
    if (len < 6 || strcmp (&tmpl[len - 6], "XXXXXX") != 0)
    {
        errno = EINVAL;
        return NULL;
    }

    value += ((long) time (NULL)) ^ getpid ();

    for (i = 0; i < 7 ; ++i, value += 7777)
    {
        /* fill in the random bits */
        for (j = 0, v = value; j < 6; ++j)
            tmpl[(len - 6) + j] = LETTERS[v % 62]; v /= 62;

        /* try to create the directory */
        if (mkdir (tmpl, 0700) == 0)
            return tmpl;
        else if (errno != EEXIST)
            return NULL;
    }

    errno = EEXIST;
    return NULL;
}
