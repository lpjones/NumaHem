#!/usr/bin/env bash
# run_test.sh
# Usage:
#   ./run_test.sh              # runs the example at the bottom
#   OR source this file and call run_app "myconfig" "<full command line>"

PRELOAD="/proj/TppPlus/tpp/libnuma_pgmig/src/libtmem.so"
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

# run_app <config_name> <command...>
# Example: run_app "cgups" "${CGUPS_DIR}/gups64-rw 16 move 30 kill 60"
run_app() {
  # if [ $# -lt 3 ]; then
  #   echo "Usage: run_app <config_name> <title> <output dir>"
  #   return 2
  # fi

  local config="$1"; shift
  local title="$1"; shift
  local out_dir="$1"; shift
  # The rest of the arguments form the command to run. Respect spaces & quoting.

  local app_dir="${ORIG_PWD}/${result_dir}/${config}"

  # Plots

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cgups_mul.py" "${app_dir}/app.txt" "${app_dir}/throughput.png"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" "${app_dir}/app.txt" "${app_dir}/gapbs_times.png"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
  #   -f "${app_dir}/stats.txt" \
  #   -g1 "dram_free" "dram_used" "dram_size" "dram_cap" \
  #   --labels "" \
  #   -o "${app_dir}/dram_stats"

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/stats.txt" \
    -g1 "dram_accesses" "rem_accesses" \
    --labels "" \
    -o "${app_dir}/accesses"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
  #   -f "${app_dir}/stats.txt" \
  #   -g1 "percent_dram" \
  #   --labels "" \
  #   -o "${app_dir}/percent"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
  #   -f "${app_dir}/stats.txt" \
  #   -g1 "internal_mem_overhead" \
  #   -g2 "mem_allocated" \
  #   --labels "" \
  #   -o "${app_dir}/mem"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
  #   -f "${app_dir}/stats.txt" \
  #   -g1 "promotions" "demotions" \
  #   -g2 "threshold" \
  #   --labels "" \
  #   -o "${app_dir}/migrations"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
  #   -f "${app_dir}/stats.txt" \
  #   -g1 "pebs_resets" \
  #   --labels "" \
  #   -o "${app_dir}/resets"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
  #   -f "${app_dir}/stats.txt" \
  #   -g1 "mig_move_time" \
  #   -g2 "mig_queue_time" \
  #   --labels "" \
  #   -o "${app_dir}/mig_time"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
  #   -f "${app_dir}/stats.txt" \
  #   -g1 "cold_pages" "hot_pages" \
  #   --labels "" \
  #   -o "${app_dir}/pages"

  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
  #   "${app_dir}/tmem_trace.bin" \
  #   --title "Memory Trace" \
  #   --output "${app_dir}/trace.png" \
  #   --start-percent 50 \
  #   --end-percent 55 \
  #   -fast
  
  
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" "${app_dir}/tmem_trace.bin" -fast -c cpu

  return ${rc}
}

# run_app bfs-hem-2GB-100
run_app resnet-PAGR-2GB-100
run_app resnet-hem-2GB-100

# % modify plots to start y-axis at 0
# % add references to end of background bibtex
# % go through related work and summarize beyond hotness related section
# % move hemem vs local up to background
# % use lru for demotion in design

echo 3 > /proc/sys/vm/drop_caches
sync