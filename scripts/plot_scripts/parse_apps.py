import os
import re

def parse_cgups(path):
    nums = []
    with open(path, 'r', errors='replace') as f:
        for line in f:
            # split by whitespace
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


def parse_gapbs(file_path):
    TRIAL_RE = re.compile(r'Trial Time:\s*([0-9]*\.?[0-9]+)')
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

def parse_resnet(filepath):
    """
    Return a sorted list of (epoch, images_per_sec) found in the file.
    If the same epoch appears multiple times, the last occurrence is used.
    """
    EPOCH_RE = re.compile(
    r"Epoch\s*[:\s]*\s*([0-9]+)\s*\s*[:]\s*([0-9]*\.?[0-9]+)\s*images/sec",
    flags=re.IGNORECASE,
)
    if not os.path.isfile(filepath):
        return []

    values = []
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
                    values.append(val)
                except Exception:
                    # ignore bad parses
                    pass

    # Return epochs sorted ascending
    return values

def parse_stream(file_path):
    ITER_RE = re.compile(r'Iter\s+\d+:\s*time\s*=\s*([0-9]*\.?[0-9]+)\s*seconds', re.IGNORECASE)
    """Return a list of float trial times from a text file."""
    vals: List[float] = []
    if not os.path.isfile(file_path):
        return vals
    with open(file_path, 'r', errors='ignore') as f:
        for line in f:
            m = ITER_RE.search(line)
            if m:
                try:
                    vals.append(float(m.group(1)))
                except ValueError:
                    pass
    return vals

def check_args(args):
    # Validate inputs exist
    for p in args.inputs:
        if not os.path.isfile(p):
            print(f"Input file '{p}' not found.", file=sys.stderr)
            return 1

    if args.labels == None:
        args.labels = [""] * len(args.inputs)

    if len(args.inputs) != len(args.labels):
        print(f"Input files not equal to the number of labels: {len(args.inputs)} != {len(args.labels)}")
        return 1