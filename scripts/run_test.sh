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

run_make() {
  cd ../src
  make clean
  make "$@"
  cd ../scripts
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
  sudo numactl -N0 env LD_PRELOAD="${PRELOAD}" "${cmd[@]}" > "${stdout_file}" 2> "${stderr_file}"
  
  local rc=$?

  mv -f trace.bin "${app_dir}"
  mv -f stats.txt "${app_dir}"
  mv -f debuglog.txt "${app_dir}"
  mv -f time.txt "${app_dir}"
  mv -f tmem_trace.bin "${app_dir}"
  mv -f preds.bin "${app_dir}"
  mv -f mig.bin "${app_dir}"
  mv -f cold.bin "${app_dir}"

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
  # ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cluster_no_app.py" "${app_dir}/trace.bin" -fast

  return ${rc}
}

# ---------------------------
# Define your knob values here
# ---------------------------
pebs_stats_vals=(1)
cluster_algo_vals=(1)
hem_algo_vals=(0)
dfs_algo_vals=(1)

his_size_vals=(8 16 32 64)
pred_depth_vals=(8 16 32 64)
dec_down_vals=(0.001 0.0001)
dec_up_vals=(0.001 0.01 0.1)
max_neighbor_vals=(8 16 32 64)
page_size_vals=(4096)

# ---------------------------
# Simple grid search
# ---------------------------
grid_search() {
  for pebs_stats in "${pebs_stats_vals[@]}"; do
    for cluster_algo in "${cluster_algo_vals[@]}"; do
      for hem_algo in "${hem_algo_vals[@]}"; do
        for his_size in "${his_size_vals[@]}"; do
          for pred_depth in "${pred_depth_vals[@]}"; do
            for dec_down in "${dec_down_vals[@]}"; do
              for dec_up in "${dec_up_vals[@]}"; do
                for max_neighbors in "${max_neighbor_vals[@]}"; do
                  for page_size in "${page_size_vals[@]}"; do
                    for dfs_algo in "${dfs_algo_vals[@]}"; do
                      run_name="bfs-his${his_size}-pred${pred_depth}-down${dec_down}-up${dec_up}-neigh${max_neighbors}-pg${page_size}"
                      echo "=== Running ${run_name} ==="

                      local app_dir="${result_dir}/${run_name}"


                      run_make pebs_stats=$pebs_stats cluster_algo=$cluster_algo hem_algo=$hem_algo \
                              his_size=$his_size pred_depth=$pred_depth dec_down=$dec_down dec_up=$dec_up \
                              max_neighbors=$max_neighbors page_size=$page_size dfs_algo=$dfs_algo

                      # run_app "${run_name}" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"
                      run_app "${run_name}" "${GAPBS_DIR}" "bfs" -f "twitter-2010.sg" -n 64 -r 0

                      # Trace files too big so remove them
                      rm -rf "${app_dir}/tmem_trace.bin"
                      rm -rf "${app_dir}/debuglog.txt"
                      rm -rf "${app_dir}/trace.bin"
                      rm -rf "${app_dir}/time.txt"
                    done
                  done
                done
              done
            done
          done
        done
      done
    done
  done
}

run_pagr_hem() {
  local app=$1
  echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
  echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

  local app_dir="${result_dir}"

  run_make cluster_algo=0 hem_algo=0 dfs_algo=0 lru_algo=0 dram_size=32212254720
  run_app "bc-local" "${GAPBS_DIR}" "./bc" -f "twitter-2010.sg" -n 64 -r 0
  run_app "resnet-local" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

  echo always | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
  echo always | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

  run_app "cgups-local" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

  echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
  echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

  run_app "bfs-local" "${GAPBS_DIR}" "./bfs" -f "twitter-2010.sg" -n 64 -r 0
  run_app "stream-local" "${STREAM_DIR}" "./stream" 2048 50

  # Resnet  
  run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
    his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
    max_neighbors=8 bfs_algo=0 dfs_algo=1 lru_algo=1
  run_app "resnet-PAGR-${app}" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

  run_make cluster_algo=0 hem_algo=1 dfs_algo=0
  run_app "resnet-hem-${app}" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

  echo always | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
  echo always | sudo tee /sys/kernel/mm/transparent_hugepage/defrag
  # CGUPS
  run_make cluster_algo=1 hem_algo=0 dfs_algo=1 lru_algo=1 \
    dec_down=0.0001 dec_up=0.01
  run_app "cgups-PAGR-${app}" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

  run_make cluster_algo=0 hem_algo=1 dfs_algo=0 lru_algo=0
  run_app "cgups-hem-${app}" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

  echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
  echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

  # BFS
  run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
    his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
    max_neighbors=8 dfs_algo=1 lru_algo=1
  run_app "bfs-PAGR-${app}" "${GAPBS_DIR}" "./bfs" -f "twitter-2010.sg" -n 64 -r 0

  run_make cluster_algo=0 hem_algo=1 dfs_algo=0
  run_app "bfs-hem-${app}" "${GAPBS_DIR}" "./bfs" -f "twitter-2010.sg" -n 64 -r 0

  # Stream
  run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
    his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
    max_neighbors=8 dfs_algo=1 lru_algo=1
  run_app "stream-PAGR-${app}" "${STREAM_DIR}" "./stream" 2048 50

  run_make cluster_algo=0 hem_algo=1
  run_app "stream-hem-${app}" "${STREAM_DIR}" "./stream" 2048 50

  # BC
  run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
    his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
    max_neighbors=8 dfs_algo=1 lru_algo=1
  run_app "bc-PAGR-${app}" "${GAPBS_DIR}" "./bc" -f "twitter-2010.sg" -n 64 -r 0

  run_make cluster_algo=0 hem_algo=1 dfs_algo=0
  run_app "bc-hem-${app}" "${GAPBS_DIR}" "./bc" -f "twitter-2010.sg" -n 64 -r 0


  # ResNet
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_resnet.py" \
    "${app_dir}/resnet-PAGR-${app}/app.txt" \
    "${app_dir}/resnet-hem-${app}/app.txt" \
    "${app_dir}/resnet-local/app.txt" \
    -o "${app_dir}/resnet-${app}.png" --labels "PAGR" "HeMem" "Local"

  # CGUPS
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_cgups_mul.py" \
    "${app_dir}/cgups-PAGR-${app}/app.txt" \
    "${app_dir}/cgups-hem-${app}/app.txt" \
    "${app_dir}/cgups-local/app.txt" \
    "${app_dir}/cgups-${app}.png" --labels "PAGR" "HeMem" "Local"

  # BFS (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bfs-PAGR-${app}/app.txt" \
    "${app_dir}/bfs-hem-${app}/app.txt" \
    "${app_dir}/bfs-local/app.txt" \
    "${app_dir}/bfs-${app}.png" --labels "PAGR" "HeMem" "Local" --title "BFS Trial Times"

  # Stream
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_stream_mul.py" \
    "${app_dir}/stream-PAGR-${app}/app.txt" \
    "${app_dir}/stream-hem-${app}/app.txt" \
    "${app_dir}/stream-local/app.txt" \
    "${app_dir}/stream-${app}.png" --labels "PAGR" "HeMem" "Local"

  # BC (GAPBS)
  ./venv/bin/python "${PLOT_SCRIPTS_DIR}/plot_gapbs_mul.py" \
    "${app_dir}/bc-PAGR-${app}/app.txt" \
    "${app_dir}/bc-hem-${app}/app.txt" \
    "${app_dir}/bc-local/app.txt" \
    "${app_dir}/bc-${app}.png" --labels "PAGR" "HeMem" "Local" --title "BC Trial Times"

}

run_pagr_hem 2GB

# grid_search


### Example usage (uncomment or call from command line / source)
# Single run using the original cgups command:
# run_app "cgups" "${CGUPS_DIR}/gups64-rw" "16" "move" "30" "kill" "60"
#
# Alternatively, if you prefer to pass the whole command as a single string:
# run_app "cgups" bash -c "${CGUPS_DIR}/gups64-rw 16 move 30 kill 60"
#
# run_app "mgups-2MB" "${MGUPS_DIR}/gups" 16000 2000 8 60 30 16
#
# Example calling multiple runs:
# run_app "stream-2MB" "${STREAM_DIR}/stream" 2048 10
# run_app "cgups-2MB-test" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60
# run_app "hgups-2MB" "${HGUPS_DIR}/gups-hotset-move" 8 100000000 34 8 32
# run_app "bfs-2MB" "${GAPBS_DIR}/bfs" -f "${GAPBS_DIR}/twitter-2010.sg" -n 16 -r 0

# YCSB
# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 bfs_algo=0 dfs_algo=1

# # Stop any existing memcached
# sudo pkill memcached
# sleep 1

# # Start memcached fresh with 4GB RAM
# sudo memcached -m 16000 -u nobody -p 11211 -d
# sleep 1   # give it a moment to start

# # Load phase
# run_app "ycsb-PaGR-load" "${YCSB_DIR}" ./bin/ycsb load memcached -s \
#   -P workloads/workloada \
#   -p recordcount=5000000 \
#   -p memcached.hosts=127.0.0.1 \
#   -threads 8

# # Run phase
# run_app "ycsb-PaGR-run" "${YCSB_DIR}" ./bin/ycsb run memcached -s \
#   -P workloads/workloada \
#   -p memcached.hosts=127.0.0.1 \
#   -p recordcount=10000000 \
#   -p operationcount=100000000 \
#   -p maxexecutiontime=60 \
#   -threads 8

# Stop memcached
# sudo pkill memcached



#resnet
# current best

# THP
# echo always | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
# echo always | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# Regular
# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 bfs_algo=0 dfs_algo=1
# run_app "resnet-PAGR" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

# run_make cluster_algo=0 hem_algo=1 dfs_algo=0
# run_app "resnet-hem" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

# run_make cluster_algo=0 hem_algo=0 dfs_algo=0
# run_app "resnet-no-155" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"




# run_make pebs_stats=1 cluster_algo=0 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 page_size=4096 bfs_algo=0
# run_app "resnet-best-4KB" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

# run_make cluster_algo=0 hem_algo=1 page_size=4096
# run_app "resnet-hem-4KB" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"


# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 page_size=1048576
# run_app "resnet-best-1MB" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

# run_make cluster_algo=0 hem_algo=1 page_size=1048576
# run_app "resnet-hem-1MB" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

# run_make cluster_algo=1 hem_algo=0 page_size=262144
# run_app "resnet-cluster-8MB" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

# run_make cluster_algo=1 hem_algo=1 page_size=262144
# run_app "resnet-both-8MB" "${RESNET_DIR}" "${ORIG_PWD}/venv/bin/python" "resnet_train.py"

#cgups

# run_make cluster_algo=0 hem_algo=0 dfs_algo=0
# run_app "cgups-no" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# run_make cluster_algo=0 hem_algo=1 dfs_algo=0
# run_app "cgups-hem" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.00005 dec_up=0.1 \
#   max_neighbors=8 dfs_algo=1
# run_app "cgups-PAGR" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# run_make cluster_algo=1 hem_algo=1 dfs_algo=1
# run_app "cgups-both" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# run_make cluster_algo=0 hem_algo=0 dfs_algo=1
# run_app "cgups-no-reg" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# run_make cluster_algo=0 hem_algo=1 dfs_algo=1
# run_app "cgups-hem-reg" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# run_make cluster_algo=1 hem_algo=0 dfs_algo=1
# run_app "cgups-cluster-reg" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# run_make cluster_algo=1 hem_algo=1 dfs_algo=1
# run_app "cgups-both" "${CGUPS_DIR}" "./gups64-rw" 8 move 30 kill 60

# #bfs
# THP
# echo always | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
# echo always | sudo tee /sys/kernel/mm/transparent_hugepage/defrag


# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 dfs_algo=1
# run_app "bfs-PAGR-cold" "${GAPBS_DIR}" "./bfs" -f "twitter-2010.sg" -n 64 -r 0

# Regular
# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 dfs_algo=1
# run_app "bfs-PAGR" "${GAPBS_DIR}" "./bfs" -f "twitter-2010.sg" -n 64 -r 0

# run_make cluster_algo=0 hem_algo=0 dfs_algo=0
# run_app "bfs-no" "${GAPBS_DIR}" "./bfs" -f "twitter-2010.sg" -n 64 -r 0

# run_make cluster_algo=0 hem_algo=1 dfs_algo=0
# run_app "bfs-hem" "${GAPBS_DIR}" "./bfs" -f "twitter-2010.sg" -n 64 -r 0


# run_make cluster_algo=1 hem_algo=1
# run_app "bfs-both" "${GAPBS_DIR}" "bfs" -f "twitter-2010.sg" -n 64 -r 0

# #stream

# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 dfs_algo=1
# run_app "stream-PAGR" "${STREAM_DIR}" "./stream" 2048 50

# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
# echo never | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 dfs_algo=1
# run_app "stream-PAGR" "${STREAM_DIR}" "./stream" 2048 50

# run_make cluster_algo=0 hem_algo=0
# run_app "stream-no" "${STREAM_DIR}" "./stream" 2048 50

# run_make cluster_algo=0 hem_algo=1
# run_app "stream-hem" "${STREAM_DIR}" "./stream" 2048 50

# run_make cluster_algo=1 hem_algo=0
# run_app "stream-cluster" "${STREAM_DIR}" "./stream" 2048 50

# run_make cluster_algo=1 hem_algo=1
# run_app "stream-both" "${STREAM_DIR}" "./stream" 2048 50

# bc
# run_make pebs_stats=1 cluster_algo=1 hem_algo=0 \
#   his_size=8 pred_depth=16 dec_down=0.0001 dec_up=0.01 \
#   max_neighbors=8 dfs_algo=1
# run_app "bc-PAGR" "${GAPBS_DIR}" "./bc" -f "twitter-2010.sg" -n 64 -r 0

# run_make cluster_algo=0 hem_algo=0 dfs_algo=0
# run_app "bc-no" "${GAPBS_DIR}" "./bc" -f "twitter-2010.sg" -n 64 -r 0

# run_make cluster_algo=0 hem_algo=1 dfs_algo=0
# run_app "bc-hem" "${GAPBS_DIR}" "./bc" -f "twitter-2010.sg" -n 64 -r 0
