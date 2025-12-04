#!/usr/bin/env bash

results_dir="./results"

run_pred() {
    local dir=$1
    local window=$2

    # echo "All predictions"
    # python plot_scripts/pred_acc.py "$dir/debuglog.txt" \
    # "$dir/tmem_trace.bin" \
    # "$dir/preds.bin" \
    # "$dir/cold.bin" \
    # --window "$window"

    echo "Migration predictions"
    python plot_scripts/pred_acc.py "$dir/debuglog.txt" \
    "$dir/tmem_trace.bin" \
    "$dir/mig.bin" \
    --window "$window"
    # "$dir/cold.bin" \
}

dir1="${results_dir}/bfs-PAGR-2GB"
dir2="${results_dir}/bfs-hem-2GB"
# echo "PAGR"
# run_pred "${results_dir}/resnet-PAGR-2GB" 0.01
# echo "HeMem"
# run_pred "${results_dir}/resnet-hem-2GB" 0.01
python plot_scripts/plot_pred_accs.py \
  --log1 "${dir1}/debuglog.txt" --trace1 "${dir1}/tmem_trace.bin" --pred1 "${dir1}/mig.bin" \
  --log2 "${dir2}/debuglog.txt" --trace2 "${dir2}/tmem_trace.bin" --pred2 "${dir2}/mig.bin" \
  --windows 0.001 0.005 0.01 0.03 0.05 0.07 0.1 0.5 1.0 \
  --labels "PAGR" "HeMem" \
  --out "${results_dir}/bfs_PAGR_HeMem_predacc.png"

