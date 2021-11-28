## Fuzzing NeoMutt

NeoMutt has some support for Fuzzing.

- https://en.wikipedia.org/wiki/Fuzzing

It's currently limited to two functions.
Two that could be susceptible to remote attacks.

- `mutt_rfc822_read_header();`
- `mutt_parse_part();`

The fuzzing machinery uses a custom entry point to the code.
This can be found in `fuzz/address.c`

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
```

### Build the Fuzzer

To build the fuzzer, we need to build with `clang` and pass some extra flags:

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

### Run the Fuzzer

The fuzzer can be run by simply:

```sh
fuzz/address-fuzz
```

or it can be run against our corpus of test cases:

```sh
# Run the fuzzer on the sample data
git clone https://github.com/neomutt/corpus-address.git
fuzz/address-fuzz corpus-address
```

To see some more options, run:

```sh
fuzz/address-fuzz -help=1
```

Adding the option `-max_total_time=3600` will limit the run time to one hour.

