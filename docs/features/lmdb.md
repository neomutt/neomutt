# LMDB

_LMDB backend for the header cache_

## Support

**Since:** NeoMutt 2016-07-23

**Dependencies:** LMDB library

## Introduction

Lightning Memory-Mapped Database (LMDB) is a software library that provides a high-performance embedded transactional database in the form of a key-value store.

NeoMutt can use LMDB as a backend for its header cache. This is an alternative to the default backend (bdb, gdbm, qdbm, or tdb).

## Variables

The header cache backend is selected by setting `$header_cache` to a LMDB path. The path must end with ".lmdb".

| Name | Type | Default |
| ---- | ---- | ------- |
| `header_cache` | string | (empty) |

## neomuttrc

```
# Example NeoMutt config file for the lmdb feature.

# LMDB is a software library that provides a high-performance embedded transactional database.

# NeoMutt can use LMDB as a backend for its header cache.

# To use LMDB for the header cache, set 'header_cache' to a path
# ending with '.lmdb'.

set header_cache = "~/.cache/neomutt/lmdb-header-cache.lmdb"

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
