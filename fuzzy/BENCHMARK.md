# libfuzzy Benchmark

Standalone benchmark program for measuring libfuzzy performance.

## Building

```bash
cd fuzzy
make -f Makefile.benchmark
```

## What It Measures

`fuzzy-benchmark` measures the raw matcher (`fuzzy_match()` with
`FUZZY_ALGO_SUBSEQ`) in three groups:
- Basic single candidate matches
- Searches across a mailbox list (24 candidates)
- Option cost comparison (`case_sensitive`, `smart_case`, `prefer_prefix`)

## Usage

```bash
# Run with default iterations (100,000)
./fuzzy-benchmark

# Run with custom iteration count
./fuzzy-benchmark 50000
./fuzzy-benchmark 1000000
```

Notes:
- The benchmark enforces a minimum of 100 iterations.
- Mailbox-list scenarios run at `iterations / 100` passes through the list.

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
- **Total Time**: wall-clock duration for the scenario
- **Per Operation**: average microseconds per match attempt
- **Throughput**: estimated operations per second
- **Match Count**: number of successful matches in that scenario

## Getting Reliable Numbers

For stable numbers:
- Build with the same compiler/options you care about.
- Use at least `100000` iterations for comparative runs.
- Run each test multiple times and compare medians.
- Avoid heavy background load while benchmarking.

Raw absolute values vary across hardware and toolchains; use this tool
primarily for relative comparisons between algorithm changes.

## Interpreting Results

### Fast Operations
- Short patterns vs short candidates
- No match scenarios (fail fast)
- Prefix matches

### Slower Operations
- Medium-length patterns
- Complex mailbox path matching
- Scattered character matches

### Comparison to Interactive Use

For interactive mailbox selection with 1000 candidates:
- At 0.050 µs/op: ~50 milliseconds (imperceptible)
- At 0.100 µs/op: ~100 milliseconds (smooth)

This is generally within human perception thresholds for interactive search.

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
