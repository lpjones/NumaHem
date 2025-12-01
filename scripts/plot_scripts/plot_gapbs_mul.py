#!/usr/bin/env python3
import sys
import os
import argparse
import re

def parse_args():
    p = argparse.ArgumentParser(
        description="Extract trial times from multiple BFS text files and plot them on one graph."
    )
    p.add_argument("inputs", nargs="+", help="One or more input text files (each must contain 'bfs' in the filename).")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
    p.add_argument("--labels", nargs='+', default=None, help="labels for graph lines")
    p.add_argument("--title", help="title")

    return p.parse_args()


TRIAL_RE = re.compile(r'Trial Time:\s*([0-9]*\.?[0-9]+)')

def parse_trial_times(file_path):
    """Return a list of float trial times from a text file."""
    vals = []
    with open(file_path, 'r', errors='ignore') as f:
        for line in f:
            m = TRIAL_RE.search(line)
            if m:
                try:
                    vals.append(float(m.group(1)))
                except ValueError:
                    pass
    return vals


def plot_multiple(datasets, labels, outpath, title):
    import matplotlib.pyplot as plt

    plt.figure(figsize=(10, 4))

    for times, label in zip(datasets, labels):
        if not times:
            plt.plot([], [], label=f"{label} (no data)")
            continue
        x = list(range(len(times)))
        plt.plot(x, times, label=label)

    plt.xlabel("Trial")
    plt.ylabel("Trial Time (s)")
    plt.title(title)
    plt.grid(True)
    plt.legend(fontsize="small")
    plt.tight_layout()

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
    title = args.title

    # Validate each input file
    for path in inputs:
        if not os.path.isfile(path):
            print(f"Input file '{path}' not found.", file=sys.stderr)
            return 4

    datasets = []
    if args.labels == None:
        labels = []
    else:
        labels = args.labels

    for path in inputs:
        times = parse_trial_times(path)
        datasets.append(times)
        if args.labels == None:
            labels.append(os.path.splitext(os.path.basename(path))[0])

    return plot_multiple(datasets, labels, outpath, title)


if __name__ == "__main__":
    sys.exit(main())
