"""Compare audio-callback timing between two Saleae Logic 2 captures.

Usage:
    python analyze.py data/before.csv data/after.csv [-o chart.png]

Each CSV is a Logic 2 digital export (File > Export Data > CSV) with the
channels named Audio, Load and Mark. Prints period statistics for each
capture and, if matplotlib is installed, draws a before/after chart.
"""
import argparse
import csv

LATE_US = 1010.0  # a 1 ms callback arriving later than this counts as late


def edges(path, channel):
    """Return (rise times, fall times) in seconds for one channel."""
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    col = rows[0].index(channel)
    rises, falls = [], []
    prev = None
    for row in rows[1:]:
        t, v = float(row[0]), int(row[col])
        if prev is not None and v != prev:
            (rises if v else falls).append(t)
        prev = v
    return rises, falls


def periods_us(path, channel="Audio"):
    rises, _ = edges(path, channel)
    return [(b - a) * 1e6 for a, b in zip(rises, rises[1:])]


def summary(path):
    p = periods_us(path)
    s = sorted(p)
    n = len(s)
    late = sum(x > LATE_US for x in p)
    return {
        "callbacks": n,
        "min": s[0],
        "p50": s[n // 2],
        "p99": s[int(n * 0.99)],
        "max": s[-1],
        "late": late,
        "late_pct": 100.0 * late / n,
    }


def chart(captures, out, skip=1000, window=200):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    surface, text, text2, muted = "#fcfcfb", "#0b0b0b", "#52514e", "#898781"
    grid, base = "#e1e0d9", "#c3c2b7"
    colors = ["#2a78d6", "#eb6834"]
    plt.rcParams.update({"font.size": 10, "text.color": text,
                         "axes.labelcolor": text2, "xtick.color": muted,
                         "ytick.color": muted})
    fig, axes = plt.subplots(len(captures), 1, figsize=(9, 5.6), sharex=True,
                             sharey=True, facecolor=surface, squeeze=False)
    for ax, (label, path), color in zip(axes[:, 0], captures, colors):
        p = periods_us(path)
        st = summary(path)
        w = p[skip:skip + window]
        ax.set_facecolor(surface)
        ax.axhline(1000, color=base, lw=1, zorder=1)
        ax.text(window + 1, 1000, "1 ms", color=muted, va="center", fontsize=9)
        ax.plot(range(len(w)), w, color=color, lw=2, zorder=3)
        ax.set_title(f"{label}\n{st['callbacks']:,} callbacks: max "
                     f"{st['max']:.0f} µs, {st['late']:,} late "
                     f"({st['late_pct']:.1f}%)", loc="left", fontsize=10, pad=6)
        ax.grid(axis="y", color=grid, lw=0.8, zorder=0)
        for side in ("top", "right", "left"):
            ax.spines[side].set_visible(False)
        ax.spines["bottom"].set_color(base)
        ax.tick_params(length=0)
        ax.set_ylabel("Callback period (µs)")
    axes[0, 0].set_ylim(750, 1350)
    axes[0, 0].set_yticks([800, 1000, 1200])
    axes[-1, 0].set_xlabel(f"Audio callback number ({window} consecutive "
                           "blocks, 48 kHz / 48 samples)")
    fig.suptitle("Daisy Seed audio callback timing with a 300 µs competing "
                 "interrupt", x=0.01, y=0.995, ha="left", fontsize=12,
                 fontweight="bold")
    fig.text(0.01, 0.005, "Measured with a Saleae logic analyzer on a GPIO "
             "toggled at the start of each callback. Load: DAC DMA callback "
             "busy-waiting 300 µs every 0.8 ms.", fontsize=8, color=text2)
    fig.tight_layout(rect=(0, 0.03, 0.97, 0.985))
    fig.savefig(out, dpi=200, facecolor=surface)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("before")
    ap.add_argument("after")
    ap.add_argument("-o", "--output", help="write a before/after chart (PNG)")
    args = ap.parse_args()

    for label, path in (("before", args.before), ("after", args.after)):
        s = summary(path)
        print(f"{label:6}  {s['callbacks']:6} callbacks  period min "
              f"{s['min']:7.1f}  p50 {s['p50']:7.1f}  p99 {s['p99']:7.1f}  "
              f"max {s['max']:7.1f} us  late {s['late']} "
              f"({s['late_pct']:.1f}%)")

    if args.output:
        chart([("Before: every interrupt at priority 0", args.before),
               ("After: audio DMA at priority 0, DAC at 1", args.after)],
              args.output)
        print(f"chart written to {args.output}")


if __name__ == "__main__":
    main()
