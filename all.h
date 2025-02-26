struct Source
{
  FILE *fp;
  bool close_fp;
  char *cache;
  long cache_size;
};

struct Segment
{
  int offset_bytes;
  int offset_cols;
};
ARRAY_HEAD(SegmentArray, struct Segment);

struct TextMarkup
{
  struct Source *source;
  int first;
  int last;

  int cid;
  const struct AttrColor *ac_text;
  const struct AttrColor *ac_merged;

  const char *ansi_start;
  const char *ansi_end;
};
ARRAY_HEAD(TextMarkupArray, struct TextMarkup);

struct Row
{
  int row_num;

  long offset;

  int cid;
  const struct AttrColor *ac_row;
  const struct AttrColor *ac_merged;

  struct TextMarkupArray text;
  struct TextMarkupArray search;

  char *cached_text;
  int num_bytes;
  int num_cols;
  struct SegmentArray segments;
  struct Layer *paged_layer;
};
ARRAY_HEAD(RowArray, struct Row);

struct Layer
{
  const char *name;
  int num_rows;
  struct RowArray rows;
  struct File *paged_file;
  const struct AttrColor *ac_layer;

  struct Source *source;
};
ARRAY_HEAD(LayerArray, struct Layer);

struct File
{
  struct LayerArray layers;
};

