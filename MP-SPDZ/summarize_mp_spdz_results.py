#!/usr/bin/env python3
"""Per-benchmark statistics of the measurements in raw_results.csv, with the
change of the medians from the baseline to the optimized circuit."""
from __future__ import annotations
import argparse, csv, statistics
from collections import defaultdict
from pathlib import Path

METRICS = [
    "time_seconds","global_data_sent_mb","data_sent_mb_p0","data_sent_mb_p1",
    "and_gates","and_time","and_process_time","and_main_thread_time","xor_time",
    "vm_rounds","ot_comm_time_sum","ot_matrix_time_sum"
]

def parse_optional_float(x):
    s = str(x or "").strip()
    if s == "" or s.lower() == "none":
        return None
    try:
        return float(s)
    except ValueError:
        return None

def base_and_variant(row):
    benchmark = row["benchmark"]
    base = row.get("base_benchmark") or benchmark.split("_opt_", 1)[0]
    variant = row.get("variant") or ("opt" if "_opt_" in benchmark else "orig")
    return base, variant

def summarize(values):
    return {
        "mean": statistics.mean(values),
        "median": statistics.median(values),
        "stdev": statistics.stdev(values) if len(values) >= 2 else 0.0,
        "min": min(values),
        "max": max(values),
    }

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--raw-csv", required=True)
    ap.add_argument("--summary-csv", default="results/summary_results.csv")
    args = ap.parse_args()

    raw_csv = Path(args.raw_csv).resolve()
    summary_csv = Path(args.summary_csv).resolve()
    summary_csv.parent.mkdir(parents=True, exist_ok=True)

    groups = defaultdict(lambda: defaultdict(list))
    with raw_csv.open("r", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            base, variant = base_and_variant(row)
            groups[base][variant].append(row)

    with summary_csv.open("w", newline="", encoding="utf-8") as f:
        fieldnames = [
            "base_benchmark","variant","metric",
            "runs","successes","mean","median","stdev","min","max",
            "improvement_vs_orig_pct"
        ]
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()

        for base, variants in sorted(groups.items()):
            summaries = {}
            for variant, rows in variants.items():
                summaries[variant] = {}
                successes = sum(1 for r in rows if r["run_ok"] == "True")
                for metric in METRICS:
                    vals = [parse_optional_float(r.get(metric)) for r in rows]
                    vals = [v for v in vals if v is not None]
                    if not vals:
                        continue
                    stats = summarize(vals)
                    summaries[variant][metric] = stats
                    w.writerow({"base_benchmark": base, "variant": variant, "metric": metric,
                                "runs": len(rows), "successes": successes, **stats,
                                "improvement_vs_orig_pct": ""})

            if "orig" in summaries and "opt" in summaries:
                for metric in METRICS:
                    if metric in summaries["orig"] and metric in summaries["opt"]:
                        orig = summaries["orig"][metric]["median"]
                        opt = summaries["opt"][metric]["median"]
                        improvement = (orig - opt) / orig * 100 if orig != 0 else 0.0
                        w.writerow({"base_benchmark": base, "variant": "opt_vs_orig", "metric": metric,
                                    "runs": "", "successes": "", "mean": "", "median": "",
                                    "stdev": "", "min": "", "max": "",
                                    "improvement_vs_orig_pct": improvement})

if __name__ == "__main__":
    main()
