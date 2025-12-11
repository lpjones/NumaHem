#!/usr/bin/env python3
"""
plot_cgups_mul.py

Usage:
    python3 plot_cgups_mul.py <input_file1> [<input_file2> ...] <output_image>

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
from parse_apps import parse_cgups, check_args
from plot_apps import plot_multiple

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
    p.add_argument("--yrange", nargs=2, default=[0,1.6e8], help="y range")
    p.add_argument("--title", default="GUPS Throughput", help="Plot title.")
    return p.parse_args()

def main():
    args = parse_args()
    check_args(args)

    datasets = []
    for p in args.inputs:
        nums = parse_cgups(p)
        datasets.append(nums)

    rc = plot_multiple(datasets, args)
    return rc

if __name__ == "__main__":
    sys.exit(main())
