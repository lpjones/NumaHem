#!/usr/bin/env bash
# run_test.sh
# Usage:
#   ./run_test.sh              # runs the example at the bottom
#   OR source this file and call run_app "myconfig" "<full command line>"

PRELOAD="/proj/TppPlus/tpp/hemem-og/src/libhemem-llama.so"
LIB_PATH="/proj/TppPlus/tpp/hemem-og/src:/proj/TppPlus/tpp/hemem-og/Hoard/src"

CGUPS_DIR="../workloads/cgups"
MGUPS_DIR="../../scripts/my_gups"
HGUPS_DIR="../workloads/hgups"
GAPBS_DIR="../workloads/gapbs"
RESNET_DIR="../workloads/resnet"
STREAM_DIR="../workloads/stream"
YCSB_DIR="../workloads/YCSB"
PLOT_SCRIPTS_DIR="plot_scripts"

result_dir="results"

ORIG_PWD="$(pwd)"

# run once to pin/disable CPUs if you have such a helper
# if [ -x ./disable_cpus.sh ]; then
#   echo "Running disable_cpus.sh"
#   ./disable_cpus.sh
# else
#   echo "Warning: disable_cpus.sh not found or not executable; skipping."
# fi

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
  if [ $# -lt 3 ]; then
    echo "Usage: run_app <config_name> <dir> <command...>"
    return 2
  fi

  local config="$1"; shift
  local work_dir="$1"; shift
  # The rest of the arguments form the command to run. Respect spaces & quoting.
  local cmd=( "$@" )

  local app_dir="${ORIG_PWD}/${result_dir}/${config}"
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
  cd "${work_dir}"
  sudo numactl -N0 env LD_PRELOAD="${PRELOAD}" LD_LIBRARY_PATH="${LIB_PATH}" "${cmd[@]}" > "${stdout_file}" 2> "${stderr_file}"
  
  local rc=$?

  mv -f trace.bin "${app_dir}"
  mv -f stats-hem.txt "${app_dir}/stats.txt"
  mv -f debuglog-hem.txt "${app_dir}/debuglog.txt"
  mv -f times.txt "${app_dir}/time.txt"
  mv -f log-hem.txt "${app_dir}/log.txt"

  cd "${ORIG_PWD}"

  echo "Run finished (rc=${rc}). stdout -> ${stdout_file}, stderr -> ${stderr_file}"

  # Plots

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cgups_mul.py" "${app_dir}/app.txt" "${app_dir}/throughput.png"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" "${app_dir}/app.txt" "${app_dir}/gapbs_times.png"

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "dram_free" "dram_used" "dram_size" "dram_cap" -o "${app_dir}/dram_stats"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "dram_accesses" "rem_accesses" -o "${app_dir}/accesses"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "percent_dram" -o "${app_dir}/percent"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "internal_mem_overhead" -g2 "mem_allocated" -o "${app_dir}/mem"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "promotions" "demotions" -g2 "threshold" -o "${app_dir}/migrations"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "pebs_resets" -o "${app_dir}/resets"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "mig_move_time" -g2 "mig_queue_time" -o "${app_dir}/mig_time"
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "cold_pages" "hot_pages" -o "${app_dir}/pages"
  # ./venv/bin/python plot_pebs_mig.py --log-file "${app_dir}/debuglog.txt" --out "${app_dir}/mig_latency"

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" "${app_dir}/tmem_trace.bin" -fast
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" "${app_dir}/tmem_trace.bin" -fast -c cpu
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" "${app_dir}/trace.bin" -fast

  return ${rc}
}

echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

run_app "cgups-hem-orig" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60