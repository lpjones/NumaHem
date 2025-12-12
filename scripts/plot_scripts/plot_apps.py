import matplotlib.pyplot as plt
import os
import sys

def plot_multiple(datasets, args):
    if not datasets:
        print("No datasets provided.", file=sys.stderr)
        return 2

    # verify at least one dataset has data
    if all(len(d) == 0 for d in datasets):
        print("No numbers found in any input files.", file=sys.stderr)
        return 2

    max_val = 0
    for nums, label in zip(datasets, args.labels):
        if not nums:
            # plot an empty placeholder so legend remains consistent
            plt.plot([], [], label=f"{label} (no data)")
            continue
        x = nums[0]
        y = nums[1]
        max_val = max(max(y), max_val)

        print(label, sum(y) / len(y))
        plt.plot(x, y, label=label)

    plt.xlabel(args.xlabel)
    plt.ylabel(args.ylabel)
    plt.title(args.title)
    plt.legend()

    if args.yrange == None:
        ymin = 0
        ymax = max_val
    else:
        ymin = float(args.yrange[0])
        ymax = float(args.yrange[1])
    plt.ylim(ymin, ymax)

    # Ensure parent dir exists
    odir = os.path.dirname(args.output)
    if odir and not os.path.exists(odir):
        os.makedirs(odir, exist_ok=True)
    
    plt.savefig(args.output)

    plt.close()
    print(f"Saved plot to {args.output}")
    return 0