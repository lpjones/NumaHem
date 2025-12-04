#!/usr/bin/env python3
"""
plot_stream_trials_multi.py

Usage:
    python3 plot_stream_trials_multi.py file1.txt [file2.txt ...] output.png

This script extracts iteration times from one or more Stream app output files
(using lines like "Iter 1: time = 0.123 seconds") and plots them on the same graph.
Each file becomes one line (labelled by its basename).
"""
from __future__ import annotations
import sys
import os
import argparse
import re
from typing import List
import matplotlib.pyplot as plt
from pathlib import Path

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent

plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

def parse_args():
    p = argparse.ArgumentParser(
        description="Extract Iter times from one or more files (skip hex 0x...) and plot them on the same graph."
    )
    p.add_argument("inputs", nargs="+", help="One or more input text files (each should contain 'stream' in the filename).")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
    p.add_argument("--xlabel", default="Trial", help="Label for the x-axis.")
    p.add_argument("--ylabel", default="Trial Time (s)", help="Label for the y-axis.")
    p.add_argument("--labels", nargs='+', default=None, help="labels for graph lines")
    p.add_argument("--title", default="Stream trial times", help="Plot title.")
    return p.parse_args()


ITER_RE = re.compile(r'Iter\s+\d+:\s*time\s*=\s*([0-9]*\.?[0-9]+)\s*seconds', re.IGNORECASE)

def parse_trial_times(file_path: str) -> List[float]:
    """Return a list of float trial times from a text file."""
    vals: List[float] = []
    if not os.path.isfile(file_path):
        return vals
    with open(file_path, 'r', errors='ignore') as f:
        for line in f:
            m = ITER_RE.search(line)
            if m:
                try:
                    vals.append(float(m.group(1)))
                except ValueError:
                    pass
    return vals

def plot_multiple(datasets: List[List[float]], labels: List[str], outpath: str, xlabel: str, ylabel: str, title: str) -> int:
    import matplotlib.pyplot as plt

    if not datasets:
        print("No datasets provided.", file=sys.stderr)
        return 2

    if all(len(d) == 0 for d in datasets):
        print("No iteration times found in any input files.", file=sys.stderr)
        return 2

    plt.figure(figsize=(10, 4))

    for idx, (nums, label) in enumerate(zip(datasets, labels)):
        if not nums:
            plt.plot([], [], label=f"{label} (no data)")
            continue
        # Use 1-based trial numbering on x-axis
        x = list(range(1, len(nums) + 1))
        plt.plot(x, nums, label=label)

    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True)
    plt.legend(fontsize="small", loc="best")
    # set x ticks to integer trials if not too many


    odir = os.path.dirname(outpath)
    if odir and not os.path.exists(odir):
        os.makedirs(odir, exist_ok=True)

    plt.savefig(outpath)
    plt.close()
    print(f"Saved plot to {outpath}")
    return 0

def main():
    args = parse_args()

    inputs = args.inputs
    outpath = args.output

    # Validate inputs
    missing = [p for p in inputs if not os.path.isfile(p)]
    if missing:
        for p in missing:
            print(f"Input file '{p}' not found.", file=sys.stderr)
        return 4

    # Enforce 'stream' substring requirement per-file (same behavior as original)
    for p in inputs:
        if "stream" not in p.lower():
            print(f"Input filename '{p}' does not contain 'stream' — aborting.", file=sys.stderr)
            return 3

    datasets = []
    labels = []
    if args.labels == None:
        labels = []
    else:
        labels = args.labels

    for p in inputs:
        nums = parse_trial_times(p)
        datasets.append(nums)
        if args.labels == None:
            labels.append(os.path.splitext(os.path.basename(p))[0])

    return plot_multiple(datasets, labels, outpath, args.xlabel, args.ylabel, args.title)


if __name__ == "__main__":
    sys.exit(main())
