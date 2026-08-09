#!/usr/bin/env python3
"""
Generate quickfix.txt and summary.md from clang-tidy-report.txt.

Filters:
- Only warnings (not notes, errors, etc.)
- Ignores files with absolute paths (outside project)
- Strips leading './' from filenames
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

INPUT_FILE = "clang-tidy-report.txt"
QUICKFIX_FILE = "quickfix.txt"
SUMMARY_FILE = "summary.md"

# Matches: file:line:col: warning: message [check-name]
WARNING_RE = re.compile(
    r'^(\.?/?[^/\s][^:]*):(\d+):(\d+):\s+warning:\s+(.*?)\s+\[([^\]]+)\]\s*$'
)


def parse_warnings(path):
    """Parse warning lines, returning list of (check, file, line, col, message).

    Deduplicates by (file, line, col, check); preserves first-occurrence order
    of check groups for quickfix output.
    """
    warnings = []
    seen = set()
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip()
            m = WARNING_RE.match(line)
            if not m:
                continue
            filepath, lineno, col, message, check = m.groups()
            # Skip absolute paths (files outside the project)
            if filepath.startswith("/"):
                continue
            # Strip leading './'
            if filepath.startswith("./"):
                filepath = filepath[2:]
            key = (filepath, int(lineno), int(col), check)
            if key in seen:
                continue
            seen.add(key)
            warnings.append((check, filepath, int(lineno), int(col), message))
    return warnings


def write_quickfix(warnings, path):
    """Write Vim quickfix file grouped by check, sorted by file/line/col.

    Groups are ordered by first occurrence in the source report.
    """
    # Group by check, preserving first-occurrence order of groups
    by_check = defaultdict(list)
    check_order = []
    for check, filepath, lineno, col, message in warnings:
        if check not in by_check:
            check_order.append(check)
        by_check[check].append((filepath, lineno, col, message))

    with open(path, "w", encoding="utf-8") as f:
        for check in check_order:
            f.write(f"[{check}]\n")
            entries = sorted(by_check[check], key=lambda e: (e[0], e[1], e[2]))
            # Find max width of "file:line:col:" for alignment
            labels = [f"{e[0]}:{e[1]}:{e[2]}:" for e in entries]
            max_len = max(len(l) for l in labels)
            for (filepath, lineno, col, message), label in zip(entries, labels):
                padding = " " * (max_len - len(label) + 1)
                f.write(f"{label}{padding}warning: {message}\n")
            f.write("\n")


def write_summary(warnings, path):
    """Write markdown summary table sorted by count descending."""
    counts = defaultdict(int)
    for check, *_ in warnings:
        counts[check] += 1

    with open(path, "w", encoding="utf-8") as f:
        f.write("| Count | Warning |\n")
        f.write("|------:|---------|\n")
        for check, count in sorted(counts.items(), key=lambda x: (-x[1], x[0])):
            f.write(f"| {count} | {check} |\n")


def main():
    input_path = Path(sys.argv[1] if len(sys.argv) > 1 else INPUT_FILE)
    if not input_path.exists():
        print(f"Error: {input_path} not found", file=sys.stderr)
        sys.exit(1)

    warnings = parse_warnings(input_path)
    print(f"Found {len(warnings)} warnings")

    write_quickfix(warnings, QUICKFIX_FILE)
    print(f"Written {QUICKFIX_FILE}")

    write_summary(warnings, SUMMARY_FILE)
    print(f"Written {SUMMARY_FILE}")


if __name__ == "__main__":
    main()
