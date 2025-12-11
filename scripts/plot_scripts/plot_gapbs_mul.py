#!/usr/bin/env python3
import sys
import os
import argparse
import re
import matplotlib.pyplot as plt
from pathlib import Path
from parse_apps import parse_gapbs, check_args
from plot_apps import plot_multiple

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent
plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

def parse_args():
    p = argparse.ArgumentParser(
        description="Extract trial times from multiple BFS text files and plot them on one graph."
    )
    p.add_argument("inputs", nargs="+", help="One or more input text files (each must contain 'bfs' in the filename).")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
    p.add_argument("--labels", nargs='+', default=None, help="labels for graph lines")
    p.add_argument("--xlabel", default="Trial", help="Label for the x-axis.")
    p.add_argument("--ylabel", default="Trial Time (s)", help="Label for the y-axis.")
    p.add_argument("--yrange", nargs=2, default=[0,2], help="y range")
    p.add_argument("--title", help="title")

    return p.parse_args()


def main():
    args = parse_args()
    check_args(args)

    datasets = []
    for path in args.inputs:
        times = parse_gapbs(path)
        datasets.append(times)

    return plot_multiple(datasets, args)


if __name__ == "__main__":
    sys.exit(main())
