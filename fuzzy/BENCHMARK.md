# libfuzzy Benchmark

Standalone benchmark program for measuring libfuzzy performance.

## Building

```bash
cd fuzzy
make -f Makefile.benchmark
```

## Usage

```bash
# Run with default iterations (100,000)
./fuzzy-benchmark

# Run with custom iteration count
./fuzzy-benchmark 50000
./fuzzy-benchmark 1000000
```

## Quick Benchmark

```bash
make -f Makefile.benchmark benchmark
```

This runs the benchmark at three different iteration counts:
- 10,000 iterations (quick test)
- 100,000 iterations (standard)
- 1,000,000 iterations (intensive)

## Test Scenarios

The benchmark tests the following scenarios:

### Basic Pattern Matching
1. **Short pattern + short candidate** - `"box"` vs `"mailbox"`
2. **Short pattern + medium candidate** - `"mlnd"` vs `"mailinglists/neomutt-dev"`
3. **Short pattern + long candidate** - `"arch"` vs `"Archive/2024/January/Projects/NeoMutt"`
4. **Medium pattern + long candidate** - `"archjan"` vs long path
5. **No match** - Testing non-matching patterns
6. **Prefix match** - Testing patterns at start of string
7. **Scattered match** - Testing patterns with gaps
8. **Full match** - Testing exact matches

### Realistic Scenarios
- **Mailbox list search** - Searches through 24 typical mailbox paths
- Tests multiple search patterns: `"inbox"`, `"mlnd"`, `"arch"`, `"work"`

### Options Comparison
- Case-insensitive matching
- Case-sensitive matching
- Smart case (auto case-sensitive if pattern has uppercase)
- Prefix preference

## Performance Metrics

The benchmark reports:
- **Total Time** - Time to complete all iterations (milliseconds)
- **Per Operation** - Time per single match operation (microseconds)
- **Throughput** - Operations per second
- **Match Count** - Number of successful matches

## Typical Results

On a modern x86_64 CPU (circa 2024), you should see:

- **Simple matches**: ~30-40 million ops/sec (0.025-0.033 µs/op)
- **Complex matches**: ~10-15 million ops/sec (0.066-0.100 µs/op)
- **Mailbox search**: ~15-18 million ops/sec through 24-item list

## Interpreting Results

### Fast Operations (< 0.050 µs/op)
- Short patterns vs short candidates
- No match scenarios (fail fast)
- Prefix matches

### Medium Operations (0.050-0.150 µs/op)
- Medium-length patterns
- Complex mailbox path matching
- Scattered character matches

### Comparison to Interactive Use

For interactive mailbox selection with 1000 candidates:
- At 0.050 µs/op: ~50 milliseconds (imperceptible)
- At 0.100 µs/op: ~100 milliseconds (smooth)

This is well within the human perception threshold (~100ms) for
instant responsiveness.

## Memory Usage

The fuzzy matcher uses:
- **No heap allocation** - Stack only
- **O(1) space** - Fixed 256-byte array for match positions
- **No recursion** - Pure iterative algorithm

Perfect for interactive use cases with thousands of candidates.

## Cleaning Up

```bash
make -f Makefile.benchmark clean
```
