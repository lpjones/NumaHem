#!/usr/bin/env python3
"""
plot_cgups_numbers.py

Usage:
    python3 plot_cgups_numbers.py <input_file> <output_image>

Requirements:
    - input filename must contain the substring "cgups" (script will exit otherwise)
    - matplotlib installed (pip install matplotlib)
"""

import sys
import os
import argparse

def parse_args():
    p = argparse.ArgumentParser(description="Extract decimal numbers from a file (skip hex 0x...) and plot them over time.")
    p.add_argument("input", help="Input text file (must contain 'cgups' in its filename).")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
    return p.parse_args()

def extract_numbers_from_file(path):
    nums = []
    with open(path, 'r', errors='replace') as f:
        for line in f:
            # split by whitespace and punctuation-ish chars
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

def plot_numbers(nums, outpath):
    import matplotlib.pyplot as plt
    if not nums:
        print("No decimal numbers found in the input file.", file=sys.stderr)
        return 2
    x = list(range(len(nums)))
    y = nums

    plt.figure(figsize=(10,4))
    plt.plot(x, y)
    plt.xlabel("Seconds")
    plt.ylabel("Throughput (bytes)")
    plt.title("Colloid GUPS Throughput")
    plt.grid(True)
    plt.tight_layout()

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

    if "cgups" not in args.input.lower():
        print("Input filename does not contain 'cgups' — aborting.", file=sys.stderr)
        return 3

    if not os.path.isfile(args.input):
        print(f"Input file '{args.input}' not found.", file=sys.stderr)
        return 4

    nums = extract_numbers_from_file(args.input)
    return_code = plot_numbers(nums, args.output)
    return return_code

if __name__ == "__main__":
    sys.exit(main())
