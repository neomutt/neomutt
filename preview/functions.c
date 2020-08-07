#include "mutt/file.h"
#include "mutt/memory.h"
#include "handler.h"
#include "mutt_parse.h"
#include "mx.h"
#include "preview/private.h"
#include "recvattach.h"
#include "state.h"

#include "email/attach.h"
#include "email/lib.h"

static struct Body *find_first_decodable_body(struct AttachCtx *actx)
{
  for (int i = 0; i < actx->idxlen; i++)
  {
    struct Body *body = actx->idx[i]->body;
    if (mutt_can_decode(body))
    {
      return body;
    }
  }

  mutt_debug(LL_DEBUG1, "preview: did not find a decodable body");
  return NULL;
}

static size_t filter_and_add(char *dest, size_t dest_available, const char *source)
{
  if (!dest || dest_available == 0 || !source)
  {
    return 0;
  }

  char *initial_dest = dest;
  char *max_dest = dest + dest_available;

  size_t char_to_copy = dest_available;
  while (char_to_copy && dest < max_dest && *source != '\0')
  {
    char sc = *source;
    // XXX: how to handle locale here? Do we have a global conf one?
    // Also no idea how non roman alphabet should be handled…
    if (isspace(sc))
    {
      *dest++ = ' ';
      --char_to_copy;
      ++source;
      // trim all spaces
      while (isspace(*source))
        ++source;
      continue;
    }

    *dest++ = *source++;
    --char_to_copy;
  }

  return dest - initial_dest;
}

void compute_mail_preview(struct PreviewWindowData *data)
{
  struct Mailbox *m = data->mailbox;
  struct Email *e = data->current_email;
  mutt_parse_mime_message(m, e);
  struct Message *msg = mx_msg_open(m, e->msgno);

  data->preview_data[0] = '\0';

  if (!msg)
  {
    mutt_debug(LL_DEBUG1, "preview: could not open mail");
    return;
  }

  struct State s = { 0 };
  s.flags |= MUTT_DISPLAY | MUTT_VERIFY | MUTT_WEED | MUTT_CHARCONV;
  s.fp_in = msg->fp;
  s.fp_out = mutt_file_mkstemp();

  struct AttachCtx *actx = mutt_mem_calloc(1, sizeof(*actx));
  actx->email = e;
  actx->fp_root = msg->fp;

  mutt_generate_recvattach_list(actx, actx->email, actx->email->body,
                                actx->fp_root, -1, 0, 0);

  struct Body *body = find_first_decodable_body(actx);
  if (!body)
  {
    goto cleanup;
  }

  mutt_body_handler(body, &s);

  rewind(s.fp_out);

  size_t sz_dest = sizeof(data->preview_data);

  size_t added = 0;
  size_t sz_line = sz_dest;
  char *line = mutt_mem_malloc(sz_line);
  for (int i = 0; i < 3; ++i)
  {
    line = mutt_file_read_line(line, &sz_line, s.fp_out, NULL, 0);
    if (!line)
      break;

    size_t char_added = filter_and_add(data->preview_data + added, sz_dest - added, line);
    added += char_added;

    if (added == sz_line)
    {
      break;
    }
  }

cleanup:
  if (line)
  {
    FREE(&line);
  }
  mx_msg_close(m, &msg);
  fclose(s.fp_out);
  mutt_actx_free(&actx);
}
