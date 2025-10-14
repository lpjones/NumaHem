#!/bin/bash

# This script disables all CPUs on NUMA node 1

# Check if running as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit 1
fi

echo "Disabling all CPUs on NUMA node 1..."

# Find all CPU directories for NUMA node 1
for cpu_path in /sys/devices/system/node/node1/cpu*; do
  cpu=$(basename "$cpu_path")
  cpu_num=${cpu#cpu}

  online_file="/sys/devices/system/cpu/${cpu}/online"

  # Check if the online file exists (not all CPUs support hotplug)
  if [ -f "$online_file" ]; then
    echo 0 > "$online_file" || echo "Failed to disable $cpu"
  fi
done
