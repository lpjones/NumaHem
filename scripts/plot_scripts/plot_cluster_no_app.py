import sys
import os
import re
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.colors as mcolors
import matplotlib.ticker as ticker
import matplotlib.style as mplstyle
import numpy as np
import warnings
import struct
import argparse
from pathlib import Path
from datetime import datetime

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent

# Shift axes to start at 0
try:
    x_vals = x_vals - np.min(x_vals)
    y_vals = y_vals - np.min(y_vals)
except:
    pass
plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

parser = argparse.ArgumentParser()
parser.add_argument("config", help="Name of config file (binary stats file)")
parser.add_argument("--output", help="Output image filename (e.g. out.png, out.pdf).")
parser.add_argument("-c", choices=["event", "cpu"], default="event", help="Color clusters by 'event' or 'cpu'")
parser.add_argument("-fast", action="store_true", help="Use heatmap mode (faster to render large datasets). Without this flag the script uses the scatter version.")
parser.add_argument("--title", default="", help="Plot title.")
parser.add_argument('--start-percent', help='Start percent of file to read (0-100)', type=float, default=0.0)
parser.add_argument('--end-percent', help='End percent of file to read (0-100)', type=float, default=100.0)

args = parser.parse_args()

config = args.config
color_by = args.c
use_heatmap = args.fast

warnings.filterwarnings("ignore", category=DeprecationWarning)

# Parameters
gap_threshold_gb = 2
min_accesses_per_cluster = 500
start_percent = args.start_percent
end_percent = args.end_percent
GB = 1024 ** 3


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


def parse_mem_np(stats_file, start_percent, end_percent):
    """
    Fast NumPy-based reader. Returns numpy arrays:
    cycles, vas, cpus, ips, events
    """
    # dtype that matches struct '<QQQIB' -> Q(8),Q(8),Q(8),I(4),B(1) == 29 bytes
    record_dtype = np.dtype([
        ('cycle', '<u8'),
        ('va',    '<u8'),
        ('ip',    '<u8'),
        ('cpu',   '<u4'),
        ('event', 'u1'),
    ])

    record_size = record_dtype.itemsize  # should be 29
    total_bytes = os.path.getsize(stats_file)
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

    # Read chunk using fromfile (fast); seek to offset then read `count` records
    with open(stats_file, 'rb') as f:
        f.seek(start_record * record_size)
        arr = np.fromfile(f, dtype=record_dtype, count=count)

    if arr.size == 0 or arr['va'].size == 0:
        print("No addresses found in binary file.")
        sys.exit(1)

    # return numpy arrays (keep them as arrays for downstream performance)
    return arr['cycle'].astype(np.uint64), arr['va'].astype(np.uint64), arr['cpu'].astype(np.uint32), arr['ip'].astype(np.uint64), arr['event'].astype(np.uint8)


def infer_clusters_np(addresses, gap_threshold_gb):
    """
    Vectorized cluster inference: find contiguous address groups separated
    by gaps >= gap_threshold_gb.
    Returns list of (start, end) cluster tuples (Python ints).
    """
    if addresses.size == 0:
        return []

    # unique sorted addresses
    uniq = np.unique(addresses)
    if uniq.size == 0:
        return []

    gap_threshold = np.uint64(gap_threshold_gb * GB)

    # compute diffs between successive unique addresses
    diffs = np.diff(uniq)
    # where diff >= gap_threshold => cut between uniq[i] and uniq[i+1]
    cut_indices = np.nonzero(diffs >= gap_threshold)[0]

    # cluster starts and ends (indices into uniq)
    starts_idx = np.concatenate(([0], cut_indices + 1))
    ends_idx = np.concatenate((cut_indices, [uniq.size - 1]))

    starts = uniq[starts_idx]
    ends = uniq[ends_idx]

    clusters = [(int(s), int(e)) for s, e in zip(starts, ends)]
    return clusters


def cluster_mem_np(cycles, addrs, cpus, ips, events, clusters):
    """
    Vectorized grouping of samples into clusters.
    Returns lists of numpy arrays per cluster:
      cluster_cycles_list, cluster_addrs_list, cluster_cpus_list, cluster_ips_list, cluster_events_list
    The order of clusters is the same as `clusters`.
    """
    if len(clusters) == 0:
        return [], [], [], [], []

    starts = np.array([c[0] for c in clusters], dtype=np.uint64)
    ends = np.array([c[1] for c in clusters], dtype=np.uint64)

    # For each sample address, determine cluster index:
    # idx = rightmost start <= addr  -> searchsorted(right) - 1
    idxs = np.searchsorted(starts, addrs, side='right') - 1

    # invalid idxs (addr < first start) will be -1; also ensure addr <= end[idx]
    valid_mask = (idxs >= 0) & (addrs <= ends[idxs])
    # Mark invalid samples with -1 so they won't match any cluster
    idxs_validated = np.where(valid_mask, idxs, -1)

    cluster_cycles_list = []
    cluster_addrs_list = []
    cluster_cpus_list = []
    cluster_ips_list = []
    cluster_events_list = []

    # For each cluster index k, select samples
    for k in range(len(clusters)):
        sel = (idxs_validated == k)
        cluster_cycles_list.append(cycles[sel])
        cluster_addrs_list.append(addrs[sel])
        cluster_cpus_list.append(cpus[sel])
        cluster_ips_list.append(ips[sel])
        cluster_events_list.append(events[sel])

    return cluster_cycles_list, cluster_addrs_list, cluster_cpus_list, cluster_ips_list, cluster_events_list


def plot_clusters(cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events, clusters, tot_addrs, config, color_by, use_heatmap=False, start_ts=None, end_ts=None):
    global args
    event_colors = {
        0: ('Local Access', 'tab:blue'),
        1: ('Remote Access',  'tab:orange'),
    }

    out_dir = os.path.dirname(config) or '.'
    basename = os.path.splitext(os.path.basename(config))[0]

    for idx in range(len(clusters)):
        cycles = np.asarray(cluster_cycles[idx]).astype(np.float64)
        addrs = np.asarray(cluster_addresses[idx]).astype(np.float64)
        events = np.asarray(cluster_events[idx])
        cpus = np.asarray(cluster_cpus[idx])
        count = addrs.size

        percent = 100 * count / tot_addrs
        cluster_start, cluster_end = clusters[idx]

        if count == 0:
            print(f"Cluster {idx} has no samples, skipping.")
            continue

        mplstyle.use('fast')

        # map cycles to time using provided start_ts/end_ts if available
        if start_ts is not None and end_ts is not None:
            cmin = cycles.min()
            cmax = cycles.max()
            if cmax == cmin:
                sys.exit(1)
            times = start_ts + (cycles - cmin) * (end_ts - start_ts) / (cmax - cmin)
        else:
            # fallback: relative cycles in seconds (not real seconds)
            times = cycles - cycles.min()

        # convert addresses to GB with bottom at 0
        addrs_gb = (addrs - np.min(addrs)) / GB
        y_max_gb = addrs_gb.max() if addrs_gb.size > 0 else 0

        if not use_heatmap:
            # Scatter mode (original)
            plt.figure(figsize=(10, 6))

            if color_by == "event":
                for evt_type, (label, color) in event_colors.items():
                    mask = (events == evt_type)
                    if not np.any(mask):
                        continue
                    plt.scatter(times[mask], addrs_gb[mask], s=3, color=color, label=label, alpha=0.6, edgecolors='none')
            elif color_by == "cpu":
                unique_cpus = np.unique(cpus)
                if unique_cpus.size == 0:
                    print(f"Cluster {idx} has no CPU samples (skipping).")
                    continue
                cmap = cm.get_cmap('tab20', max(1, unique_cpus.size))
                cpu_to_color = {cpu: cmap(i % cmap.N) for i, cpu in enumerate(unique_cpus)}

                for cpu in unique_cpus:
                    mask = (cpus == cpu)
                    plt.scatter(times[mask], addrs_gb[mask], s=1, color=cpu_to_color[cpu], label=f'CPU {cpu}', alpha=0.6, edgecolors='none')
            else:
                print(f"Unknown color_by option: {color_by}")
                return

            plt.xlabel("Time (s)")
            plt.ylabel("Virtual Address (GB)")
            if args.title == "":
                plt.title(f"Alloc Cluster {idx}: 0x{cluster_start:x} - 0x{cluster_end:x} ({count} accesses, {percent:.2f}%)")
            else:
                plt.title(args.title)

            # format x-axis as human-readable timestamps if we have absolute times
            if start_ts is not None and end_ts is not None:
                def fmt_ts(x, _):
                    return f"{x:.0f}"
                plt.gca().xaxis.set_major_formatter(ticker.FuncFormatter(fmt_ts))

            plt.gca().yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f"{x:.1f}"))
            plt.ylim(0, max(y_max_gb * 1.05, 1.0))
            plt.legend(loc='upper right', markerscale=2.0)

            if args.output != None:
                base = os.path.splitext(args.output)[0]
                output_file = f"{base}-{idx}"
            else:
                output_file = os.path.join(out_dir, f"{basename}-{idx}-{color_by}-scatter.png")

            plt.tight_layout()
            plt.savefig(output_file, dpi=300)
            plt.close()
            print(f"Saved cluster scatter plot to {output_file}")

        else:
            # Heatmap mode (vectorized histograms) with fixed shared bin edges
            mplstyle.use('fast')
            plt.figure(figsize=(10, 6))

            bins_x = 300
            bins_y = 300

            # Build common edges for this cluster so all histograms align
            cmin, cmax = float(times.min()), float(times.max())
            amin, amax = float(addrs_gb.min()), float(addrs_gb.max())
            if cmin == cmax:
                cmin -= 0.5
                cmax += 0.5
            if amin == amax:
                amin -= 0.5
                amax += 0.5

            # Use linspace edges (bins+1 points) to force consistent bins
            xedges = np.linspace(cmin, cmax, bins_x + 1)
            yedges = np.linspace(amin, amax, bins_y + 1)
            bins_edges = [xedges, yedges]

            if color_by == "event":
                Hs = []
                sorted_evt_keys = sorted(event_colors.keys())
                for evt_type in sorted_evt_keys:
                    mask = (events == evt_type)
                    if not np.any(mask):
                        Hs.append(np.zeros((bins_x, bins_y), dtype=np.float32))
                        continue
                    H, _, _ = np.histogram2d(times[mask], addrs_gb[mask], bins=bins_edges)
                    Hs.append(H)

                stack = np.stack(Hs, axis=0)  # (n_events, bins_x, bins_y)
                total_counts = np.sum(stack, axis=0)
                argmax_idx = np.argmax(stack, axis=0)

                colors_array = np.array([mcolors.to_rgb(event_colors[k][1]) for k in sorted_evt_keys])
                img = colors_array[argmax_idx]          # (bins_x, bins_y, 3)
                img = img.transpose(1, 0, 2)            # -> (bins_y, bins_x, 3)

                zero_mask = (total_counts == 0).T
                white = np.array([1.0, 1.0, 1.0])
                img[zero_mask] = white

                extent = [xedges[0], xedges[-1], yedges[0], yedges[-1]]
                plt.imshow(img, origin='lower', extent=extent, aspect='auto', interpolation='nearest')
                plt.xlabel("Time (s)")
                plt.ylabel("Virtual Address (GB)")
                if args.title == "":
                    plt.title(f"Alloc Cluster {idx}: 0x{cluster_start:x} - 0x{cluster_end:x} ({count} accesses, {percent:.2f}%)")
                else:
                    plt.title(args.title)

                if start_ts is not None and end_ts is not None:
                    def fmt_ts(x, _):
                        return f"{x:.0f}"
                    plt.gca().xaxis.set_major_formatter(ticker.FuncFormatter(fmt_ts))

                plt.gca().yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f"{x:.1f}"))
                plt.grid(False)
                plt.tight_layout()

                handles = [plt.Line2D([0], [0], marker='s', color='w', markerfacecolor=event_colors[k][1], markersize=8, label=event_colors[k][0]) for k in sorted_evt_keys]
                plt.legend(handles=handles, loc='upper right', markerscale=2.0)

            elif color_by == "cpu":
                unique_cpus = np.unique(cpus)
                if unique_cpus.size == 0:
                    print(f"Cluster {idx} has no CPU samples (skipping).")
                    continue

                # color palette for unique CPUs
                if unique_cpus.size <= 20:
                    cmap = cm.get_cmap('tab20', unique_cpus.size)
                    colors_list = [cmap(i)[:3] for i in range(unique_cpus.size)]
                else:
                    cmap = cm.get_cmap('turbo', unique_cpus.size)
                    colors_list = [cmap(i)[:3] for i in range(unique_cpus.size)]

                Hs = []
                for cpu in unique_cpus:
                    mask = (cpus == cpu)
                    if not np.any(mask):
                        Hs.append(np.zeros((bins_x, bins_y), dtype=np.float32))
                        continue
                    H, _, _ = np.histogram2d(times[mask], addrs_gb[mask], bins=bins_edges)
                    Hs.append(H)

                stack = np.stack(Hs, axis=0)
                total_counts = np.sum(stack, axis=0)
                argmax_idx = np.argmax(stack, axis=0)

                colors_array = np.array(colors_list)
                img = colors_array[argmax_idx]   # (bins_x, bins_y, 3)
                img = img.transpose(1, 0, 2)

                zero_mask = (total_counts == 0).T
                white = np.array([1.0, 1.0, 1.0])
                img[zero_mask] = white

                extent = [xedges[0], xedges[-1], yedges[0], yedges[-1]]
                plt.imshow(img, origin='lower', extent=extent, aspect='auto', interpolation='nearest')
                plt.xlabel("Time (s)")
                plt.ylabel("Virtual Address (GB)")
                plt.title(f"Alloc Cluster {idx}: 0x{cluster_start:x} - 0x{cluster_end:x} ({count} accesses, {percent:.2f}%)")

                if start_ts is not None and end_ts is not None:
                    def fmt_ts(x, _):
                        return f"{x:.0f}"
                    plt.gca().xaxis.set_major_formatter(ticker.FuncFormatter(fmt_ts))

                plt.gca().yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f"{x:.1f}"))
                plt.grid(False)
                plt.tight_layout()

                # legend (limit to first 50 cpus)
                max_legend = min(unique_cpus.size, 50)
                handles = []
                for i in range(max_legend):
                    cpu = unique_cpus[i]
                    handles.append(plt.Line2D([0], [0], marker='s', color='w', markerfacecolor=colors_list[i], markersize=6, label=f'CPU {cpu}'))
                plt.legend(handles=handles, loc='upper right', markerscale=2.0)

            else:
                print(f"Unknown color_by option: {color_by}")
                return

            if args.output != None:
                base = os.path.splitext(args.output)[0]
                output_file = f"{base}-{idx}"
            else:
                output_file = os.path.join(out_dir, f"{basename}-{idx}-{color_by}-heatmap.png")

            plt.savefig(output_file, dpi=300)
            plt.close()
            print(f"Saved cluster heatmap to {output_file}")



def print_clusters(clusters):
    print(f"Found {len(clusters)} allocation clusters (gap ≥ {gap_threshold_gb} GB)")
    for idx, (start, end) in enumerate(clusters):
        print(f"Cluster {idx}: {(end - start) / GB:.4f} GB   {hex(start)} - {hex(end)}")


def print_mem(raw_addresses, cluster_addresses):
    total = raw_addresses.size if isinstance(raw_addresses, np.ndarray) else len(raw_addresses)
    covered = sum(c.size if isinstance(c, np.ndarray) else len(c) for c in cluster_addresses)
    for idx, c in enumerate(cluster_addresses):
        size = c.size if isinstance(c, np.ndarray) else len(c)
        print(f"Cluster {idx}: {size} ({size / total * 100:.2f}%)")
    print(f"Total in clusters: {covered / total * 100:.2f}%")


if __name__ == "__main__":
    raw_cycles, raw_addresses, raw_cpus, raw_ips, raw_events = parse_mem_np(config, start_percent, end_percent)

    clusters = infer_clusters_np(raw_addresses, gap_threshold_gb)

    cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events = cluster_mem_np(
        raw_cycles, raw_addresses, raw_cpus, raw_ips, raw_events, clusters
    )

    # Filter out clusters with too few accesses (vectorized counting)
    counts = np.array([a.size for a in cluster_addresses], dtype=int)
    keep_mask = counts >= min_accesses_per_cluster
    if not np.any(keep_mask):
        print("No clusters passed the access count threshold.")
        sys.exit(1)

    # Apply filter and reindex lists
    cluster_cycles = [cluster_cycles[i] for i in range(len(cluster_cycles)) if keep_mask[i]]
    cluster_addresses = [cluster_addresses[i] for i in range(len(cluster_addresses)) if keep_mask[i]]
    cluster_cpus = [cluster_cpus[i] for i in range(len(cluster_cpus)) if keep_mask[i]]
    cluster_ips = [cluster_ips[i] for i in range(len(cluster_ips)) if keep_mask[i]]
    cluster_events = [cluster_events[i] for i in range(len(cluster_events)) if keep_mask[i]]
    clusters = [clusters[i] for i in range(len(clusters)) if keep_mask[i]]

    print_clusters(clusters)

    # determine debuglog path in same folder as config
    config_path = Path(config)
    out_dir = str(config_path.parent) if config_path.parent != Path('.') else '.'
    debuglog_path = os.path.join(out_dir, 'debuglog.txt')

    start_ts = None
    end_ts = None
    if os.path.exists(debuglog_path):
        try:
            orig_start, orig_end = parse_log(debuglog_path)
            full_range = orig_end - orig_start
            start_ts = orig_start + full_range * (start_percent / 100)
            end_ts   = orig_start + full_range * (end_percent / 100)
            print(f"start={start_ts}, end={end_ts}")
        except Exception as e:
            print(f"Warning: failed to parse debuglog '{debuglog_path}': {e}. Falling back to cycle-relative X axis.")
    else:
        print(f"No debuglog found at {debuglog_path}. Using cycle-relative X axis.")

    plot_clusters(cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events, clusters, raw_addresses.size, config, color_by, use_heatmap=use_heatmap, start_ts=start_ts, end_ts=end_ts)

    print_mem(raw_addresses, cluster_addresses)
