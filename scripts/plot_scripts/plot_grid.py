#!/usr/bin/env python3
import os
import re
import statistics
from collections import defaultdict, OrderedDict

results_dir = "./results/grid_runs"
IMGS_RE = re.compile(r'Epoch\s+\d+\s*:\s*([0-9]*\.?[0-9]+)\s*images/sec', re.IGNORECASE)

max_im_sec = 0
max_path = ""

def parse_app_txt(path):
    global max_im_sec
    global max_path
    """Return average images/sec found in app.txt, or None if not found."""
    if not os.path.isfile(path):
        return None
    vals = []
    with open(path, 'r', errors='ignore') as f:
        for line in f:
            mo = IMGS_RE.search(line)
            if mo:
                try:
                    val = float(mo.group(1))
                    vals.append(val)
                    if val > max_im_sec:
                        max_im_sec = val
                        max_path = path

                except ValueError:
                    pass
    if not vals:
        return None
    return statistics.mean(vals)

# Parse knobs from run directory name.
# This expects tokens like: pebs0-clust1-hem0-his8-pred16-down0.0001-up0.01
KNOB_RE = re.compile(r'(pebs|clust|hem|his|pred|down|up|pg)([^-_/]+)', re.IGNORECASE)

def to_number(s):
    """Convert string to int or float if possible, otherwise return original string."""
    s = s.strip()
    if s == '':
        return s
    try:
        i = int(s)
        return i
    except Exception:
        pass
    try:
        f = float(s)
        return f
    except Exception:
        pass
    return s

def parse_runname(runname):
    """Return dict of knobs found in runname, e.g. {'pebs':0,'clust':1,...}"""
    found = KNOB_RE.findall(runname)
    if not found:
        return {}
    d = {}
    for k, v in found:
        d[k.lower()] = to_number(v)
    return d

# Collect data
resnet_dirs = [d for d in os.listdir(results_dir) if "resnet-stats" in d]
data = []  # list of dicts: {'run':name, 'knobs':{...}, 'perf':float}
for rd in sorted(resnet_dirs):
    run_path = os.path.join(results_dir, rd)
    app_path = os.path.join(run_path, "app.txt")
    perf = parse_app_txt(app_path)
    if perf is None:
        # skip runs without perf data
        continue
    knobs = parse_runname(rd)
    if not knobs:
        # skip if we couldn't parse knobs
        continue
    data.append({'run': rd, 'knobs': knobs, 'perf': perf})

if not data:
    print("No valid runs with perf data found under", results_dir)
    raise SystemExit(1)

# Determine all knob keys observed
all_knobs = sorted({k for entry in data for k in entry['knobs'].keys()})

print("Found knobs:", all_knobs)
print("Total parsed runs:", len(data))
print()

# For every knob, group runs by the other-knobs and show perf per value of the chosen knob
for chosen in all_knobs:
    print("="*80)
    print(f"Results for knob: '{chosen}' (x-axis). Fixing all other knobs.")
    print("-"*80)
    # grouping: key = tuple(sorted((other_k, other_v))) of other knobs
    groups = defaultdict(list)
    for entry in data:
        knobs = entry['knobs']
        if chosen not in knobs:
            continue
        other = tuple(sorted((k, knobs[k]) for k in knobs if k != chosen))
        groups[other].append((knobs[chosen], entry['perf'], entry['run']))

    if not groups:
        print(f"No data for knob '{chosen}'\n")
        continue

    # For each group (fixed other knobs), print a small table
    for other_key, points in sorted(groups.items(), key=lambda kv: kv[0]):
        # Build a human readable label for the fixed knobs
        if other_key:
            fixed_label = ", ".join(f"{k}={v}" for k, v in other_key)
        else:
            fixed_label = "(no other knobs)"
        print(f"Fixed: {fixed_label}")

        # aggregate perf by chosen-knob-value
        agg = defaultdict(list)
        for x_val, perf, run in points:
            agg[x_val].append(perf)

        # print header
        print(f"{chosen:>12} | {'mean images/sec':>16} | count | runs")
        print("-"*70)
        for x in sorted(agg.keys(), key=lambda v: (isinstance(v, str), v)):
            vals = agg[x]
            mean_perf = statistics.mean(vals)
            print(f"{str(x):>12} | {mean_perf:16.3f} | {len(vals):5d} | ", end="")
            # list up to 4 run names for quick identification
            runs_for_x = [r for (xx, p, r) in points if xx == x]
            # print(runs_for_x)
            print( (", ..." if len(runs_for_x) > 4 else ""))
        print()

print("Done.")
print(max_path, max_im_sec)
