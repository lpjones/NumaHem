import os
import sys
import argparse
import matplotlib.pyplot as plt
from pathlib import Path
from parse_apps import parse_resnet, parse_gapbs, parse_cgups, parse_stream

# Get the directory of the current script
script_dir = Path(__file__).resolve().parent
plt.style.use(os.path.join(script_dir, 'ieee.mplstyle'))

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("input_dir", help="directory of results")
    p.add_argument("output", help="Output image filename (e.g. out.png, out.pdf).")
    p.add_argument("--workload", required=True, choices=["cgups", "bc", "resnet", "bfs", "stream"])
    p.add_argument("--xlabel", default="PEBS Sample Rate", help="Label for the x-axis.")
    p.add_argument("--ylabel", default="Throughput", help="Label for the y-axis.")
    p.add_argument("--yrange", nargs=2, default=[0,6], help="y range")
    p.add_argument("--title", default="Throughput vs Sample Rate", help="Plot title.")
    return p.parse_args()

def parse_folder(args):
    match args.workload:
        case "cgups":
            pass
    return 0

def get_parse_func(workload):
    match workload:
        case "cgups":
            return parse_cgups
        case "bc":
            return parse_gapbs
        case "resnet":
            return parse_resnet
        case "bfs":
            return parse_gapbs
        case "stream":
            return parse_stream

def get_sample_rate(folder):
    return int(folder.split("-")[-1])

def get_algo(folder):
    return folder.split("-")[1]

def find_files(args):
    parse_func = get_parse_func(args.workload)
    workload_dict = dict()
    
    for folder in os.listdir(args.input_dir):
        if not os.path.isdir(os.path.join(args.input_dir, folder)):
            continue
        if args.workload in folder:
            in_dir = os.path.join(args.input_dir, folder, "app.txt")
            sample_rate = get_sample_rate(folder)
            algo = get_algo(folder)

            workload_vals = parse_func(in_dir)
            workload_avg = sum(workload_vals) / len(workload_vals)

            if algo not in workload_dict:
                workload_dict[algo] = [(sample_rate, workload_avg)]
            else:
                workload_dict[algo].append((sample_rate, workload_avg))
    for algo in workload_dict:
        workload_dict[algo] = sorted(workload_dict[algo], key=lambda x: x[0])
    return workload_dict

def get_label(algo):
    match algo:
        case "hem":
            return "HeMem", "red", "--"
        case "PAGR":
            return "PAGR", "black", "-"
        case "local":
            return "All Fast Mem", "blue", ":"
    return ""

def plot_workload(workload_dict, args):
    for idx, algo in enumerate(workload_dict):
        x_vals = [x[0] for x in workload_dict[algo]]
        y_vals = [y[1] for y in workload_dict[algo]]
        label, color, ls = get_label(algo)

        plt.plot(x_vals, y_vals, label=label, color=color, ls=ls)

    plt.xscale("log")
    plt.xlabel(args.xlabel)
    plt.ylabel(args.ylabel)
    plt.title(args.title)
    plt.legend()
    plt.ylim(float(args.yrange[0]), float(args.yrange[1]))

    plt.savefig(args.output)
    plt.close()

    print(f"Saved plot to {args.output}")

if __name__ == "__main__":
    args = parse_args()
    workload_dict = find_files(args)
    # print(workload_dict)
    plot_workload(workload_dict, args)