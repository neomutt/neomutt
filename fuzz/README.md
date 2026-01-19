## Fuzzing NeoMutt

NeoMutt has some support for Fuzzing.

- https://en.wikipedia.org/wiki/Fuzzing

### Fuzz Targets

#### address-fuzz

Tests two functions that could be susceptible to remote attacks:

- `mutt_rfc822_read_header();`
- `mutt_parse_part();`

#### cli-fuzz

Tests the command line parser:

- `cli_parse();`

#### date-fuzz

Tests the date parser:

- `mutt_date_parse_date();`

### Fuzzing Machinery

The fuzzing machinery uses a custom entry point to the code.
Each fuzz target implements the LibFuzzer interface:

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
```

### Build the Fuzzers

To build the fuzzers, we need to build with `clang` and pass some extra flags:

```sh
# Set some environment variables
export EXTRA_CFLAGS="-fsanitize=fuzzer"
export CXXFLAGS="$EXTRA_CFLAGS"
```

```sh
# Configure and build
./configure CC=clang --disable-doc --quiet --fuzzing
make CC=clang CXX=clang fuzz
```

### Run the Fuzzers

The fuzzers can be run simply:

```sh
fuzz/address-fuzz
fuzz/cli-fuzz
fuzz/date-fuzz
```

or they can be run against a corpus of test cases:

```sh
# Run the address fuzzer on sample data
git clone https://github.com/neomutt/corpus-address.git
fuzz/address-fuzz corpus-address

# Run the CLI fuzzer on sample data
git clone https://github.com/neomutt/corpus-cli.git
fuzz/cli-fuzz corpus-cli
```

To see some more options, run:

```sh
fuzz/address-fuzz -help=1
```

Adding the option `-max_total_time=3600` will limit the run time to one hour.

