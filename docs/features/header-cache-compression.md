# Header Cache Compression

_Options for compressing the header cache files_

## Support

**Since:** NeoMutt 2020-02-22

**Dependencies:** [header cache](/guide/optionalfeatures.html#caching)

## Introduction

The Header Cache Compression Feature can be used for speeding up the loading of large mailboxes. Also the space used on disk can be shrunk by about 50% - depending on the compression method being used.

The implementation sits on top of the header caching functions. So the header cache compression can be used together with all available database backends.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `header_cache_compress_method` | string | (empty) |
| `header_cache_compress_level` | number | `1` |

The `header_cache_compress_method` can be _(empty)_ - which means, that no header cache compression should be used. But when set to _lz4_, _zlib_ or _zstd_ - then the compression is turned on.

The `header_cache_compress_level` defines the compression level, which should be used together with the selected `header_cache_compress_method`. Here is an overview of the possible settings:

| Method Name | Min | Max |
| ----------- | --- | --- |
| lz4 | 1 | 12 |
| zlib | 1 | 9 |
| zstd | 1 | 22 |

## neomuttrc

```
# Example NeoMutt config file for the header cache compression feature.

# --------------------------------------------------------------------------
# VARIABLES – shown with their default values
# --------------------------------------------------------------------------
set header_cache_compress_level = 1
set header_cache_compress_method = ""

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Tino Reichardt
