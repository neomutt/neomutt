# Custom CodeQL Queries for NeoMutt

This directory contains custom CodeQL queries and libraries to reduce false positives in CodeQL security scanning.

## Buffer Pool Sanitizers

### Problem

CodeQL was reporting false positives related to NeoMutt's Buffer Pool:

1. A function gets a Buffer from the Buffer Pool using `buf_pool_get()`
2. It reads into that Buffer from untrusted input (e.g., user config)
3. It returns the Buffer to the Pool using `buf_pool_release()`
4. Later, another function gets that same Buffer from the Pool
5. CodeQL incorrectly believed the Buffer was still tainted

This is a false positive because `buf_pool_release()` calls `buf_reset()` which uses `memset()` to clear the Buffer's memory.

### Solution

We created custom CodeQL sanitizers that recognize when Buffers are cleared:

- **`CustomSanitizers.qll`**: A library file that defines `BufferPoolSanitizer` class
- **`TaintWithBufferSanitizers.ql`**: A query that uses these sanitizers for taint tracking

### Sanitized Functions

The following functions are recognized as sanitizers:

- `buf_reset(buf)`: Clears the Buffer with `memset(buf->data, 0, buf->dsize)`
- `buf_pool_release(&buf)`: Calls `buf_reset()` internally before returning Buffer to Pool
- `memset(ptr, 0, size)`: Explicitly clears memory to zero
- `buf_pool_get()`: Returns a clean Buffer from the Pool

## Files

- **`qlpack.yml`**: Defines the CodeQL query pack
- **`CustomSanitizers.qll`**: Library with sanitizer definitions
- **`TaintWithBufferSanitizers.ql`**: Taint tracking query using the sanitizers

## Usage

These queries are automatically included by the GitHub Actions CodeQL workflow via the configuration in `.github/codeql.yml`.

## References

- [CodeQL C/C++ documentation](https://codeql.github.com/docs/codeql-language-guides/codeql-for-cpp/)
- [CodeQL taint tracking](https://codeql.github.com/docs/codeql-language-guides/using-flow-labels-for-precise-data-flow-analysis/)
