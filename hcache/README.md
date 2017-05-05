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

