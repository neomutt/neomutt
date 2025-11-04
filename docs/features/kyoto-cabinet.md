# Kyoto Cabinet

_Kyoto Cabinet backend for the header cache_

## Support

**Since:** NeoMutt 2016-10-02

**Dependencies:** Kyoto Cabinet library

## Introduction

Kyoto Cabinet is a library of routines for managing a database. The database is a simple data file containing records, each is a pair of a key and a value. Every key and value is serial bytes with variable length. Both binary data and character string can be used as a key and a value. Each key must be unique within a database. There is neither concept of data tables nor data types. Records are organized in hash table or B+ tree.

NeoMutt can use Kyoto Cabinet as a backend for its header cache. This is an alternative to the default backend (bdb, gdbm, qdbm, or tdb).

## Variables

The header cache backend is selected by setting `$header_cache` to a Kyoto Cabinet path. The path must end with ".kch" for hash table or ".kct" for B+ tree.

| Name | Type | Default |
| ---- | ---- | ------- |
| `header_cache` | string | (empty) |

## neomuttrc

```
# Example NeoMutt config file for the kyoto-cabinet feature.

# Kyoto Cabinet is a library of routines for managing a database.

# NeoMutt can use Kyoto Cabinet as a backend for its header cache.

# To use Kyoto Cabinet for the header cache, set 'header_cache' to a path
# ending with '.kch' for hash table or '.kct' for B+ tree.

set header_cache = "~/.cache/neomutt/kc-header-cache.kch"

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
