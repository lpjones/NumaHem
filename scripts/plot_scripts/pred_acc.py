#!/usr/bin/env python3
import numpy as np
import re
import argparse
import os
import sys
import time

def parse_args():
    p = argparse.ArgumentParser(description="Get Prediction accuracy using time window from prediction")
    p.add_argument("logfile", help="log file (text) with [timestamp] ... PRED: 0xaddr ...")
    p.add_argument("tracefile", help="trace file (binary) matching record dtype '<QQQIB' (cycle,va,ip,cpu,event)")
    p.add_argument("predfile", help="pred file (binary) matching record dtype '<QQQIB' (cycle,va,ip,cpu,event)")
    p.add_argument("--coldfile", required=False, default=None, help="cold file (binary) matching record dtype '<QQQIB' (cycle,va,ip,cpu,event)")
    p.add_argument("--window", type=float, required=True, help="time window in seconds to look forward from each prediction")
    return p.parse_args()

def parse_log(file_path):
    """
    Extract timestamp from only the first and last line of the file.
    Returns (first_timestamp, last_timestamp)
    """
    ts_re = re.compile(r'\[(?P<ts>[0-9.]+)\]')

    with open(file_path, 'r') as f:
        # ----- first line -----
        first_line = f.readline()
        m = ts_re.search(first_line)
        if not m:
            raise ValueError("No timestamp found in first line of log.")
        first_ts = float(m.group("ts"))

        # ----- last line -----
        f.seek(0, 2)  # seek to end of file
        pos = f.tell()
        # walk backwards until we hit a newline before the last line
        num_new_lines = 0
        while pos > 0:
            pos -= 1
            f.seek(pos)
            if f.read(1) == '\n':
                num_new_lines += 1
                if num_new_lines == 2:
                    break
        last_line = f.readline()

        m = ts_re.search(last_line)
        if not m:
            raise ValueError("No timestamp found in last line of log.")
        last_ts = float(m.group("ts"))

    return first_ts, last_ts

def parse_trace(file_path, start_percent=0, end_percent=100):
    """
    Fast NumPy-based reader. Returns numpy arrays:
    cycles, vas, cpus, ips, events
    """
    record_dtype = np.dtype([
        ('cycle', '<u8'),
        ('va',    '<u8'),
        ('ip',    '<u8'),
        ('cpu',   '<u4'),
        ('event', 'u1'),
    ])

    record_size = record_dtype.itemsize  # should be 29
    total_bytes = os.path.getsize(file_path)
    total_records = total_bytes // record_size
    if total_records == 0:
        print("Empty file.")
        sys.exit(1)

    start_record = int(total_records * start_percent / 100)
    end_record = int(total_records * end_percent / 100)
    count = max(0, end_record - start_record)
    if count == 0:
        print("No records in requested range.")
        sys.exit(1)

    with open(file_path, 'rb') as f:
        f.seek(start_record * record_size)
        arr = np.fromfile(f, dtype=record_dtype, count=count)

    if arr.size == 0 or arr['va'].size == 0:
        print("No addresses found in binary file.")
        sys.exit(1)

    return (arr['cycle'].astype(np.uint64),
            arr['va'].astype(np.uint64),
            arr['cpu'].astype(np.uint32),
            arr['ip'].astype(np.uint64),
            arr['event'].astype(np.uint8))

from collections import Counter
import numpy as np


def accuracy_sliding_window(cyc, va, p_cyc, p_va, window):
    # ensure arrays are numpy and sorted by cyc
    cyc = np.asarray(cyc)
    va = np.asarray(va, dtype=np.uint64)
    p_cyc = np.asarray(p_cyc)
    p_va = np.asarray(p_va, dtype=np.uint64)

    # sort predictions by timestamp while remembering order (optional)
    # idx = np.argsort(p_cyc)
    # sorted_p_cyc = p_cyc[idx]
    # sorted_p_va = p_va[idx]

    left = 0
    right = 0
    N = cyc.size
    counter = Counter()
    hits = 0
    misses = 0
    total = p_cyc.size

    num_loops = 0
    for ts, paddr in zip(p_cyc, p_va):
        if num_loops % 512 == 0:
            print(f"\r{num_loops / total * 100:.2f}%", end='')
        num_loops += 1
        # expand right to include events <= ts + window
        upper = ts + window
        while right < N and cyc[right] <= upper:
            counter[int(va[right])] += 1
            right += 1

        # shrink left to exclude events < ts (left bound is inclusive or exclusive depending on your semantics)
        # original code used searchsorted left index for start => exclude events with cyc < ts
        while left < N and cyc[left] < ts:
            addr = int(va[left])
            counter[addr] -= 1
            if counter[addr] == 0:
                del counter[addr]
            left += 1

        # membership check is O(1) average
        if int(paddr) in counter:
            hits += 1
        else:
            misses += 1
    print("\r", end='')
    accuracy = hits / total * 100.0 if total > 0 else 0.0
    return total, hits, misses, accuracy


def main():
    args = parse_args()

    start_time, end_time = parse_log(args.logfile)
    print("Total runtime:", end_time)

    cyc, va, _, _, _ = parse_trace(args.tracefile, 0, 100)
    p_cyc, p_va, _, _, _ = parse_trace(args.predfile, 0, 100)
    if args.coldfile != None:
        c_cyc, c_va, _, _, _ = parse_trace(args.coldfile, 0, 100)

    max_cyc = max(np.max(cyc), np.max(p_cyc))
    min_cyc = min(np.min(cyc), np.min(p_cyc))
    
    # Convert to seconds linearly
    cyc = (cyc - min_cyc) / (max_cyc - min_cyc)
    p_cyc = (p_cyc - min_cyc) / (max_cyc - min_cyc)

    if min_cyc == max_cyc:
        print("All prediction timestamps are identical; cannot map time range.")
        sys.exit(1)

    start = time.time()
    total, hits, misses, accuracy = accuracy_sliding_window(cyc, va, p_cyc, p_va, args.window)
    end = time.time()
    print("Finished in", end - start, "seconds")

    if args.coldfile != None:
        start = time.time()
        c_total, c_hits, c_misses, c_accuracy = accuracy_sliding_window(cyc, va, c_cyc, c_va, args.window)
        end = time.time()
        print("Finished in", end - start, "seconds")


    print("=== Hot Prediction accuracy results ===")
    print(f"Total predictions in log       : {total}")
    print(f"Hits                           : {hits}")
    print(f"Misses                         : {misses}")
    print(f"Accuracy                       : {accuracy:.2f}%")

    if args.coldfile != None:
        print("\n=== Cold Prediction accuracy results ===")
        print(f"Total predictions in log       : {c_total}")
        print(f"Hits                           : {c_misses}")
        print(f"Misses                         : {c_hits}")
        print(f"Accuracy                       : {100-c_accuracy:.2f}%")

if __name__ == "__main__":
    main()
