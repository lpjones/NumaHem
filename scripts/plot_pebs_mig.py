#!/usr/bin/env python3
"""
plot_mig_latencies.py

Parse a log file containing lines like:
  [35.700894350] PEBS: Made hot: 0x792975200000
  [35.700850735] MIG: Finished migration: 0x792976e00000

For every "PEBS: Made hot" entry, find the next (future) "MIG: Finished migration"
entry with the same hex address and compute the time difference (finished - made).

Bin those time differences by 1-second intervals using the integer second of the
"made" timestamp (floor), average the deltas in each bin, and plot the averages
over time. Saves the plot to a PNG (or other matplotlib-supported) file.

Usage:
  python plot_mig_latencies.py --log-file /path/to/log.txt --out out.png

"""
import re
import argparse
import math
from collections import defaultdict, OrderedDict
import matplotlib.pyplot as plt

def parse_log_file(path):
    """
    Parse the log file and return two dicts:
      made_by_addr: addr -> list of made_times (sorted ascending)
      finished_by_addr: addr -> list of finished_times (sorted ascending)
    """
    # regex for timestamp at start: [35.700894350]
    ts_re = re.compile(r'^\s*\[([0-9]+\.[0-9]+)\]')
    # regexes for events
    made_re = re.compile(r'PEBS:\s*Made hot[:\s]*([0-9a-fxA-F]+)')
    finished_re = re.compile(r'MIG:\s*Finished migration[:\s]*([0-9a-fxA-F]+)')

    made_by_addr = defaultdict(list)
    finished_by_addr = defaultdict(list)

    with open(path, 'r', errors='replace') as f:
        for line in f:
            # timestamp
            m_ts = ts_re.match(line)
            if not m_ts:
                continue
            t = float(m_ts.group(1))

            # check made
            m_made = made_re.search(line)
            if m_made:
                addr = m_made.group(1).lower()
                made_by_addr[addr].append(t)
                continue

            # check finished
            m_finished = finished_re.search(line)
            if m_finished:
                addr = m_finished.group(1).lower()
                finished_by_addr[addr].append(t)
                continue

    # Ensure lists are sorted (they should be because file read is chronological,
    # but just in case)
    for d in (made_by_addr, finished_by_addr):
        for k in d:
            d[k].sort()
    return made_by_addr, finished_by_addr

def match_made_to_finished(made_by_addr, finished_by_addr):
    """
    For each address, iterate made times and find the next finished time > made_time.
    Each finished time is used at most once (consumed).
    Returns a list of (made_time, delta_seconds).
    """
    matches = []
    for addr, made_list in made_by_addr.items():
        finished_list = finished_by_addr.get(addr, [])
        if not finished_list:
            continue
        j = 0
        n_finished = len(finished_list)
        for made_t in made_list:
            # advance j until finished_list[j] > made_t
            while j < n_finished and finished_list[j] <= made_t:
                j += 1
            if j >= n_finished:
                break  # no future finished for this made
            fin_t = finished_list[j]
            delta = fin_t - made_t
            # only positive deltas are meaningful; skip negative or zero (if any)
            if delta >= 0.0:
                matches.append((made_t, delta))
            j += 1  # consume this finished entry so it won't be reused
    # sort by made time (just in case)
    matches.sort(key=lambda x: x[0])
    return matches

def get_final_log_time(log_file_path):
    """
    Scan the log file to find the last timestamp in the log.
    """
    ts_re = re.compile(r'^\s*\[([0-9]+\.[0-9]+)\]')
    last_ts = 0.0
    with open(log_file_path, 'r', errors='replace') as f:
        for line in f:
            m_ts = ts_re.match(line)
            if m_ts:
                t = float(m_ts.group(1))
                if t > last_ts:
                    last_ts = t
    return last_ts

def bin_and_average(matches, log_end_time, bin_size_secs=1.0):
    """
    Bin matches into integer-second bins based on floor(made_time).
    Any bin without measurements is filled with 0.
    Uses log_end_time to extend the bins to the end of the log.
    Returns ordered mapping sec -> average_delta (seconds).
    """
    bins = defaultdict(list)
    for made_t, delta in matches:
        sec = int(math.floor(made_t / bin_size_secs) * bin_size_secs)
        bins[sec].append(delta)

    if not bins:
        return OrderedDict()

    min_sec = min(bins.keys())
    max_sec = int(math.ceil(log_end_time / bin_size_secs))

    averaged = OrderedDict()
    for s in range(min_sec, max_sec + 1):
        values = bins.get(s)
        if values:
            averaged[s] = sum(values) / len(values)
        else:
            averaged[s] = 0.0  # fill empty bins with 0

    return averaged


def plot_binned_averages(binned_avg, out_path, title=None):
    if not binned_avg:
        raise ValueError("No matched events to plot.")

    xs = list(binned_avg.keys())
    ys = [binned_avg[x] for x in xs]

    plt.figure(figsize=(12, 5))
    plt.yscale('log')
    plt.plot(xs, ys, marker='.', linestyle='-')
    plt.xlabel('Log time (seconds)')
    plt.ylabel('Average migration latency (s) per 1s bin')
    if title:
        plt.title(title)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()

def main():
    ap = argparse.ArgumentParser(description="Plot migration latencies matched to PEBS Made hot events.")
    ap.add_argument('--log-file', '-i', required=True, help='Path to log file')
    ap.add_argument('--out', '-o', default='mig_latencies.png', help='Output image file (png, pdf, etc.)')
    ap.add_argument('--bin-size', type=float, default=1.0, help='Bin size in seconds (default 1.0)')
    args = ap.parse_args()

    made_by_addr, finished_by_addr = parse_log_file(args.log_file)
    matches = match_made_to_finished(made_by_addr, finished_by_addr)

    if not matches:
        print("No matches found between 'PEBS: Made hot' and future 'MIG: Finished migration' events.")
        return
    log_end_time = get_final_log_time(args.log_file)
    binned = bin_and_average(matches, log_end_time, bin_size_secs=args.bin_size)
    # binned = bin_and_average(matches, bin_size_secs=args.bin_size)

    title = f"Avg migration latency per {args.bin_size:.0f}s bin (matched events: {len(matches)})"
    plot_binned_averages(binned, args.out, title=title)
    print(f"Saved plot to {args.out}. Matched events: {len(matches)}. Bins: {len(binned)}")

if __name__ == '__main__':
    main()
