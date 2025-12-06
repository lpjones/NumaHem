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
  if [ $# -lt 3 ]; then
    echo "Usage: run_app <config_name> <title> <output dir>"
    return 2
  fi

  local config="$1"; shift
  local title="$1"; shift
  local out_dir="$1"; shift
  # The rest of the arguments form the command to run. Respect spaces & quoting.

  local app_dir="${ORIG_PWD}/${result_dir}/${config}"

  # Plots

#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cgups_mul.py" "${app_dir}/app.txt" "${app_dir}/throughput.png"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" "${app_dir}/app.txt" "${app_dir}/gapbs_times.png"

#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "dram_free" "dram_used" "dram_size" "dram_cap" -o "${app_dir}/dram_stats"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "dram_accesses" "rem_accesses" -o "${app_dir}/accesses"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "percent_dram" -o "${app_dir}/percent"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "internal_mem_overhead" -g2 "mem_allocated" -o "${app_dir}/mem"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "promotions" "demotions" -g2 "threshold" -o "${app_dir}/migrations"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "pebs_resets" -o "${app_dir}/resets"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "mig_move_time" -g2 "mig_queue_time" -o "${app_dir}/mig_time"
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" -f "${app_dir}/stats.txt" -g1 "cold_pages" "hot_pages" -o "${app_dir}/pages"

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/tmem_trace.bin" \
    --title "${title} Memory Trace" \
    --output "${out_dir}/${config}-trace.png" \
    -fast
  
  
#   ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" "${app_dir}/tmem_trace.bin" -fast -c cpu

  return ${rc}
}

run_pagr_hem_loc() {
  local app=$1
  local app_dir="${result_dir}"

  echo "Comparison plots PAGR vs HeMem vs Local"
  # ResNet
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_resnet.py" \
    "${app_dir}/resnet-PAGR-${app}/app.txt" \
    "${app_dir}/resnet-hem-${app}/app.txt" \
    "${app_dir}/resnet-local-${app}/app.txt" \
    "${app_dir}/resnet-PAGR_HeMem_loc-${app}.png" \
    --labels "PAGR" "HeMem" "Local" \
    --title "ResNet50 Images/Sec"

  # CGUPS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cgups_mul.py" \
    "${app_dir}/cgups-PAGR-${app}/app.txt" \
    "${app_dir}/cgups-hem-${app}/app.txt" \
    "${app_dir}/cgups-local-${app}/app.txt" \
    "${app_dir}/cgups-PAGR_HeMem_loc-${app}.png" \
    --labels "PAGR" "HeMem" "Local" \
    --title "GUPS Throughput"

  # BFS (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bfs-PAGR-${app}/app.txt" \
    "${app_dir}/bfs-hem-${app}/app.txt" \
    "${app_dir}/bfs-local-${app}/app.txt" \
    "${app_dir}/bfs-PAGR_HeMem_loc-${app}.png" \
    --labels "PAGR" "HeMem" "Local" \
    --title "BFS Trial Times"

  # Stream
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stream_mul.py" \
    "${app_dir}/stream-PAGR-${app}/app.txt" \
    "${app_dir}/stream-hem-${app}/app.txt" \
    "${app_dir}/stream-local-${app}/app.txt" \
    "${app_dir}/stream-PAGR_HeMem_loc-${app}.png" \
    --labels "PAGR" "HeMem" "Local" \
    --title "Stream Trial Times"

  # BC (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bc-PAGR-${app}/app.txt" \
    "${app_dir}/bc-hem-${app}/app.txt" \
    "${app_dir}/bc-local-${app}/app.txt" \
    "${app_dir}/bc-PAGR_HeMem_loc-${app}.png" \
    --labels "PAGR" "HeMem" "Local" \
    --title "BC Trial Times"
}

run_pagr_hem() {
  local app=$1
  local app_dir="${result_dir}"

  echo "PAGR vs HeMem"
  # ResNet
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_resnet.py" \
    "${app_dir}/resnet-PAGR-${app}/app.txt" \
    "${app_dir}/resnet-hem-${app}/app.txt" \
    "${app_dir}/resnet-PAGR_HeMem-${app}.png" \
    --labels "PAGR" "HeMem" \
    --title "PAGR vs HeMem ResNet50 Images/Sec"

  # CGUPS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cgups_mul.py" \
    "${app_dir}/cgups-PAGR-${app}/app.txt" \
    "${app_dir}/cgups-hem-${app}/app.txt" \
    "${app_dir}/cgups-PAGR_HeMem-${app}.png" \
    --labels "PAGR" "HeMem" \
    --title "PAGR vs HeMem GUPS Throughput"

  # BFS (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bfs-PAGR-${app}/app.txt" \
    "${app_dir}/bfs-hem-${app}/app.txt" \
    "${app_dir}/bfs-PAGR_HeMem-${app}.png" \
    --labels "PAGR" "HeMem" \
    --title "PAGR vs HeMem BFS Trial Times"

  # Stream
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stream_mul.py" \
    "${app_dir}/stream-PAGR-${app}/app.txt" \
    "${app_dir}/stream-hem-${app}/app.txt" \
    "${app_dir}/stream-PAGR_HeMem-${app}.png" \
    --labels "PAGR" "HeMem" \
    --title "PAGR vs HeMem Stream Trial Times"

  # BC (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bc-PAGR-${app}/app.txt" \
    "${app_dir}/bc-hem-${app}/app.txt" \
    "${app_dir}/bc-PAGR_HeMem-${app}.png" \
    --labels "PAGR" "HeMem" \
    --title "PAGR vs HeMem BC Trial Times"
}

run_pagr_loc() {
  local app=$1
  local app_dir="${result_dir}"

  echo "PAGR VS local"
  # ResNet
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_resnet.py" \
    "${app_dir}/resnet-PAGR-${app}/app.txt" \
    "${app_dir}/resnet-local/app.txt" \
    "${app_dir}/resnet-PAGR_loc-${app}.png" \
    --labels "PAGR" "Local" \
    --title "PAGR vs Local ResNet50 Images/Sec"

  # CGUPS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cgups_mul.py" \
    "${app_dir}/cgups-PAGR-${app}/app.txt" \
    "${app_dir}/cgups-local/app.txt" \
    "${app_dir}/cgups-PAGR_loc-${app}.png" \
    --labels "PAGR" "Local" \
    --title "PAGR vs Local GUPS Throughput"

  # BFS (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bfs-PAGR-${app}/app.txt" \
    "${app_dir}/bfs-local/app.txt" \
    "${app_dir}/bfs-PAGR_loc-${app}.png" \
    --labels "PAGR" "Local" \
    --title "PAGR vs Local BFS Trial Times"

  # Stream
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stream_mul.py" \
    "${app_dir}/stream-PAGR-${app}/app.txt" \
    "${app_dir}/stream-local/app.txt" \
    "${app_dir}/stream-PAGR_loc-${app}.png" \
    --labels "PAGR" "Local" \
    --title "PAGR vs Local Stream Trial Times"

  # BC (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bc-PAGR-${app}/app.txt" \
    "${app_dir}/bc-local/app.txt" \
    "${app_dir}/bc-PAGR_loc-${app}.png" \
    --labels "PAGR" "Local" \
    --title "PAGR vs Local BC Trial Times"
}

run_stats_pagr_hem() {
  local app=$1
  local app_dir="${result_dir}"

  echo "Comparison stat plots PAGR vs HeMem"
  # Resnet
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/resnet-PAGR-${app}/stats.txt" \
    "${app_dir}/resnet-hem-${app}/stats.txt" \
    -g1 "percent_dram" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 10 \
    -o "${app_dir}/resnet-PAGR_HeMem_per-${app}.png"

  # BC
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/bc-PAGR-${app}/stats.txt" \
    "${app_dir}/bc-hem-${app}/stats.txt" \
    -g1 "percent_dram" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/bc-PAGR_HeMem_per-${app}.png"

  # BFS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/bfs-PAGR-${app}/stats.txt" \
    "${app_dir}/bfs-hem-${app}/stats.txt" \
    -g1 "percent_dram" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/bfs-PAGR_HeMem_per-${app}.png"
  
  # Stream
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/stream-PAGR-${app}/stats.txt" \
    "${app_dir}/stream-hem-${app}/stats.txt" \
    -g1 "percent_dram" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/stream-PAGR_HeMem_per-${app}.png"

  # GUPS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/cgups-PAGR-${app}/stats.txt" \
    "${app_dir}/cgups-hem-${app}/stats.txt" \
    -g1 "percent_dram" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/cgups-PAGR_HeMem_per-${app}.png"

  # Migrations
  # Resnet
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/resnet-PAGR-${app}/stats.txt" \
    "${app_dir}/resnet-hem-${app}/stats.txt" \
    -g1 "promotions" "demotions" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 10 \
    -o "${app_dir}/resnet-PAGR_HeMem_mig-${app}.png"

  # BC
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/bc-PAGR-${app}/stats.txt" \
    "${app_dir}/bc-hem-${app}/stats.txt" \
    -g1 "promotions" "demotions" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/bc-PAGR_HeMem_mig-${app}.png"

  # BFS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/bfs-PAGR-${app}/stats.txt" \
    "${app_dir}/bfs-hem-${app}/stats.txt" \
    -g1 "promotions" "demotions" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/bfs-PAGR_HeMem_mig-${app}.png"
  
  # Stream
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/stream-PAGR-${app}/stats.txt" \
    "${app_dir}/stream-hem-${app}/stats.txt" \
    -g1 "promotions" "demotions" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/stream-PAGR_HeMem_mig-${app}.png"

  # GUPS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stats_mul.py" \
    -f "${app_dir}/cgups-PAGR-${app}/stats.txt" \
    "${app_dir}/cgups-hem-${app}/stats.txt" \
    -g1 "promotions" "demotions" \
    --labels "PAGR" "HeMem" \
    --start-percent 0 \
    --end-percent 100 \
    -o "${app_dir}/cgups-PAGR_HeMem_mig-${app}.png"
}

run_trace() {
  local app=$1
  local app_dir="${result_dir}"
  
  echo "Memory Traces"
  # Local
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/bc-local-${app}/tmem_trace.bin" \
    --title "BC All Local Memory Trace" \
    --output "${app_dir}/bc-local-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/resnet-local-${app}/tmem_trace.bin" \
    --title "ResNet50 All Local Memory Trace" \
    --output "${app_dir}/resnet-local-trace.png" \
    --start-percent 0 \
    --end-percent 8 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/cgups-local-${app}/tmem_trace.bin" \
    --title "GUPS All Local Memory Trace" \
    --output "${app_dir}/cgups-local-trace.png" \
    --start-percent 0 \
    --end-percent 100 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/bfs-local-${app}/tmem_trace.bin" \
    --title "BFS All Local Memory Trace" \
    --output "${app_dir}/bfs-local-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/stream-local-${app}/tmem_trace.bin" \
    --title "Stream All Local Memory Trace" \
    --output "${app_dir}/stream-local-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  # Resnet
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/resnet-PAGR-${app}/tmem_trace.bin" \
    --title "ResNet50 PAGR Memory Trace" \
    --output "${app_dir}/resnet-PAGR-${app}-trace.png" \
    --start-percent 0 \
    --end-percent 8 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/resnet-hem-${app}/tmem_trace.bin" \
    --title "ResNet50 HeMem Memory Trace" \
    --output "${app_dir}/resnet-HeMem-${app}-trace.png" \
    --start-percent 0 \
    --end-percent 8 \
    -fast

  # CGUPS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/cgups-PAGR-${app}/tmem_trace.bin" \
    --title "GUPS PAGR Memory Trace" \
    --output "${app_dir}/cgups-PAGR-${app}-trace.png" \
    --start-percent 0 \
    --end-percent 100 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/cgups-hem-${app}/tmem_trace.bin" \
    --title "GUPS HeMem Memory Trace" \
    --output "${app_dir}/cgups-hem-${app}-trace.png" \
    --start-percent 0 \
    --end-percent 100 \
    -fast

  # BFS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/bfs-PAGR-${app}/tmem_trace.bin" \
    --title "BFS PAGR Memory Trace" \
    --output "${app_dir}/bfs-PAGR-${app}-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/bfs-hem-${app}/tmem_trace.bin" \
    --title "BFS HeMem Memory Trace" \
    --output "${app_dir}/bfs-hem-${app}-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  # Stream
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/stream-PAGR-${app}/tmem_trace.bin" \
    --title "Stream PAGR Memory Trace" \
    --output "${app_dir}/stream-PAGR-${app}-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/stream-hem-${app}/tmem_trace.bin" \
    --title "Stream HeMem Memory Trace" \
    --output "${app_dir}/stream-hem-${app}-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  # BC
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/bc-PAGR-${app}/tmem_trace.bin" \
    --title "BC PAGR Memory Trace" \
    --output "${app_dir}/bc-PAGR-${app}-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast

  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" \
    "${app_dir}/bc-hem-${app}/tmem_trace.bin" \
    --title "BC HeMem Memory Trace" \
    --output "${app_dir}/bc-hem-${app}-trace.png" \
    --start-percent 50 \
    --end-percent 55 \
    -fast
}

run_plots() {
  local app=$1
  local app_dir="${result_dir}"
  
  run_pagr_hem_loc $app
  # echo

  # run_pagr_hem $app
  # echo

  # run_pagr_loc $app
  # echo

  run_stats_pagr_hem $app
  echo
  
  run_trace $app
}

run_plots 2GB-6400
