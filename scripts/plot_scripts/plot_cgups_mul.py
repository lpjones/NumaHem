#!/usr/bin/env python3
"""
plot_cgups_numbers_multi.py

Usage:
    python3 plot_cgups_numbers_multi.py <input_file1> [<input_file2> ...] <output_image>

This script extracts decimal numbers from one or more text files (skips tokens starting with 0x)
and plots each file's sequence as a separate line on the same figure.

Requirements:
    - matplotlib installed (pip install matplotlib)
"""

import sys
import os
import argparse
import matplotlib.pyplot as plt
from pathlib import Path

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent

plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

def parse_args():
    p = argparse.ArgumentParser(
        description="Extract decimal numbers from one or more files (skip hex 0x...) and plot them on the same graph."
    )
    p.add_argument("inputs", nargs='+', help="One or more input text files.")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
    p.add_argument("--labels", nargs='+', default=None, help="labels for graph lines")
    p.add_argument("--xlabel", default="Seconds", help="Label for the x-axis.")
    p.add_argument("--ylabel", default="Throughput (bytes)", help="Label for the y-axis.")
    p.add_argument("--title", default="GUPS Throughput", help="Plot title.")
    return p.parse_args()

def extract_numbers_from_file(path):
    nums = []
    with open(path, 'r', errors='replace') as f:
        for line in f:
            # split by whitespace
            tokens = line.strip().split()
            for tok in tokens:
                tok = tok.strip().rstrip(',')  # drop trailing commas
                # skip obvious hex tokens that start with 0x (case-insensitive)
                if tok.lower().startswith("0x"):
                    continue
                # If token is pure decimal digits, accept it
                if tok.isdigit():
                    nums.append(int(tok))
                # else: ignore (this will ignore hex addresses and text)
    return nums

def plot_multiple(datasets, labels, outpath, xlabel, ylabel, title):
    import matplotlib.pyplot as plt

    if not datasets:
        print("No datasets provided.", file=sys.stderr)
        return 2

    # verify at least one dataset has data
    if all(len(d) == 0 for d in datasets):
        print("No decimal numbers found in any input files.", file=sys.stderr)
        return 2

    plt.figure(figsize=(10, 4))
    for nums, label in zip(datasets, labels):
        if not nums:
            # plot an empty placeholder so legend remains consistent
            plt.plot([], [], label=f"{label} (no data)")
            continue
        x = list(range(len(nums)))
        y = nums
        plt.plot(x, y, label=label)

    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    plt.legend(loc="best")

    # Ensure parent dir exists
    odir = os.path.dirname(outpath)
    if odir and not os.path.exists(odir):
        os.makedirs(odir, exist_ok=True)

    plt.savefig(outpath)
    plt.close()
    print(f"Saved plot to {outpath}")
    return 0

def main():
    args = parse_args()

    outpath = args.output
    input_paths = args.inputs

    # Validate inputs exist
    missing = [p for p in input_paths if not os.path.isfile(p)]
    if missing:
        for p in missing:
            print(f"Input file '{p}' not found.", file=sys.stderr)
        return 4

    datasets = []
    if args.labels == None:
        labels = []
    else:
        labels = args.labels
    for p in input_paths:
        nums = extract_numbers_from_file(p)
        datasets.append(nums)
        if args.labels == None:
            labels.append(os.path.splitext(os.path.basename(p))[0])

    rc = plot_multiple(datasets, labels, outpath, args.xlabel, args.ylabel, args.title)
    return rc

if __name__ == "__main__":
    sys.exit(main())
