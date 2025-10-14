#!/usr/bin/env bash
# run_test.sh
# Usage:
#   ./run_test.sh              # runs the example at the bottom
#   OR source this file and call run_app "myconfig" "<full command line>"

CGUPS_DIR="../../colloid/apps/gups"
MGUPS_DIR="../../scripts/my_gups"
HGUPS_DIR="../../hemem-og/microbenchmarks"
GAPS_DIR="../../gapbs"
RESNET_DIR="../../scripts"

result_dir="./results"

# run once to pin/disable CPUs if you have such a helper
if [ -x ./disable_cpus.sh ]; then
  echo "Running disable_cpus.sh"
  ./disable_cpus.sh
else
  echo "Warning: disable_cpus.sh not found or not executable; skipping."
fi

# Ensure result_dir exists
mkdir -p "${result_dir}"

# Normalize environment: a helper to prepare system before each run
_prepare_system() {
  # clear caches, turn off swap, disable automatic NUMA balancing, flush
  echo "Dropping caches, turning off swap, disabling numa balancing..."
  echo 3 > /proc/sys/vm/drop_caches || echo "Failed to drop caches (permission?)"
  swapoff -a || echo "swapoff failed or no swap configured"
  echo 0 > /proc/sys/kernel/numa_balancing || echo "Failed to set numa_balancing"
  sync
}

# run_app <config_name> <command...>
# Example: run_app "cgups" "${CGUPS_DIR}/gups64-rw 16 move 30 kill 60"
run_app() {
  if [ $# -lt 2 ]; then
    echo "Usage: run_app <config_name> <command...>"
    return 2
  fi

  local config="$1"; shift
  # The rest of the arguments form the command to run. Respect spaces & quoting.
  local cmd=( "$@" )

  local app_dir="${result_dir}/${config}"
  rm -rf "${app_dir}"
  mkdir -p "${app_dir}"

  echo
  echo "=== Running config='${config}' cmd='${cmd[*]}' ==="
  _prepare_system

  # Files inside app_dir
  local stdout_file="${app_dir}/app.txt"
  local stderr_file="${app_dir}/stderr.txt"

  # Run the command with LD_PRELOAD. Use bash -c to allow redirections in the command
  # but preferring not to lose quoting; we execute the array with LD_PRELOAD in environment.
  # Capture exit code so we can report it.
  # gdb --args env LD_PRELOAD="${PRELOAD}" "${cmd[@]}" #> "${stdout_file}" 2> "${stderr_file}"
  "${cmd[@]}" > "${stdout_file}" 2> "${stderr_file}"
  local rc=$?


  echo "Run finished (rc=${rc}). stdout -> ${stdout_file}, stderr -> ${stderr_file}"

  # Plots

  ./venv/bin/python plot_cgups.py "${app_dir}/app.txt" "${app_dir}/throughput.png"

  return ${rc}
}

### Example usage (uncomment or call from command line / source)
# Single run using the original cgups command:
# run_app "cgups" "${CGUPS_DIR}/gups64-rw" "16" "move" "30" "kill" "60"
#
# Alternatively, if you prefer to pass the whole command as a single string:
# run_app "cgups" bash -c "${CGUPS_DIR}/gups64-rw 16 move 30 kill 60"
#
# Example calling multiple runs:
run_app "cgups-reg-2GB" "${CGUPS_DIR}/gups64-rw" 8 move 150 kill 300
# run_app "mgups-reg" "${MGUPS_DIR}/gups" 16000 2000 8 60 30 16
# run_app "hgups-reg" "${HGUPS_DIR}/gups-hotset-move" 8 100000000 34 8 30
# run_app "bfs-reg" "${GAPS_DIR}/bfs" -f "${GAPS_DIR}/twitter-2010.sg" -n 8 -r 0
# run_app "resnet-reg" ./venv/bin/python "${RESNET_DIR}/resnet_train.py"