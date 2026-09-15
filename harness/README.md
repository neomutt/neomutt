# Mailbox Backend Test Harnesses

Standalone test programs for NeoMutt's mailbox backends. Each harness is a non-interactive, ncurses-free program that exercises a specific mailbox backend for testing, benchmarking, and stress testing.

## Available Harnesses

- **mbox-harness** — Unix mbox format
- **mmdf-harness** — MMDF format
- **maildir-harness** — Maildir format
- **mh-harness** — MH format
- **compmbox-harness** — Compressed mailbox format
- **imap-harness** — IMAP protocol
- **pop-harness** — POP3 protocol
- **nntp-harness** — NNTP (Usenet) protocol
- **notmuch-harness** — Notmuch virtual mailbox

## Building

The harnesses are built automatically when you build NeoMutt:

```bash
./configure [options]
make
```

To build only the harnesses:

```bash
make harness
```

Individual harnesses:

```bash
make harness/mbox-harness
make harness/maildir-harness
# etc.
```

## Usage

```
harness-program [options] <mailbox-path>

Options:
  -l, --list          List emails
  -r, --read <N>      Read email number N
  -c, --check         Check for new mail
  -a, --all           Do all: list, check
  -n, --repeat <N>    Repeat N times (default: 1)
  -q, --quiet         Suppress output
  -v, --verbose       Extra debug output
  -h, --help          Show help

Network options:
  -u, --user <user>   Username
  -p, --pass <pass>   Password (or set NEOMUTT_PASS env var)
```

## Examples

### Basic Operations

List all emails in a mailbox:

```bash
./harness/mbox-harness --list ~/Mail/inbox
```

Read a specific email (by index):

```bash
./harness/maildir-harness --read 0 ~/Maildir
```

Check for new mail:

```bash
./harness/mh-harness --check ~/Mail/mh-inbox
```

Do everything (list + check):

```bash
./harness/mmdf-harness --all ~/Mail/mmdf-inbox
```

### Benchmarking

Repeat operations to measure performance:

```bash
# Run 1000 iterations, show timing
./harness/mbox-harness --all --repeat 1000 --quiet ~/Mail/inbox
# Output: Completed 1000 iterations in 2.456 seconds (407.1 ops/sec)
```

Compare backend performance:

```bash
# Mbox
./harness/mbox-harness --list --repeat 500 --quiet ~/Mail/test.mbox

# Maildir
./harness/maildir-harness --list --repeat 500 --quiet ~/Mail/test-maildir

# MH
./harness/mh-harness --list --repeat 500 --quiet ~/Mail/test-mh
```

### Testing Sample Data

#### Mbox

```bash
# Create test mbox
cat > /tmp/test.mbox << 'EOF'
From sender@example.com Thu Jan  1 00:00:00 2024
Subject: Test Email 1
From: sender@example.com
To: recipient@example.com
Date: Thu, 1 Jan 2024 00:00:00 +0000

Body of test email 1.

From another@example.com Fri Jan  2 00:00:00 2024
Subject: Test Email 2
From: another@example.com
To: recipient@example.com
Date: Fri, 2 Jan 2024 00:00:00 +0000

Body of test email 2.
EOF

# Test it
./harness/mbox-harness --all /tmp/test.mbox
```

#### MMDF

```bash
# Create test MMDF mailbox (uses ^A^A^A^A delimiters)
(
  printf '\001\001\001\001\n'
  printf 'From: sender@example.com\n'
  printf 'To: recipient@example.com\n'
  printf 'Subject: MMDF Test Email\n'
  printf 'Date: Thu, 1 Jan 2024 00:00:00 +0000\n'
  printf '\n'
  printf 'Body of MMDF email.\n'
  printf '\001\001\001\001\n'
) > /tmp/test.mmdf

# Test it
./harness/mmdf-harness --all /tmp/test.mmdf
```

#### Maildir

```bash
# Create test Maildir
mkdir -p /tmp/test-maildir/{cur,new,tmp}
cat > /tmp/test-maildir/cur/1704067200.001.host:2,S << 'EOF'
Subject: Maildir Test Email
From: sender@example.com
To: recipient@example.com
Date: Mon, 1 Jan 2024 00:00:00 +0000

Body of Maildir email.
EOF

# Test it
./harness/maildir-harness --all /tmp/test-maildir
```

#### MH

```bash
# Create test MH mailbox
mkdir -p /tmp/test-mh
cat > /tmp/test-mh/1 << 'EOF'
Subject: MH Test Email 1
From: sender@example.com
To: recipient@example.com
Date: Thu, 1 Jan 2024 00:00:00 +0000

Body of MH email 1.
EOF

cat > /tmp/test-mh/2 << 'EOF'
Subject: MH Test Email 2
From: another@example.com
To: recipient@example.com
Date: Fri, 2 Jan 2024 00:00:00 +0000

Body of MH email 2.
EOF

echo "unseen: 1 2" > /tmp/test-mh/.mh_sequences

# Test it
./harness/mh-harness --all /tmp/test-mh
```

#### IMAP

```bash
# With credentials on the command line
./harness/imap-harness --user alice --pass secret --all imap://mail.example.com/INBOX

# With password from environment variable
export NEOMUTT_PASS=secret
./harness/imap-harness --user alice --all imap://mail.example.com/INBOX

# Username in the URL (password via env or --pass)
./harness/imap-harness --pass secret --all imap://alice@mail.example.com/INBOX
```

#### POP3

```bash
./harness/pop-harness --user alice --pass secret --all pop://mail.example.com/INBOX
```

#### NNTP

```bash
# Authenticated Usenet server
./harness/nntp-harness --user alice --pass secret --all nntp://news.example.com/comp.mail.mutt

# Anonymous Usenet server (no credentials needed)
./harness/nntp-harness --all nntp://news.example.com/comp.mail.mutt
```

#### Notmuch

```bash
# Requires a Notmuch database (no network credentials needed)
./harness/notmuch-harness --all notmuch:///path/to/maildir?query=tag:inbox
```

### Stress Testing

Run repeated operations to stress test the backend:

```bash
# Heavy read stress test
for i in {0..99}; do
  ./harness/maildir-harness --read $i ~/Maildir &
done
wait

# Continuous check stress test
./harness/mbox-harness --check --repeat 10000 ~/Mail/inbox
```

### Integration with Scripts

Parse output for automated testing:

```bash
#!/bin/bash
# Check if mailbox opens successfully
if ./harness/mbox-harness --list --quiet /tmp/test.mbox 2>&1 | grep -q "Error"; then
  echo "FAIL: Mailbox open failed"
  exit 1
else
  echo "PASS: Mailbox opened successfully"
fi
```

Benchmark and compare:

```bash
#!/bin/bash
# Benchmark all backends
echo "Backend,Operations/sec"
for backend in mbox mmdf maildir mh; do
  result=$(./harness/${backend}-harness --all --repeat 1000 --quiet /tmp/test.${backend} 2>&1)
  ops=$(echo "$result" | grep -oP '[\d.]+(?= ops/sec)')
  echo "$backend,$ops"
done
```

## Output Examples

### List emails

```
$ ./harness/mbox-harness --list /tmp/test.mbox
Opened mailbox: /tmp/test.mbox (2 messages)
     0: Test Email 1
     1: Test Email 2
Closed mailbox
```

### Read email

```
$ ./harness/maildir-harness --read 0 /tmp/test-maildir
Opened mailbox: /tmp/test-maildir (1 messages)
Message 0:
  Subject: Maildir Test Email
  Date: Mon, 1 Jan 2024 00:00:00 +0000
  Size: 28
Closed mailbox
```

### Check for new mail

```
$ ./harness/mh-harness --check /tmp/test-mh
Opened mailbox: /tmp/test-mh (2 messages)
Check: ok (no change)
Closed mailbox
```

### Benchmark mode

```
$ ./harness/maildir-harness --all --repeat 1000 --quiet /tmp/test-maildir
Completed 1000 iterations in 0.135 seconds (7404.2 ops/sec)
```

## Use Cases

### Development

- **API coverage testing** — Exercise MxOps interface methods
- **Regression testing** — Verify backend behavior after changes
- **Performance profiling** — Use with `perf`, `valgrind`, etc.

```bash
# Profile with perf
perf record ./harness/mbox-harness --all --repeat 1000 ~/Mail/inbox
perf report

# Check for memory leaks
valgrind --leak-check=full ./harness/maildir-harness --all ~/Maildir
```

### CI/CD

- **Automated testing** — Non-interactive tests in CI pipelines
- **Benchmark tracking** — Monitor performance over time
- **Fuzzing preparation** — Generate test corpus

```yaml
# GitHub Actions example
- name: Test mailbox backends
  run: |
    ./harness/mbox-harness --all test-data/test.mbox
    ./harness/mmdf-harness --all test-data/test.mmdf
    ./harness/maildir-harness --all test-data/test-maildir
    ./harness/mh-harness --all test-data/test-mh
```

### Debugging

- **Isolate backend issues** — Test without full NeoMutt complexity
- **Reproduce bugs** — Minimal test case for bug reports
- **GDB debugging** — Step through backend code

```bash
# Debug with GDB
gdb --args ./harness/mbox-harness --read 0 /tmp/test.mbox
(gdb) break mx_msg_open
(gdb) run
```

## Architecture

Each harness program:

1. Initializes NeoMutt core libraries (config, modules, etc.)
2. Registers only the required backend module
3. Forces the mailbox type (no auto-detection)
4. Uses the Mailbox API (`mx_*` functions) to:
   - Open/close mailboxes
   - List emails
   - Read email bodies
   - Check for new mail

The harnesses share common infrastructure in `common.c` and `common.h`:

- Command-line parsing (`getopt_long`)
- NeoMutt initialization/cleanup
- Mailbox operation wrappers
- Timing and benchmarking

## Differences from Full NeoMutt

The harnesses are **minimal standalone programs** that:

- ✅ Link against all NeoMutt libraries (except `main.o`)
- ✅ Initialize the config system and modules
- ✅ Exercise real backend code paths
- ❌ Do not read `.neomuttrc` or config files
- ❌ Do not use ncurses or any UI
- ❌ Do not handle user input (except CLI args)
- ❌ Do not support all NeoMutt features

## Troubleshooting

### "Error: failed to open mailbox"

- Check file/directory permissions
- Verify mailbox format matches harness (use correct harness for format)
- For network backends (IMAP, POP, NNTP), check connectivity and credentials
- For Notmuch, verify the database path and query syntax

### "Mailbox is corrupt"

- MMDF requires `\001\001\001\001` delimiters (see examples)
- Mbox requires "From " line separators
- Maildir requires `cur/`, `new/`, `tmp/` subdirectories
- MH requires numbered message files

### Segmentation fault

- Check that StartupComplete is properly initialized
- Verify config variables are registered (D_ON_STARTUP vars)
- Run with `valgrind` to identify memory issues

### Performance issues

- Use `--quiet` for benchmarks to avoid I/O overhead
- Increase `--repeat` count for more accurate timing
- Profile with `perf` to identify bottlenecks

## Related Tools

- **test/neomutt-test** — Unit test suite
- **fuzz/*** — Fuzzing harnesses for security testing
- **neomutt** — Full interactive email client

## Contributing

When adding new harness functionality:

1. Update `common.h` / `common.c` for shared features
2. Follow existing patterns (see `mbox.c` as template)
3. Add documentation to this README
4. Test with real mailbox data
5. Verify benchmarks work with `--repeat` and `--quiet`

## License

Copyright (C) 2026 Richard Russon and contributors

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later
version.
