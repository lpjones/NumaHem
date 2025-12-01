#!/usr/bin/env python3
"""
plot_epoch_images.py

Usage:
    python3 plot_epoch_images.py file1.txt [file2.txt ...] -o output.png

This script extracts "Epoch <N>: <val> images/sec" lines from one or more
app output files and plots images/sec vs epoch for each file on the same graph.
"""
from __future__ import annotations
import argparse
import os
import re
import sys
from typing import Dict, List, Tuple

args = None

EPOCH_RE = re.compile(
    r"Epoch\s*[:\s]*\s*([0-9]+)\s*\s*[:]\s*([0-9]*\.?[0-9]+)\s*images/sec",
    flags=re.IGNORECASE,
)

def parse_epoch_images(filepath: str) -> List[Tuple[int, float]]:
    """
    Return a sorted list of (epoch, images_per_sec) found in the file.
    If the same epoch appears multiple times, the last occurrence is used.
    """
    if not os.path.isfile(filepath):
        return []

    values: Dict[int, float] = {}
    with open(filepath, "r", errors="replace") as f:
        for line in f:
            m = EPOCH_RE.search(line)
            if not m:
                # some logs might use "Epoch 1: 4.13 images/sec" without an extra colon or with other spacing,
                # try a more permissive pattern:
                m2 = re.search(r"Epoch\s*([0-9]+)\s*[:]\s*([0-9]*\.?[0-9]+)\s*images/sec", line, flags=re.IGNORECASE)
                m = m2
            if m:
                try:
                    epoch = int(m.group(1))
                    val = float(m.group(2))
                    values[epoch] = val
                except Exception:
                    # ignore bad parses
                    pass

    # Return epochs sorted ascending
    return sorted(values.items(), key=lambda kv: kv[0])

def plot_files(filepaths: List[str], outpath: str, title: str = "ResNet50 Throughput"):
    import matplotlib.pyplot as plt

    if not filepaths:
        print("No input files provided.", file=sys.stderr)
        return 2

    datasets = []
    if args.labels == None:
        labels = []
    else:
        labels = args.labels
    max_epoch = 0
    for p in filepaths:
        data = parse_epoch_images(p)
        if data:
            epochs, vals = zip(*data)
            print(sum(vals) / len(vals))
            max_epoch = max(max_epoch, max(epochs))
            datasets.append((list(epochs), list(vals)))
        else:
            datasets.append(([], []))
        if args.labels == None:
            labels.append(os.path.splitext(os.path.basename(p))[0])

    if all(len(ds[0]) == 0 for ds in datasets):
        print("No epoch/images/sec entries found in any input files.", file=sys.stderr)
        return 3

    plt.figure(figsize=(10, 5))
    styles = ['-', '--', '-.', ':']
    markers = ['o', 's', '^', 'd', 'v', 'P', 'X']
    for idx, ((epochs, vals), label) in enumerate(zip(datasets, labels)):
        style = styles[idx % len(styles)]
        marker = markers[idx % len(markers)]
        if not epochs:
            # plot invisible entry for legend
            plt.plot([], [], label=f"{label} (no data)")
            continue
        plt.plot(epochs, vals, label=label)

    plt.xlabel("Epoch")
    plt.ylabel("Images / sec")
    plt.title(title)
    plt.grid(True)
    plt.legend(fontsize="small", loc="best")
    plt.xticks(range(1, max_epoch + 1))
    plt.tight_layout()

    # Ensure output dir exists
    odir = os.path.dirname(outpath)
    if odir and not os.path.exists(odir):
        os.makedirs(odir, exist_ok=True)

    plt.savefig(outpath, dpi=300)
    plt.close()
    print(f"Saved plot to {outpath}")
    return 0

def main(argv=None):
    global args
    argv = argv if argv is not None else sys.argv[1:]
    parser = argparse.ArgumentParser(description="Plot images/sec per epoch from one or more app logs.")
    parser.add_argument("files", nargs="+", help="One or more app output files to parse.")
    parser.add_argument("-o", "--output", required=True, help="Output image filename (e.g. out.png).")
    parser.add_argument("--title", default="ResNet50 Throughput", help="Plot title.")
    parser.add_argument("--labels", nargs='+', default=None, help="labels for graph lines")
    args = parser.parse_args(argv)

    # validate files exist (warn but continue if some missing)
    missing = [p for p in args.files if not os.path.isfile(p)]
    if missing:
        for p in missing:
            print(f"Warning: input file '{p}' not found; it will be skipped.", file=sys.stderr)
        # remove missing files from list
        files = [p for p in args.files if os.path.isfile(p)]
    else:
        files = args.files

    if not files:
        print("No valid input files to process.", file=sys.stderr)
        return 4

    return plot_files(files, args.output, title=args.title)

if __name__ == "__main__":
    sys.exit(main())
