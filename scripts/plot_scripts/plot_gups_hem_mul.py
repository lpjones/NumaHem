import sys
import os
import matplotlib.pyplot as plt
import re
from pathlib import Path

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent

plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

stats_path = './results'

def read_throughput(filepath):
    if not os.path.exists(filepath):
        return []
    with open(filepath, 'r') as f:
        return [int(line.strip()) for line in f if line.strip().isdigit()]

def read_total_migrations(filepath):
    if not os.path.exists(filepath):
        return []
    migrations = []
    pattern = re.compile(r"migrations:\s+(\d+)")
    with open(filepath, 'r') as f:
        for line in f:
            match = pattern.search(line)
            if match:
                migrations.append(int(match.group(1)))
    return migrations

if len(sys.argv) < 2:
    print("Usage: python plot_throughput_and_migrations.py <config1> [<config2> ...]")
    sys.exit(1)

configs = sys.argv[1:]

# Plot setup
fig, ax1 = plt.subplots(figsize=(10, 6))
ax1.set_xlabel('Time (seconds)')
ax1.set_ylabel('Throughput (bytes/sec)', color='tab:blue')
ax1.grid(True)

ax2 = ax1.twinx()
ax2.set_ylabel('Total Migrations', color='tab:purple')

# Line styles so each config is distinct
styles = ['-', '--', '-.', ':']
style_idx = 0

for config in configs:
    app_file = os.path.join(stats_path, config, "app.txt")
    log_file = os.path.join(stats_path, config, "log.txt")

    # Read data
    throughput_data = read_throughput(app_file)
    migration_data = read_total_migrations(log_file)

    throughput_time = list(range(len(throughput_data)))
    migration_time = list(range(len(migration_data)))

    style = styles[style_idx % len(styles)]
    style_idx += 1

    # Throughput curve
    ax1.plot(
        throughput_time,
        throughput_data,
        linestyle=style,
        color='tab:blue',
        label=f'Throughput ({config})'
    )

    # Migration curve
    ax2.plot(
        migration_time,
        migration_data,
        linestyle=style,
        color='tab:purple',
        label=f'Migrations ({config})'
    )

plt.title("Throughput and Total Migrations Over Time")

# Combined legend
lines1, labels1 = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
plt.legend(lines1 + lines2, labels1 + labels2, fontsize="small", loc="upper left")

fig.tight_layout()

# Always save to a top-level file, not per-config
output_file = "./results/throughput_migrations_multi.png"
plt.savefig(output_file, dpi=300)
# print(f"Saved plot to {output_file}")
