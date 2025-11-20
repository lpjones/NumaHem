import sys
import os
import argparse
import re

def parse_args():
    p = argparse.ArgumentParser(description="Extract decimal numbers from a file (skip hex 0x...) and plot them over time.")
    p.add_argument("input", help="Input text file (must contain 'bfs' in its filename).")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
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

def plot_numbers(nums, outpath):
    import matplotlib.pyplot as plt
    if not nums:
        print("No decimal numbers found in the input file.", file=sys.stderr)
        return 2
    x = list(range(len(nums)))
    y = nums

    plt.figure(figsize=(10,4))
    plt.plot(x, y)
    plt.xlabel("Trial")
    plt.ylabel("Trial Time (s)")
    plt.title("GAPBS trial times")
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

    if "bfs" not in args.input.lower():
        print("Input filename does not contain 'bfs' — aborting.", file=sys.stderr)
        return 3

    if not os.path.isfile(args.input):
        print(f"Input file '{args.input}' not found.", file=sys.stderr)
        return 4

    nums = parse_trial_times(args.input)
    return_code = plot_numbers(nums, args.output)
    return return_code

if __name__ == "__main__":
    sys.exit(main())
