# GitHub Copilot Instructions for NeoMutt

## Project Overview

NeoMutt is a command-line email client (Mail User Agent) written in C. It's a project that gathers all the patches against Mutt and provides a place for developers to collaborate.

### Architecture

NeoMutt is organized into modular libraries, each with its own directory:

- **Core libraries**: `mutt/`, `core/`, `email/`, `config/`, `address/`
- **Protocol handlers**: `imap/`, `pop/`, `nntp/`, `maildir/`, `mbox/`, `mh/`
- **UI components**: `gui/`, `pager/`, `browser/`, `menu/`, `sidebar/`
- **Features**: `notmuch/`, `autocrypt/`, `compress/`, `alias/`, `attach/`
- **Utilities**: `parse/`, `pattern/`, `expando/`, `store/`, `hcache/`

Each module typically has a `lib.h` file that serves as the public interface.

## Code Style and Conventions

### C Style Guidelines

- **Standard**: C11 with GNU extensions
- **Indentation**: 2 spaces (never tabs for C code, except Makefiles use tabs)
- **Line length**: 80 characters maximum
- **Brace style**: Allman style (braces on separate lines)
- **Pointer alignment**: Right-aligned (e.g., `char *ptr`)
- **Header order**: Follow the `.clang-format` priority order:
  1. `"config.h"` (always first)
  2. Standard library headers (`<...>`)
  3. `"private.h"` (if applicable)
  4. Library headers by category (mutt/, address/, config/, email/, core/, etc.)
  5. Local headers

### Naming Conventions

- **Functions**: Use snake_case with module prefix (e.g., `mutt_str_copy()`, `buf_pool_get()`)
- **Types**: CamelCase for structs and typedefs
- **Constants**: UPPER_CASE for macros and enum values
- **Files**: snake_case.c/h

### Documentation

- **File headers**: Every file must have a Doxygen `@file` comment describing its purpose
- **Function comments**: Use Doxygen format with `@param`, `@return`, etc.
- **Copyright**: Include appropriate copyright headers with author attributions
- **License**: GPL v2+

Example:
```c
/**
 * @file
 * Brief description of file purpose
 *
 * @authors
 * Copyright (C) YYYY Name <email@example.com>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 */
```

## Common Patterns and Practices

### Buffer Management

- Use Buffer Pool pattern for temporary buffers:
  ```c
  struct Buffer *buf = buf_pool_get();
  // ... use buffer ...
  buf_pool_release(&buf);
  ```
- Always release buffers, even on error paths (use goto cleanup pattern if needed)

### Error Handling

- Use goto labels for cleanup on error paths
- Return meaningful error codes
- Clean up resources in reverse order of allocation

### String Handling

- Use NeoMutt's string functions from `mutt/lib.h` (e.g., `mutt_str_copy()`, `mutt_str_equal()`)
- Always check buffer sizes to prevent overflows
- Prefer `mutt_str_*` functions over standard C string functions

### Hash Tables

- Use `mutt_hash_walk()` with `HashWalkState` to iterate hash tables
- Example from repository memories:
  ```c
  struct HashWalkState state = { 0 };
  while (mutt_hash_walk(hash_table, &state))
  {
    // Process entry
  }
  ```

### Memory Safety

- Always initialize pointers to NULL
- Check allocations for failure
- Use `FREE()` macro which sets pointer to NULL after freeing
- Be careful with pointer arithmetic

## Common Pitfalls to Avoid

### Parse/Execute Pattern

When refactoring command parsing:
- ALWAYS separate parsing from execution (see parse_unbind() pattern)
- Create struct to hold parsed data
- Return early on parse errors with clear messages
- Write tests for parser BEFORE implementation

### Buffer Pool
✅ ALWAYS release buffers on ALL code paths: 
```c
struct Buffer *buf = buf_pool_get();
if (error_condition) {
  buf_pool_release(&buf);  // Release before return! 
  return -1;
}
buf_pool_release(&buf);
```

## Build and Test

NeoMutt uses GitHub Actions for continuous integration. The main workflow is `.github/workflows/build-and-test.yml` which performs three build configurations: default, minimal (disabled), and full (everything).

### Standard Build Configurations

**Default Build:**
```bash
./configure --full-doc
make neomutt
```

**Minimal Build (features disabled):**
```bash
./configure --full-doc --disable-nls --disable-pgp --disable-smime --ssl --gsasl
make neomutt
```

**Full Build (all features enabled):**
```bash
./configure --full-doc --autocrypt --bdb --fmemopen --gdbm --gnutls --gpgme --gss \
  --kyotocabinet --lmdb --lua --lz4 --notmuch --pcre2 --qdbm --rocksdb --sasl --tdb \
  --testing --tokyocabinet --with-lock=fcntl --zlib --zstd
make neomutt
```

### Build Steps

1. **Configure**: Set up the build with desired features
   ```bash
   mkdir build-default
   cd build-default
   ../configure --full-doc [options]
   ```

2. **Compile**: Build the neomutt binary
   ```bash
   make neomutt
   ```

3. **Verify**: Check the build
   ```bash
   ./neomutt -v    # Show version and features
   ./neomutt -h all  # Show all help
   ```

4. **Validate Documentation**:
   ```bash
   make validate-docs
   ```

### Testing

**Prerequisites**: Unit tests require test files from a separate repository.

1. **Setup Test Files** (one-time setup):
   ```bash
   # Clone test files repository
   git clone https://github.com/neomutt/neomutt-test-files test-files
   cd test-files
   ./setup.sh
   cd ..
   ```

2. **Set Environment Variable**:
   ```bash
   # For local development:
   export NEOMUTT_TEST_DIR=$PWD/test-files
   
   # In GitHub Actions CI:
   export NEOMUTT_TEST_DIR=$GITHUB_WORKSPACE/test-files
   ```

3. **Build Tests**:
   ```bash
   make test/neomutt-test
   ```

4. **Run All Tests**:
   ```bash
   make test
   # Or directly:
   test/neomutt-test
   ```

5. **List Available Tests**:
   ```bash
   test/neomutt-test -l
   ```

6. **Run Individual Tests**:
   ```bash
   test/neomutt-test test_parse_bind
   test/neomutt-test test_mutt_str_copy
   ```

### Test Structure

Tests are located in `test/` directory, organized by module. Tests use the `acutest.h` framework.

Test file structure:
```c
#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"

void test_function_name(void)
{
  // Test cases using TEST_CHECK() macros
}
```

### Fuzz Testing

NeoMutt supports fuzz testing using LLVM's LibFuzzer. Fuzzing helps find security vulnerabilities and bugs by testing parsing functions with randomly generated inputs.

#### Current Fuzz Targets

Two fuzz executables exist in the `fuzz/` directory:

- **`fuzz/address-fuzz`**: Tests email header parsing via `mutt_rfc822_read_header()` and MIME parsing via `mutt_parse_part()`. These are security-critical functions susceptible to remote attacks.
- **`fuzz/date-fuzz`**: Tests date parsing via `mutt_date_parse_date()`.

#### Building the Fuzzers

Fuzzing requires the `clang` compiler with the `-fsanitize=fuzzer` flag:

```bash
# Configure with fuzzing enabled
./configure CC=clang --fuzzing --disable-doc --disable-nls --disable-idn2

# Build the fuzz executables
make CC=clang CXX=clang fuzz
```

**Note**: The `--fuzzing` flag automatically adds `-fsanitize=fuzzer` to CFLAGS and LDFLAGS. You may need `--disable-nls` and `--disable-idn2` if those libraries are not installed.

#### Running the Fuzzers

Basic usage:
```bash
# Run with default settings (1200 second timeout)
fuzz/address-fuzz

# Run for a limited time (recommended for CI)
fuzz/address-fuzz -max_total_time=60

# Show help and all available options
fuzz/address-fuzz -help=1
```

Using a corpus directory (recommended):
```bash
# Clone the address corpus
git clone https://github.com/neomutt/corpus-address.git

# Run fuzzer with corpus
fuzz/address-fuzz corpus-address
```

Useful options:
- `-max_total_time=N`: Limit runtime to N seconds
- `-timeout=N`: Timeout per test case in seconds (default: 1200)
- `-jobs=N`: Run N parallel fuzzing jobs
- `-workers=N`: Number of worker processes
- `-verbosity=N`: Output verbosity level

#### Fuzz Test Implementation

Fuzz targets implement the LibFuzzer entry point:
```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  // Parse the fuzzed data
  // Return 0 on success, -1 to reject input
  return 0;
}
```

See `fuzz/address.c` and `fuzz/date.c` for examples.

#### Future Fuzzing Targets

The following libraries are candidates for future fuzz testing:

- **`libcli`** (`cli/`): Command line parsing
  - `cli_parse()`: Parses command line arguments into `CommandLine` struct
  - Entry point: `cli/parse.c`
  - Security relevance: Processes untrusted command line input

- **`libparse`** (`parse/`): NeoMutt command parsing
  - `parse_rc_line()`: Parses runtime configuration commands
  - `parse_extract_token()`: Tokenizes config file lines
  - Entry point: `parse/rc.c`, `parse/extract.c`
  - Security relevance: Processes config files which may contain malicious content

### Common Configure Options

- **TLS/SSL**: `--gnutls` or `--ssl`
- **GPG**: `--gpgme`
- **Search**: `--notmuch`
- **Scripting**: `--lua`
- **Testing**: `--testing`
- **Fuzzing**: `--fuzzing` (requires clang compiler)
- **Authentication**: `--sasl`, `--gss`, `--gsasl`
- **Header Cache**: `--lmdb`, `--tokyocabinet`, `--kyotocabinet`, `--gdbm`, `--bdb`, `--qdbm`, `--rocksdb`, `--tdb`
- **Compression**: `--lz4`, `--zlib`, `--zstd`
- **Documentation**: `--full-doc` (build complete documentation set)

### Code Formatting

- Format C code with: `clang-format -i <file>`
- Settings are in `.clang-format`
- EditorConfig settings in `.editorconfig` for basic formatting

## Security Considerations

### Critical Security Practices

1. **Input Validation**: Always validate and sanitize user input
2. **Buffer Overflows**: Use safe string functions with size limits
3. **Command Injection**: Carefully escape or validate data used in external commands
4. **Path Traversal**: Validate file paths
5. **Email Parsing**: Be careful with email headers and MIME parsing
6. **Credentials**: Never log or expose credentials
7. **Memory Safety**: Prevent use-after-free, double-free, buffer overflows

### Security-Sensitive Areas

- Email parsing (`parse/`, `email/`)
- External command execution (`external.c`)
- Network connections (`conn/`, `imap/`, `pop/`, `nntp/`)
- Cryptography (`ncrypt/`, `autocrypt/`)
- Configuration parsing (`config/`)

## Contributing Guidelines

### AI Assistance Disclosure

**IMPORTANT**: If you use AI assistance while contributing, you **must** disclose this in the pull request. See `docs/CONTRIBUTING.md` for details.

### Commit Message Format

- First line: Short summary (≤50 characters)
- Blank line
- Detailed description (wrapped at ~80 characters)
- Use bullet points for multiple changes
- Reference relevant issues/PRs

Example:
```
hook: fix memory leak in pattern hooks

- Free pattern before returning on error
- Add goto cleanup label for error paths
- Fixes #12345
```

### Pull Request Checklist

- [ ] Code follows the style guide (use clang-format)
- [ ] Documentation updated if needed (docs/manual.xml.head)
- [ ] All builds and tests passing
- [ ] Doxygen documentation added for new functions
- [ ] No compiler warnings introduced
- [ ] Security implications considered

### Code Review Expectations

- Keep commits clear and focused on single changes
- Avoid combining multiple features/fixes in one commit
- Rewrite Git history if needed to maintain clarity
- Eliminate all compiler warnings

## Module-Specific Guidelines

### Hook System (`hook.c`, `hook.h`)

- Hook parsers are organized by parameter types (global, pattern-based, regex-based)
- Hook commands registered in `HookCommands` array
- Use `mutt_get_hook_type()` to identify hook types by parser function pointers

### Configuration (`config/`)

- Configuration variables have specific types
- Use appropriate validator functions
- Follow existing patterns for new config options

### Email Handling (`email/`, `parse/`)

- Always validate email headers
- Use safe parsing functions
- Be aware of malformed/malicious emails

## Resources

- **Website**: https://neomutt.org
- **Development Guide**: https://neomutt.org/dev.html
- **Code Style Guide**: https://neomutt.org/dev/code
- **Doxygen Documentation**: https://neomutt.org/dev/doxygen
- **Issue Tracker**: https://github.com/neomutt/neomutt/issues

## Notes for AI Code Assistants

1. **Understand before changing**: Review surrounding code and existing patterns before making changes
2. **Minimal changes**: Make the smallest possible changes to achieve the goal
3. **Test your changes**: Build and test code after modifications
4. **Follow existing patterns**: Match the style and approach of surrounding code
5. **Security first**: Always consider security implications
6. **Document properly**: Include appropriate Doxygen comments
7. **Clean up**: Use buffer pools correctly, handle errors properly
8. **Respect history**: This is a mature codebase with many contributors; respect existing conventions
