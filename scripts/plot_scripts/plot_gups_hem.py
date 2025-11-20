import sys
import os
import matplotlib.pyplot as plt
import re

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
    print("Usage: python plot_throughput_and_migrations.py <config>")
    sys.exit(1)

config = sys.argv[1]

app_file = os.path.join(stats_path, config, "app.txt")
log_file = os.path.join(stats_path, config, "log.txt")

# Read files
throughput_data = read_throughput(app_file)
migration_data = read_total_migrations(log_file)

# Time axes
throughput_time = list(range(len(throughput_data)))
migration_time = list(range(len(migration_data)))

# Plot
fig, ax1 = plt.subplots(figsize=(10, 6))

# Throughput plot
color = 'tab:blue'
ax1.set_xlabel('Time (seconds)')
ax1.set_ylabel('Throughput (bytes/sec)', color=color)
ax1.plot(throughput_time, throughput_data, label='Throughput', color=color)
ax1.tick_params(axis='y', labelcolor=color)
ax1.grid(True)

# Migrations plot (secondary Y-axis)
ax2 = ax1.twinx()
color = 'tab:purple'
ax2.set_ylabel('Total Migrations', color=color)
ax2.plot(migration_time, migration_data, label='Total Migrations', color=color)
ax2.tick_params(axis='y', labelcolor=color)

plt.title(f'Throughput and Total Migrations Over Time: {config}')
fig.tight_layout()
output_file = os.path.join(stats_path, config, "throughput_migrations.png")
plt.savefig(output_file, dpi=300)
# print(f"Saved plot to {output_file}")
