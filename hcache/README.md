# Header Cache

These files make up NeoMutt's header cache.
There are six different databases to choose from:
- BerkeleyDB
- GDBM
- KyotoCabinet
- LMDB
- QDBM
- TokyoCabinet

Each backend implements the interface which is defined in `hcache.h`

# Operation

The header cache works by caching a number of fields from a number of structs.
These are hardcoded in the `serial_*` functions in implemented in
`hcache/serialize.c`. The header cache has a CRC checksum that gets invalidated
whenever the definition of any of the structs defined in the `STRUCTURES`
variable in `hcache/hcachever.sh` changes. So, changes to any structure
definition, or addition / removal of a structure to / from the `STRUCTURES`
variable trigger an invalidation of any stored header cache. However, the CRC
is **not** invalidated whenever a new field from a structure is added or
removed to the set of the serialized fields. If you add or remove any fields in
any of the functions in `hcache/serialize.c`, please bump the `BASEVERSION`
variable in `hcache/hcachever.sh`.
