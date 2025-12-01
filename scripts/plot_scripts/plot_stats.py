#!/usr/bin/env python3
"""
plot_stats_sparse.py

Parse lines containing tokens like: name: [value]
and plot up to two metric groups. Metrics are stored sparsely:
only present values are appended (no NaNs).

You can now specify a portion of the input file to read using
--start-percent and --end-percent (values 0..100).

Usage examples:
  ./plot_stats_sparse.py -f stats.txt -g1 dram_free dram_used dram_size
  ./plot_stats_sparse.py -f stats.txt -g1 dram_free dram_used -g2 wrapped_records wrapped_headers -o out.png
  cat stats.txt | ./plot_stats_sparse.py -g1 dram_free
  ./plot_stats_sparse.py -f stats.txt -g1 dram_free --start-percent 10 --end-percent 50

Options:
  -f/--file    input file (default stdin)
  -g1 ...      metrics for left axis (required)
  -g2 ...      metrics for right axis (optional)
  -o/--out     output filename (png/pdf). If omitted, show interactively
  --x-regex    regex with one capturing group to extract x value from each line (not implemented)
  --start-percent float  start percentage of file to read (0-100), default 0
  --end-percent   float  end percentage of file to read (0-100], default 100
"""
from collections import defaultdict
import re
import sys
import argparse
import numpy as np
import matplotlib.pyplot as plt

RE_METRIC = re.compile(r'([A-Za-z0-9_]+)\s*:\s*\[\s*([+-]]?[0-9]*\.?[0-9]+(?:[eE][+-]?\d+)?)\s*\]')

# small regex fix: allow optional +/-
RE_METRIC = re.compile(r'([A-Za-z0-9_]+)\s*:\s*\[\s*([+-]?[0-9]*\.?[0-9]+(?:[eE][+-]?\d+)?)\s*\]')

def parse_sparse(lines):
    period = 1
    for line in lines[1:]:
        if line[0] != '\t':
            break
        period += 1
    """
    Parse lines and return:
      - metrics: dict -> metric_name : (list_of_x, list_of_y)
    If x_regex not provided, x is the line index (0-based), scaled by 1/5 as before.
    """
    metrics = defaultdict(lambda: ([], []))  # name -> (xs, ys)

    for idx, line in enumerate(lines):
        x_val = float(idx) / period

        for m in RE_METRIC.finditer(line):
            name = m.group(1)
            try:
                val = float(m.group(2))
            except Exception:
                continue
            xs, ys = metrics[name]
            xs.append(x_val)
            ys.append(val)

    return metrics

def plot_groups_sparse(metrics, g1, g2, out_fname=None, title=None):
    fig, ax1 = plt.subplots(figsize=(10,6))
    plotted_any = False

    # group1 (left axis)
    if g1:
        for m in g1:
            if m not in metrics:
                print(f"Warning: metric '{m}' not found in input; skipping.", file=sys.stderr)
                continue
            xs, ys = metrics[m]
            if len(xs) == 0:
                print(f"Warning: metric '{m}' has no samples; skipping.", file=sys.stderr)
                continue
            ax1.plot(xs, ys, label=m)
            plotted_any = True
        ax1.set_ylabel(" / ".join(g1))

    # group2 (right axis)
    if g2:
        ax2 = ax1.twinx()
        for m in g2:
            if m not in metrics:
                print(f"Warning: metric '{m}' not found in input; skipping.", file=sys.stderr)
                continue
            xs, ys = metrics[m]
            if len(xs) == 0:
                print(f"Warning: metric '{m}' has no samples; skipping.", file=sys.stderr)
                continue
            ax2.plot(xs, ys, linestyle='--', label=m)
            plotted_any = True
        ax2.set_ylabel(" / ".join(g2))

        # combined legend
        lines1, labels1 = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        if lines1 or lines2:
            ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left')
    else:
        if plotted_any:
            ax1.legend(loc='upper left')

    if not plotted_any:
        print("No metrics plotted (none found). Exiting.", file=sys.stderr)
        return

    ax1.set_xlabel("Time (s)")
    if title:
        plt.title(title)
    plt.grid(True)
    plt.tight_layout()
    if out_fname:
        plt.savefig(out_fname)
        print(f"Saved plot to {out_fname}")
    else:
        plt.show()

def slice_lines_by_percent(lines, start_percent, end_percent):
    """
    Return the slice of lines between start_percent and end_percent.
    start_percent and end_percent are floats in [0,100].
    """
    n = len(lines)
    if n == 0:
        return []

    # clamp and convert to indices
    sp = max(0.0, min(100.0, start_percent))
    ep = max(0.0, min(100.0, end_percent))

    if ep <= sp:
        raise ValueError(f"end-percent ({end_percent}) must be greater than start-percent ({start_percent})")

    start_idx = int((sp / 100.0) * n)
    end_idx = int((ep / 100.0) * n)

    # ensure at least one line if percentages select very small range
    if end_idx <= start_idx:
        end_idx = min(start_idx + 1, n)

    return lines[start_idx:end_idx]

def main():
    ap = argparse.ArgumentParser(description="Plot sparse metrics (no NaNs appended).")
    ap.add_argument('--file', '-f', help='Input file (default stdin)', default=None)
    ap.add_argument('-g1', nargs='+', help='Metrics for group 1 (left axis)', required=True)
    ap.add_argument('-g2', nargs='*', help='Metrics for group 2 (right axis)', default=[])
    ap.add_argument('-o', '--out', help='Output filename (png/pdf). If omitted, shows interactively', default=None)
    ap.add_argument('--title', help='Plot title', default=None)
    ap.add_argument('--start-percent', help='Start percent of file to read (0-100)', type=float, default=0.0)
    ap.add_argument('--end-percent', help='End percent of file to read (0-100)', type=float, default=100.0)
    args = ap.parse_args()

    if args.file:
        with open(args.file, 'r') as f:
            lines = f.readlines()
    else:
        lines = sys.stdin.readlines()

    if not lines:
        print("No input lines", file=sys.stderr)
        sys.exit(1)

    # Apply percent slicing
    try:
        sliced = slice_lines_by_percent(lines, args.start_percent, args.end_percent)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    if len(sliced) == 0:
        print("Selected slice contains no lines after applying percentages.", file=sys.stderr)
        sys.exit(1)

    # Informational print (to stderr) so piping stdout isn't polluted
    if args.start_percent != 0.0 or args.end_percent != 100.0:
        print(f"Reading lines {len(lines)} -> slice [{args.start_percent}%, {args.end_percent}%] -> {len(sliced)} lines", file=sys.stderr)

    metrics = parse_sparse(sliced)
    plot_groups_sparse(metrics, args.g1, args.g2, out_fname=args.out, title=args.title)

if __name__ == '__main__':
    main()
