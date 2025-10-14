import sys
import os
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.colors as mcolors
import matplotlib.ticker as ticker
import numpy as np
import re
import warnings
import struct
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("config", help="Name of config file (without extension)")
parser.add_argument("-c", choices=["event", "cpu"], default="event", help="Color clusters by 'event' or 'cpu'")
args = parser.parse_args()

config = args.config
color_by = args.c

warnings.filterwarnings("ignore", category=DeprecationWarning)

# Parameters
gap_threshold_gb = 2
min_accesses_per_cluster = 500
start_percent = 0
end_percent = 100


def parse_mem(stats_file, start_percent, end_percent):
    raw_cycles, raw_addresses, raw_cpus, raw_ips, raw_events = [], [], [], [], []
    record_format = '<QQQIB'  # cycle, va, ip, cpu, event
    record_size = struct.calcsize(record_format)

    with open(stats_file, 'rb') as f:
        f.seek(0, 2)
        total_bytes = f.tell()
        total_records = total_bytes // record_size

        start_record = int(total_records * start_percent / 100)
        end_record = int(total_records * end_percent / 100)

        f.seek(start_record * record_size)

        for _ in range(start_record, end_record):
            bytes_read = f.read(record_size)
            if len(bytes_read) != record_size:
                break

            cycle, va, ip, cpu, event = struct.unpack(record_format, bytes_read)
            raw_cycles.append(cycle)
            raw_cpus.append(cpu)
            raw_addresses.append(va)
            raw_ips.append(ip)
            raw_events.append(event)

    if not raw_addresses:
        print("No addresses found in binary file.")
        sys.exit(1)

    return raw_cycles, raw_addresses, raw_cpus, raw_ips, raw_events


def infer_clusters(addresses, gap_threshold_gb):
    addrs_sorted = sorted(set(addresses))
    gap_threshold = gap_threshold_gb * 1024 ** 3
    clusters = []

    cluster_start = addrs_sorted[0]
    for i in range(1, len(addrs_sorted)):
        if addrs_sorted[i] - addrs_sorted[i - 1] >= gap_threshold:
            clusters.append((cluster_start, addrs_sorted[i - 1]))
            cluster_start = addrs_sorted[i]
    clusters.append((cluster_start, addrs_sorted[-1]))

    return clusters


def cluster_mem(cycles, addrs, cpus, ips, events, clusters):
    cluster_addrs = [[] for _ in clusters]
    cluster_cpus = [[] for _ in clusters]
    cluster_ips = [[] for _ in clusters]
    cluster_events = [[] for _ in clusters]
    cluster_cycles = [[] for _ in clusters]

    for cycle, addr, cpu, ip, evt in zip(cycles, addrs, cpus, ips, events):
        for idx, (start, end) in enumerate(clusters):
            if start <= addr <= end:
                cluster_cycles[idx].append(cycle)
                cluster_addrs[idx].append(addr)
                cluster_cpus[idx].append(cpu)
                cluster_ips[idx].append(ip)
                cluster_events[idx].append(evt)
                break

    return cluster_cycles, cluster_addrs, cluster_cpus, cluster_ips, cluster_events


def plot_clusters(cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events, clusters, tot_addrs, config, color_by):
    event_colors = {
        0: ('DRAMREAD', 'tab:blue'),
        1: ('NVMREAD',  'tab:orange'),
        2: ('WRITE',    'tab:red'),
    }

    for idx in range(len(clusters)):
        cycles = np.array(cluster_cycles[idx])
        addrs = np.array(cluster_addresses[idx])
        events = np.array(cluster_events[idx])
        count = len(addrs)

        percent = 100 * count / tot_addrs
        cluster_start, cluster_end = clusters[idx]

        plt.figure(figsize=(10, 6))

        if color_by == "event":
            for evt_type, (label, color) in event_colors.items():
                mask = (events == evt_type)
                if not np.any(mask):
                    continue
                plt.scatter(cycles[mask], addrs[mask], s=2, color=color, label=label)
        elif color_by == "cpu":
            cpus = np.array(cluster_cpus[idx])
            unique_cpus = np.unique(cpus)
            cmap = cm.get_cmap('tab20', len(unique_cpus))
            cpu_to_color = {cpu: cmap(i) for i, cpu in enumerate(unique_cpus)}

            for cpu in unique_cpus:
                mask = (cpus == cpu)
                plt.scatter(cycles[mask], addrs[mask], s=2, color=cpu_to_color[cpu], label=f'CPU {cpu}')
        else:
            print(f"Unknown color_by option: {color_by}")
            return

        plt.xlabel("Cycle")
        plt.ylabel("Virtual Address")
        plt.title(f"Alloc Cluster {idx}: 0x{cluster_start:x} - 0x{cluster_end:x} ({count} accesses, {percent:.2f}%)")
        plt.gca().yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f'0x{int(x):x}'))
        plt.grid(True)
        plt.tight_layout()
        plt.legend(markerscale=3, fontsize=7)

        basename = os.path.splitext(os.path.basename(config))[0]
        output_file = os.path.join(os.path.dirname(config), f"{basename}-{idx}-{color_by}.png")
        plt.savefig(output_file, dpi=300)
        plt.close()
        print(f"Saved cluster plot to {output_file}")


def print_clusters(clusters):
    print(f"Found {len(clusters)} allocation clusters (gap ≥ {gap_threshold_gb} GB)")
    for idx, (start, end) in enumerate(clusters):
        print(f"Cluster {idx}: {(end - start) / 1024 ** 3:.4f} GB   {hex(start)} - {hex(end)}")


def print_mem(raw_addresses, cluster_addresses):
    total = len(raw_addresses)
    covered = sum(len(c) for c in cluster_addresses)
    for idx, c in enumerate(cluster_addresses):
        print(f"Cluster {idx}: {len(c)} ({len(c) / total * 100:.2f}%)")
    print(f"Total in clusters: {covered / total * 100:.2f}%")


if __name__ == "__main__":
    raw_cycles, raw_addresses, raw_cpus, raw_ips, raw_events = parse_mem(config, start_percent, end_percent)
    clusters = infer_clusters(raw_addresses, gap_threshold_gb)

    cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events = cluster_mem(
        raw_cycles, raw_addresses, raw_cpus, raw_ips, raw_events, clusters
    )

    # Filter out clusters with too few accesses
    filtered = [
        (cyc, addr, cpu, ip, evt, cl)
        for cyc, addr, cpu, ip, evt, cl in zip(
            cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events, clusters
        )
        if len(addr) >= min_accesses_per_cluster
    ]

    if not filtered:
        print("No clusters passed the access count threshold.")
        sys.exit(1)
    (cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events, clusters) = zip(*filtered)
    print_clusters(clusters)

    plot_clusters(cluster_cycles, cluster_addresses, cluster_cpus, cluster_ips, cluster_events, clusters, len(raw_addresses), config, color_by)
    print_mem(raw_addresses, cluster_addresses)
