#!/usr/bin/env python3
import numpy as np
import re
import argparse
import os
import sys
import time
from collections import Counter
import matplotlib.pyplot as plt

from pathlib import Path

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent

plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

def parse_args():
    p = argparse.ArgumentParser(description="Compare prediction accuracy vs window between two runs")
    # run 1
    p.add_argument("--log1", required=True, help="log file for run1 (text) with [timestamp] ... PRED: 0xaddr ...")
    p.add_argument("--trace1", required=True, help="trace file for run1 (binary)")
    p.add_argument("--pred1", required=True, help="pred file for run1 (binary)")
    p.add_argument("--cold1", required=False, default=None, help="cold file for run1 (binary)")
    # run 2
    p.add_argument("--log2", required=True, help="log file for run2")
    p.add_argument("--trace2", required=True, help="trace file for run2")
    p.add_argument("--pred2", required=True, help="pred file for run2")
    p.add_argument("--cold2", required=False, default=None, help="cold file for run2")
    # windows
    p.add_argument("--windows", type=float, required=True, nargs='+',
                   help="Window sizes (normalized units)")
    # label names for legend
    p.add_argument("--labels", nargs=2, required=True,
                   help="Labels for run1 and run2 in the legend")
    p.add_argument("--out", default="accuracy_compare.png",
                   help="Output plot filename")
    p.add_argument("--title", default="Prediction Accuracy", help="Plot title.")
    
    p.add_argument("--show", action="store_true", help="Show plot after saving")
    return p.parse_args()

def parse_log(file_path):
    ts_re = re.compile(r'\[(?P<ts>[0-9.]+)\]')
    with open(file_path, 'r') as f:
        first_line = f.readline()
        m = ts_re.search(first_line)
        if not m:
            raise ValueError("No timestamp found in first line")
        first_ts = float(m.group("ts"))

        f.seek(0, 2)
        pos = f.tell()
        while pos > 0:
            pos -= 1
            f.seek(pos)
            if f.read(1) == '\n':
                break
        last_line = f.readline().strip()
        if last_line == "":
            f.seek(0)
            lines = [l.strip() for l in f.readlines() if l.strip()]
            last_line = lines[-1]

        m = ts_re.search(last_line)
        if not m:
            raise ValueError("No timestamp in last line")
        last_ts = float(m.group("ts"))

    return first_ts, last_ts

def parse_trace(file_path, start_percent=0, end_percent=100):
    record_dtype = np.dtype([
        ('cycle', '<u8'),
        ('va',    '<u8'),
        ('ip',    '<u8'),
        ('cpu',   '<u4'),
        ('event', 'u1'),
    ])
    record_size = record_dtype.itemsize
    total_bytes = os.path.getsize(file_path)
    total_records = total_bytes // record_size

    start_record = int(total_records * start_percent / 100)
    end_record   = int(total_records * end_percent / 100)
    count = end_record - start_record
    if count <= 0:
        print("No records in", file_path); sys.exit(1)

    with open(file_path, 'rb') as f:
        f.seek(start_record * record_size)
        arr = np.fromfile(f, dtype=record_dtype, count=count)

    return (arr['cycle'], arr['va'], arr['cpu'], arr['ip'], arr['event'])

def accuracy_multiwindows(cyc, va, p_cyc, p_va, windows):
    """
    Compute accuracy for many windows at once.
    cyc, va must be sorted by cyc
    p_cyc, p_va must be sorted by p_cyc
    windows must be sorted ascending
    """

    cyc = np.asarray(cyc)
    va  = np.asarray(va)
    p_cyc = np.asarray(p_cyc)
    p_va  = np.asarray(p_va)

    N = len(cyc)
    W = len(windows)

    # One left/right pointer per window
    left  = [0] * W
    right = [0] * W
    counters = [Counter() for _ in range(W)]

    hits   = [0] * W
    misses = [0] * W
    total  = len(p_cyc)

    for idx, (ts, paddr) in enumerate(zip(p_cyc, p_va)):
        if idx % 512 == 0:
            print(f"\r{idx/total*100:.2f}%", end='', flush=True)

        # Process each window size independently
        for w_i, window in enumerate(windows):
            upper = ts + window

            # expand right
            r = right[w_i]
            while r < N and cyc[r] <= upper:
                counters[w_i][int(va[r])] += 1
                r += 1
            right[w_i] = r

            # shrink left
            l = left[w_i]
            while l < N and cyc[l] < ts:
                a = int(va[l])
                counters[w_i][a] -= 1
                if counters[w_i][a] == 0:
                    del counters[w_i][a]
                l += 1
            left[w_i] = l

            # membership
            if int(paddr) in counters[w_i]:
                hits[w_i] += 1
            else:
                misses[w_i] += 1

    print("\r", end='', flush=True)

    accuracies = [h / total * 100.0 for h in hits]
    return accuracies


def evaluate_for_windows(cyc_raw, va_raw, p_cyc_raw, p_va_raw, windows):
    windows = sorted(windows)

    max_cyc = max(np.max(cyc_raw), np.max(p_cyc_raw))
    min_cyc = min(np.min(cyc_raw), np.min(p_cyc_raw))

    cyc = (cyc_raw - min_cyc) / (max_cyc - min_cyc)
    p_cyc = (p_cyc_raw - min_cyc) / (max_cyc - min_cyc)

    # sort both sequences
    idx = np.argsort(cyc)
    cyc = cyc[idx]
    va  = va_raw[idx]

    pidx = np.argsort(p_cyc)
    p_cyc = p_cyc[pidx]
    p_va  = p_va_raw[pidx]

    # optimized call:
    accs = accuracy_multiwindows(cyc, va, p_cyc, p_va, windows)

    return accs


def load_run(trace_file, pred_file, cold_file=None):
    cyc, va, _, _, _ = parse_trace(trace_file)
    p_cyc, p_va, _, _, _ = parse_trace(pred_file)
    if cold_file:
        c_cyc, c_va, _, _, _ = parse_trace(cold_file)
    else:
        c_cyc = c_va = None
    return cyc, va, p_cyc, p_va, c_cyc, c_va

def main():
    args = parse_args()
    windows = sorted(args.windows)

    cyc1, va1, p_cyc1, p_va1, _, _ = load_run(args.trace1, args.pred1, args.cold1)
    cyc2, va2, p_cyc2, p_va2, _, _ = load_run(args.trace2, args.pred2, args.cold2)

    print("Evaluating run1...")
    run1_accs = evaluate_for_windows(cyc1, va1, p_cyc1, p_va1, windows)
    print("Evaluating run2...")
    run2_accs = evaluate_for_windows(cyc2, va2, p_cyc2, p_va2, windows)

    plt.figure(figsize=(8,5))
    plt.xscale("log")                       # <-- log scale
    plt.plot(windows, run1_accs, label=args.labels[0])
    plt.plot(windows, run2_accs, label=args.labels[1])
    plt.xlabel("Window size (log scale)")
    plt.ylabel("Accuracy (%)")
    plt.title(args.title)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(args.out)
    print("Saved plot to", args.out)

    if args.show:
        plt.show()

if __name__ == "__main__":
    main()
