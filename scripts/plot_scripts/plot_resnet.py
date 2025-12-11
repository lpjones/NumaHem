#!/usr/bin/env python3
import sys
import os
import argparse
import matplotlib.pyplot as plt
from pathlib import Path
from parse_apps import parse_resnet, check_args
from plot_apps import plot_multiple

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent
plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

def parse_args():
    p = argparse.ArgumentParser(
        description="Extract Im/sec from multiple resnet text files and plot them on one graph."
    )
    p.add_argument("inputs", nargs="+", help="One or more input text files.")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
    p.add_argument("--labels", nargs='+', default=None, help="labels for graph lines")
    p.add_argument("--xlabel", default="Epoch", help="Label for the x-axis.")
    p.add_argument("--ylabel", default="Images/sec", help="Label for the y-axis.")
    p.add_argument("--yrange", nargs=2, default=[0,6], help="y range")
    p.add_argument("--title", help="title")

    return p.parse_args()


def main():
    args = parse_args()
    check_args(args)

    datasets = []
    for path in args.inputs:
        times = parse_resnet(path)
        datasets.append(times)

    return plot_multiple(datasets, args)


if __name__ == "__main__":
    sys.exit(main())
