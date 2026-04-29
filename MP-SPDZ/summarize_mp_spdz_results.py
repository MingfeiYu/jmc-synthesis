#!/usr/bin/env python3
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
    if x is None:
        return None
    s = str(x).strip()
    if s == "" or s.lower() == "none":
        return None
    try:
        return float(s)
    except ValueError:
        return None

def parse_optional_bool(x):
    if x is None:
        return None
    s = str(x).strip().lower()
    if s in ("true", "1", "yes", "y", "ok", "success"):
        return True
    if s in ("false", "0", "no", "n", "fail", "failed"):
        return False
    return None

def infer_variant_and_base(row):
    benchmark = row.get("benchmark", "") or ""
    variant = (row.get("variant", "") or "").strip()
    base = (row.get("base_benchmark", "") or "").strip()

    if not variant or variant == "unknown":
        if "_opt_" in benchmark:
            variant = "opt"
            inferred_base = benchmark.split("_opt_", 1)[0]
        else:
            variant = "orig"
            inferred_base = benchmark
    else:
        inferred_base = benchmark.split("_opt_", 1)[0] if "_opt_" in benchmark else benchmark

    if not base:
        base = inferred_base

    return base, variant

def summarize(values):
    return {
        "mean": statistics.mean(values),
        "median": statistics.median(values),
        "stdev": statistics.stdev(values) if len(values) >= 2 else 0.0,
        "min": min(values),
        "max": max(values),
    }

def get_metric_value(row, metric):
    """
    Backward-compatible extraction.
    Also infer missing per-party sent traffic when possible.
    """
    # direct hit
    val = parse_optional_float(row.get(metric, ""))
    if val is not None:
        return val

    # legacy fallback: some older raw CSVs may only have a generic data_sent_mb column
    if metric == "data_sent_mb_p0":
        generic = parse_optional_float(row.get("data_sent_mb", ""))
        if generic is not None:
            return generic

    # infer p0 from global and p1 if missing
    if metric == "data_sent_mb_p0":
        g = parse_optional_float(row.get("global_data_sent_mb", ""))
        p1 = parse_optional_float(row.get("data_sent_mb_p1", ""))
        if g is not None and p1 is not None:
            inferred = g - p1
            # avoid tiny negative values due to rounding
            return max(inferred, 0.0)

    # infer p1 from global and p0 if missing
    if metric == "data_sent_mb_p1":
        g = parse_optional_float(row.get("global_data_sent_mb", ""))
        p0 = parse_optional_float(row.get("data_sent_mb_p0", ""))
        if g is not None and p0 is not None:
            inferred = g - p0
            return max(inferred, 0.0)

    return None

def row_is_success(row):
    run_ok = parse_optional_bool(row.get("run_ok", None))
    compile_ok = parse_optional_bool(row.get("compile_ok", None))

    if run_ok is not None:
        return run_ok
    if compile_ok is not None:
        return compile_ok

    # fallback heuristic for legacy rows:
    # if there is a non-empty runtime and/or global communication, treat as successful
    if parse_optional_float(row.get("time_seconds", "")) is not None:
        return True
    if parse_optional_float(row.get("global_data_sent_mb", "")) is not None:
        return True
    return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--raw-csv", required=True)
    ap.add_argument("--summary-csv", default="results/summary_results.csv")
    args = ap.parse_args()

    raw_csv = Path(args.raw_csv).resolve()
    summary_csv = Path(args.summary_csv).resolve()
    summary_csv.parent.mkdir(parents=True, exist_ok=True)

    # Group by base_benchmark and variant
    groups = defaultdict(lambda: defaultdict(list))

    with raw_csv.open("r", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            base, variant = infer_variant_and_base(row)
            row["_base_benchmark_norm"] = base
            row["_variant_norm"] = variant
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
                successes = sum(1 for r in rows if row_is_success(r))

                for metric in METRICS:
                    vals = [get_metric_value(r, metric) for r in rows]
                    vals = [v for v in vals if v is not None]
                    if not vals:
                        continue

                    stats = summarize(vals)
                    summaries[variant][metric] = stats

                    w.writerow({
                        "base_benchmark": base,
                        "variant": variant,
                        "metric": metric,
                        "runs": len(rows),
                        "successes": successes,
                        **stats,
                        "improvement_vs_orig_pct": ""
                    })

            # add comparison rows (opt vs orig), using medians
            if "orig" in summaries and "opt" in summaries:
                for metric in METRICS:
                    if metric in summaries["orig"] and metric in summaries["opt"]:
                        orig = summaries["orig"][metric]["median"]
                        opt = summaries["opt"][metric]["median"]
                        if orig != 0:
                            improvement = (orig - opt) / orig * 100
                        else:
                            improvement = 0.0

                        w.writerow({
                            "base_benchmark": base,
                            "variant": "opt_vs_orig",
                            "metric": metric,
                            "runs": "",
                            "successes": "",
                            "mean": "",
                            "median": "",
                            "stdev": "",
                            "min": "",
                            "max": "",
                            "improvement_vs_orig_pct": improvement
                        })

if __name__ == "__main__":
    main()
