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

## Build and Test

### Building

```bash
./configure [options]
make
```

Common configure options:
- `--gnutls` / `--ssl` - TLS/SSL support
- `--gpgme` - GPG support
- `--notmuch` - Notmuch search engine
- `--lua` - Lua scripting
- `--testing` - Enable testing infrastructure

### Testing

```bash
make test
```

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
