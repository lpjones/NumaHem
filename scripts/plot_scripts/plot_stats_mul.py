#!/usr/bin/env python3
"""
plot_stats_sparse.py

Parse lines containing tokens like: name: [value]
and plot up to two metric groups. Metrics are stored sparsely:
only present values are appended (no NaNs).

Now supports multiple input files and plots the same metrics from each file
on the same axes.

Usage examples:
  ./plot_stats_sparse.py -f stats1.txt stats2.txt -g1 dram_free dram_used dram_size
  ./plot_stats_sparse.py -f stats1.txt stats2.txt -g1 dram_free -g2 wrapped_records -o out.pdf
  cat stats.txt | ./plot_stats_sparse.py -g1 dram_free
  ./plot_stats_sparse.py -f stats.txt -g1 dram_free --start-percent 10 --end-percent 50

Options:
  -f/--file    input files (default stdin if omitted)
  -g1 ...      metrics for left axis (required)
  -g2 ...      metrics for right axis (optional)
  --labels     labels for each file (optional, must match number of files)
  -o/--out     output filename (png/pdf). If omitted, show interactively
  --start-percent float  start percentage of file to read (0-100), default 0
  --end-percent   float  end percentage of file to read (0-100], default 100
"""
from collections import defaultdict
import re
import sys
import argparse
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import os

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent

# keep using your local style if present; otherwise ignore failure silently
mplstyle = os.path.join(script_dir, 'ieee.mplstyle')
try:
    plt.style.use(mplstyle)
except Exception:
    try:
        plt.style.use('science')
    except Exception:
        pass

# small regex fix: allow optional +/-
RE_METRIC = re.compile(r'([A-Za-z0-9_]+)\s*:\s*\[\s*([+-]?[0-9]*\.?[0-9]+(?:[eE][+-]?\d+)?)\s*\]')

def parse_sparse(lines):
    """
    Parse lines and return a dict:
      metrics[name] = (list_of_x, list_of_y)

    The 'period' logic is preserved: detect a leading-tabbed block to decide rate.
    x values are float(idx)/period
    """
    period = 1
    # determine period by looking at initial lines similar to original logic
    if len(lines) > 1:
        for line in lines[1:]:
            if line and line[0] != '\t':
                break
            period += 1

    metrics = defaultdict(lambda: ([], []))  # name -> (xs, ys)
    for idx, line in enumerate(lines):
        # keep original scale behavior
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

def read_and_parse_file(path_or_handle, start_percent, end_percent):
    """
    path_or_handle: either a filename (str/Path) or a file-like object (stdin)
    Returns parsed metrics dict for that file/stream.
    """
    if hasattr(path_or_handle, 'read'):
        lines = path_or_handle.readlines()
    else:
        with open(path_or_handle, 'r') as f:
            lines = f.readlines()

    if not lines:
        return {}

    sliced = slice_lines_by_percent(lines, start_percent, end_percent)
    return parse_sparse(sliced)

def plot_groups_multi(metrics_list, file_labels, g1, g2, out_fname=None, title=None):
    """
    metrics_list: list of metrics dicts, one dict per input file
    file_labels: list of labels (same length as metrics_list)
    g1: list of metrics for left axis
    g2: list of metrics for right axis
    """
    fig, ax1 = plt.subplots(figsize=(10,6))
    plotted_any = False

    # plotting group1 (left axis)
    for file_idx, metrics in enumerate(metrics_list):
        label_prefix = file_labels[file_idx]
        for m in g1:
            if m not in metrics:
                print(f"Warning: metric '{m}' not found in file '{label_prefix}'; skipping.", file=sys.stderr)
                continue
            xs, ys = metrics[m]
            if len(xs) == 0:
                print(f"Warning: metric '{m}' in file '{label_prefix}' has no samples; skipping.", file=sys.stderr)
                continue
            ax1.plot(xs, ys, label=f"{label_prefix}")
            plotted_any = True
    if g1:
        ax1.set_ylabel(" / ".join(g1))

    ax2 = None
    if g2:
        ax2 = ax1.twinx()
        for file_idx, metrics in enumerate(metrics_list):
            label_prefix = file_labels[file_idx]
            for m in g2:
                if m not in metrics:
                    print(f"Warning: metric '{m}' not found in file '{label_prefix}'; skipping.", file=sys.stderr)
                    continue
                xs, ys = metrics[m]
                if len(xs) == 0:
                    print(f"Warning: metric '{m}' in file '{label_prefix}' has no samples; skipping.", file=sys.stderr)
                    continue
                # dashed lines for group2
                ax2.plot(xs, ys, linestyle='--', label=f"{label_prefix}: {m}")
                plotted_any = True
        ax2.set_ylabel(" / ".join(g2))

    if not plotted_any:
        print("No metrics plotted (none found). Exiting.", file=sys.stderr)
        return

    # build combined legend
    lines = []
    labels = []
    l1, lab1 = ax1.get_legend_handles_labels()
    lines.extend(l1); labels.extend(lab1)
    if ax2:
        l2, lab2 = ax2.get_legend_handles_labels()
        lines.extend(l2); labels.extend(lab2)

    if lines:
        ax1.legend(lines, labels, loc='upper left')

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

def main():
    ap = argparse.ArgumentParser(description="Plot sparse metrics (no NaNs appended). Supports multiple files.")
    ap.add_argument('--file', '-f', nargs='+', help='Input file(s). If omitted, reads a single stream from stdin', default=None)
    ap.add_argument('-g1', nargs='+', help='Metrics for group 1 (left axis)', required=True)
    ap.add_argument('-g2', nargs='*', help='Metrics for group 2 (right axis)', default=[])
    ap.add_argument('--labels', nargs='*', help='Optional labels for each input file (must match number of files)', default=None)
    ap.add_argument('-o', '--out', help='Output filename (png/pdf). If omitted, shows interactively', default=None)
    ap.add_argument('--title', help='Plot title', default=None)
    ap.add_argument('--start-percent', help='Start percent of file to read (0-100)', type=float, default=0.0)
    ap.add_argument('--end-percent', help='End percent of file to read (0-100)', type=float, default=100.0)
    args = ap.parse_args()

    files = args.file
    labels = args.labels

    # if no files provided, read single stream from stdin
    if not files:
        # read stdin once and parse
        metrics = read_and_parse_file(sys.stdin, args.start_percent, args.end_percent)
        metrics_list = [metrics]
        file_labels = [labels[0] if labels and len(labels) >= 1 else 'stdin']
    else:
        # multiple files provided
        metrics_list = []
        file_labels = []
        for f in files:
            try:
                metrics = read_and_parse_file(f, args.start_percent, args.end_percent)
            except Exception as e:
                print(f"Error reading file '{f}': {e}", file=sys.stderr)
                sys.exit(1)
            metrics_list.append(metrics)
            file_labels.append(Path(f).name)

        # validate labels if provided
        if labels:
            if len(labels) != len(metrics_list):
                print(f"Error: --labels length ({len(labels)}) must match number of files ({len(metrics_list)})", file=sys.stderr)
                sys.exit(1)
            file_labels = labels

    # Informational print (to stderr) so piping stdout isn't polluted
    if args.start_percent != 0.0 or args.end_percent != 100.0:
        nf = len(files) if files else 1
        print(f"Reading {nf} file(s) -> slice [{args.start_percent}%, {args.end_percent}%] applied to each", file=sys.stderr)

    plot_groups_multi(metrics_list, file_labels, args.g1, args.g2, out_fname=args.out, title=args.title)

if __name__ == '__main__':
    main()
