#include <stdbool.h>
#include "buffer.h"

/**
 * format_pretty_size - Display an abbreviated size, like 3.4K
 * @param buf    Buffer to write into
 * @param num    Number to abbreviate
 * @param show_bytes Show the number of bytes
 * @param show_fractions Show fractions
 * @param show_mb Show in megabytes
 * @param units_on_left Show units on the left
 * @retval num   Number of characters written
 * @retval 0     Error
 *
 * Formats a number to be more human-readable by appending a unit (K, M, G, etc.) if needed if needed.
 * If necessary, the buffer is expanded.
 */
int format_pretty_size(struct Buffer *buf, size_t num, bool show_bytes,
                       bool show_fractions, bool show_mb, bool units_on_left)
{
  const int one_kilobyte = 1024;
  int num_characters_written;

  if (show_bytes && (num < one_kilobyte))
  {
    num_characters_written = buf_add_printf(buf, "%zu", num);
  }
  else if (num == 0)
  {
    return buf_addstr(buf, units_on_left ? "K0" : "0K");
  }
  else if (show_fractions && (num < 10189)) /* 0.1K - 9.9K */
  {
    num_characters_written = buf_add_printf(buf, units_on_left ? "K%3.1f" : "%3.1fK",
                                            (num < 103) ? 0.1 : (num / (double) one_kilobyte));
  }
  else if (!show_mb || (num < 1023949)) /* 10K - 999K */
  {
    /* 51 is magic which causes 10189/10240 to be rounded up to 10 */
    num_characters_written = buf_add_printf(buf, units_on_left ? "K%zu" : "%zuK",
                                            (num + 51) / one_kilobyte);
  }
  else if (show_fractions && (num < 10433332)) /* 1.0M - 9.9M */
  {
    num_characters_written = buf_add_printf(buf, units_on_left ? "M%3.1f" : "%3.1fM",
                                            num / 1048576.0);
  }
  else /* 10M+ */
  {
    /* (10433332 + 52428) / 1048576 = 10 */
    num_characters_written = buf_add_printf(buf, units_on_left ? "M%zu" : "%zuM",
                                            (num + 52428) / 1048576);
  }

  return num_characters_written * (num_characters_written > 0);
}
