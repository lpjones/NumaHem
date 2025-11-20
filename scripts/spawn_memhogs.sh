#!/bin/bash

# Usage: ./spawn_memhogs.sh <num_instances>

if [ -z "$1" ]; then
    echo "Usage: $0 <num_memhogs>"
    exit 1
fi

N=$1
PIDS=()

cleanup() {
    echo ""
    echo "Cleaning up… killing all memhog processes"
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
        fi
    done
    wait
    echo "Done."
    exit 0
}

# Trap Ctrl-C, SIGTERM, script exit
trap cleanup INT TERM EXIT

for ((i=1; i<=N; i++)); do
    echo "Starting memhog instance $i..."
    numactl -N1 -m1 -- memhog -r100000000 100M >/dev/null 2>&1 &
    PIDS+=($!)
done

echo "Spawned $N memhog processes. Press Ctrl-C to stop them."

# Keep script running so trap works
wait
