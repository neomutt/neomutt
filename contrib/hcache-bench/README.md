# NeoMutt's hcache benchmark

## Introduction

The shell script and the configuration file in this directory can be used to benchmark the NeoMutt hcache backends. 

## Preparation

In order to run the benchmark, you must have a directory in maildir format at hand. Mutt will load messages from there and populate the header cache with them. Please note that you'll need a reasonable large number of messages - >50k - to see anything interesting.

## Running the benchmark

The script accepts the following arguments

```
-e Path to the mutt executable
-m Path to the maildir directory
-t Number of times to repeat the test
-b List of backends to test
```

Example: `./neomutt-hcache-bench.sh -e /usr/local/bin/mutt -m ../maildir -t 10 -b "lmdb qdbm bdb kyotocabinet"`

## Operation

The benchmark works by instructing mutt to use the backends specified with `-b` one by one and to load the messages from the maildir specified with `-m`. Mutt is launched twice with the same configuration. The first time, no header cache storage exists, so mutt populates it. The second time, the previously populated header cache storage is used to reload the headers. The times taken to execute these two operations are kept track of independently.

At the end, a summary with the average times is provided.

## Sample output

```sh
$ sh neomutt-hcache-bench.sh -m ~/maildir -e ../../mutt -t 10 -b "bdb gdbm qdbm lmdb kyotocabinet tokyocabinet"
Running in /tmp/tmp.TFHQ8iPy
 1 - populating - bdb
 1 - reloading  - bdb
 1 - populating - gdbm
 1 - reloading  - gdbm
 1 - populating - qdbm
 1 - reloading  - qdbm
 1 - populating - lmdb
 1 - reloading  - lmdb
 1 - populating - kyotocabinet
 1 - reloading  - kyotocabinet
 1 - populating - tokyocabinet
 1 - reloading  - tokyocabinet
 2 - populating - bdb
 2 - reloading  - bdb
 2 - populating - gdbm
 ....
10 - populating - kyotocabinet
10 - reloading  - kyotocabinet
10 - populating - tokyocabinet
10 - reloading  - tokyocabinet

*** populate
bdb            8.814 real 4.227 user 2.667 sys
gdbm           24.938 real 4.082 user 3.852 sys
qdbm           9.806 real 5.201 user 2.480 sys
lmdb           7.637 real 3.589 user 2.385 sys
kyotocabinet   11.493 real 6.456 user 2.537 sys
tokyocabinet   9.521 real 5.104 user 2.356 sys

*** reload
bdb            2.101 real 1.027 user .614 sys
gdbm           3.474 real .885 user .584 sys
qdbm           2.170 real 1.142 user .552 sys
lmdb           1.694 real .845 user .478 sys
kyotocabinet   2.729 real 1.541 user .591 sys
tokyocabinet   2.526 real 1.395 user .581 sys
```

## Notes

The benchmark uses a temporary directory for the log files and the header cache storage files. These are left available for inspection. This also means that *you* must take care of removing the temporary directory once you are done.

The path to the temporary directory is printed on standard output when the benchmark starts, e.g., `Running in /tmp/tmp.WjSFtdPf`.
