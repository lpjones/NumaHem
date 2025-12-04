import struct
import numpy as np
import argparse
import os
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import shutil
from pathlib import Path

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent

plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

parser = argparse.ArgumentParser()
parser.add_argument("config", help="Name of config file (binary file)")
args = parser.parse_args()

config = args.config

# format
# 1 pebs record
# 8 neighbors
# 1 threshold

pebs_dtype = np.dtype([
    ('cycle', '<u8'),
    ('va',    '<u8'),
    ('ip',    '<u8'),
    ('cpu',   '<u4'),
    ('event', 'u1'),
])

neighbor_dtype = np.dtype([
    ('page', pebs_dtype),
    ('distance', '<f8'),
    ('time_diff', '<u8'),
])


pred_dtype = np.dtype([
    ('page', pebs_dtype),
    ('neighbors', neighbor_dtype, (2,)),
    ('threshold', '<f8'),
])


def print_pebs(p):
    """Print one or more pebs_dtype entries"""
    if p.ndim == 0:  # single record
        print(f"PEBS: cycle={p['cycle']}, va=0x{p['va']:x}, ip=0x{p['ip']:x}, cpu={p['cpu']}, event={p['event']}")
    else:  # array
        for i, rec in enumerate(p):
            print(f"[{i}] cycle={rec['cycle']}, va=0x{rec['va']:x}, ip=0x{rec['ip']:x}, cpu={rec['cpu']}, event={rec['event']}")


def print_neighbor(n):
    """Print one or more neighbor_dtype entries"""
    if n.ndim == 0:
        print("Neighbor:")
        print_pebs(n['page'])
        print(f"  distance={n['distance']:.4f}, time_diff={n['time_diff']}")
    else:
        for i, rec in enumerate(n):
            print(f"Neighbor[{i}]:")
            print_pebs(rec['page'])
            print(f"  distance={rec['distance']:.4f}, time_diff={rec['time_diff']}")


def print_pred(pred):
    """Print one or more pred_dtype entries"""
    if pred.ndim == 0:
        print("=== Prediction Record ===")
        print("Page predicting from:")
        print_pebs(pred['page'])
        print("Neighbors:")
        for i, n in enumerate(pred['neighbors']):
            print(f"  Neighbor[{i}]:")
            print_pebs(n['page'])
            print(f"    distance={n['distance']:.4f}, time_diff={n['time_diff']}")
        print(f"Threshold: {pred['threshold']:.6f}")
    else:
        for i, rec in enumerate(pred):
            print(f"\n=== Prediction {i} ===")
            print_pred(rec)  # reuse single-record version


def parse_preds(file_path):
    record_size = pred_dtype.itemsize
    print("record size:", record_size)
    total_bytes = os.path.getsize(file_path)
    total_records = total_bytes // record_size
    print("total records:", total_records)

    with open(file_path, "rb") as f:
        arr = np.fromfile(f, dtype=pred_dtype, count=total_records)

    return arr



# Plot ones that 
#   have at least 1 neighbor below threshold
def plot_preds(preds, out_dir):
    shutil.rmtree(out_dir, ignore_errors=True)
    os.makedirs(out_dir, exist_ok=True)
    count = 0

    for pred in preds:
        valid_neighbors = [n for n in pred['neighbors'] if n['page']['va'] != 0]
        neighbors_thresh = [n for n in valid_neighbors if n['distance'] < pred['threshold']]


        if len(neighbors_thresh) >= 1 and len(valid_neighbors) >= 1:
            main_cyc = int(pred['page']['cycle'])
            main_va = int(pred['page']['va'])
            thresh = float(pred['threshold'])

            neigh_cycles = []
            neigh_vas = []
            neigh_colors = []
            labels = []

            for n in valid_neighbors:
                cyc = int(n['page']['cycle'])
                va = int(n['page']['va'])
                dist = float(n['distance'])
                color = 'red' if (0 < dist < thresh) else 'blue'
                label = f"dist: {dist:.2e}"
                neigh_cycles.append(cyc)
                neigh_vas.append(va)
                neigh_colors.append(color)
                labels.append(label)
            
            plt.figure(figsize=(10,6))

            plt.scatter([main_cyc], [main_va], c='black', s=80, marker='x', label='page accessed', linewidths=2)
            plt.scatter(neigh_cycles, neigh_vas, c=neigh_colors, s=40, edgecolors=None, label='neighbors')

            for xi, yi, label in zip(neigh_cycles, neigh_vas, labels):
                plt.text(xi, yi + 10, label, ha='center', va='bottom', fontsize=7, color='black')

            plt.xlabel("Cycle")
            plt.ylabel("Virtual Address")
            plt.title(f"Prediction snapshot - Threshold={thresh:.2e}")
            plt.gca().yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f'0x{int(x):x}'))
            plt.grid(True)
            plt.tight_layout()
            plt.legend(markerscale=1, fontsize=8)

            num_neighbors = len(valid_neighbors)

            output_file = os.path.join(out_dir, f"record{count}_neigh{num_neighbors}.png")
            plt.savefig(output_file, dpi=300)
            plt.close()
            print(f"Saved cluster scatter plot to {output_file}")

        count += 1



if __name__ == "__main__":
    preds = parse_preds(config)

    out_dir = os.path.dirname(config) or '.'
    out_dir = os.path.join(out_dir, "pred_plots")

    plot_preds(preds, out_dir)
    