#!/usr/bin/env python3
"""
Analyze MP-SPDZ garbled circuit benchmark logs.

For each baseline/optimized pair, reports six metrics:
  1. Label [ms]      — wire-label sending time  (OT sub-column)
  2. Comm. [KB]      — OT communication volume  (OT sub-column)
  3. Time [ms]       — OT wall-clock time        (OT sub-column)
  4. Comm. [KB]      — garbled-gate data volume  (GG sub-column)
  5. Eval. [ms]      — AND-gate evaluation time  (GG sub-column)
  6. Total Comm.[KB] — columns 2 + 4 combined

Column layout matches the paper's Table.

Usage:
  python3 analyze_benchmarks.py [--log-dir PATH] [--tex]
"""

import argparse
import glob
import os
import re
from collections import defaultdict

DEFAULT_LOG_DIR = os.path.join(os.path.dirname(__file__), "results", "logs")

# Display name overrides: internal base_name -> printed label
DISPLAY_NAMES = {"AES": "AES-128"}


def display_name(name):
    return DISPLAY_NAMES.get(name, name)


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def parse_log(path):
    """Return (one_to_one_mb, sending_mb, and_time_s, ot_time_s, wire_label_time_s)."""
    one_to_one_mb = sending_mb = and_time_s = ot_time_s = wire_label_time_s = None
    for line in open(path):
        if one_to_one_mb is None:
            m = re.search(r"Sending one-to-one\s+([\d.e+\-]+)\s+MB in \d+ rounds", line)
            if m:
                one_to_one_mb = float(m.group(1))
        if sending_mb is None:
            m = re.search(r"Sending directly\s+([\d.e+\-]+)\s+MB in \d+ rounds", line)
            if m:
                sending_mb = float(m.group(1))
        if and_time_s is None:
            m = re.search(r"^AND time:\s+([\d.e+\-]+)", line)
            if m:
                and_time_s = float(m.group(1))
        if ot_time_s is None:
            m = re.search(r"^OT time:\s+([\d.e+\-]+)", line)
            if m:
                ot_time_s = float(m.group(1))
        if wire_label_time_s is None:
            m = re.search(r"^Wire label time:\s+([\d.e+\-]+)", line)
            if m:
                wire_label_time_s = float(m.group(1))
        if None not in (one_to_one_mb, sending_mb, and_time_s, ot_time_s, wire_label_time_s):
            break
    return one_to_one_mb, sending_mb, and_time_s, ot_time_s, wire_label_time_s


# ---------------------------------------------------------------------------
# Pairing logic
# ---------------------------------------------------------------------------

def parse_spec(spec):
    """Return (base_name, is_opt, suffix) from a benchmark spec string."""
    if "_opt_" in spec:
        idx = spec.index("_opt_")
        return spec[:idx], True, spec[idx + 5:]
    m = re.match(r"^(.+)_(\d+_\d+)$", spec)
    if m:
        return m.group(1), False, m.group(2)
    return spec, False, ""


def build_pairs(all_specs):
    """Match each optimized spec with its baseline. Returns list of (base_name, bspec, ospec)."""
    opt_specs = sorted(s for s in all_specs if "_opt_" in s)
    pairs = []
    for ospec in opt_specs:
        base_name, _, opt_suffix = parse_spec(ospec)
        candidates = [s for s in all_specs if "_opt_" not in s and s.startswith(base_name + "_")]
        if not candidates:
            continue
        if len(candidates) == 1:
            bspec = candidates[0]
        else:
            suffix_match = [c for c in candidates if c == f"{base_name}_{opt_suffix}"]
            bspec = suffix_match[0] if suffix_match else candidates[0]
        pairs.append((base_name, bspec, ospec))
    return pairs


def extract_inputs(spec):
    """Return (garbler_bits, evaluator_bits) strings from the spec suffix."""
    _, _, suffix = parse_spec(spec)
    parts = suffix.split("_")
    if len(parts) == 2:
        return parts[0], parts[1]
    return "?", "?"


# ---------------------------------------------------------------------------
# Formatting helpers
# ---------------------------------------------------------------------------

def avg(vals):
    vals = [v for v in vals if v is not None]
    return sum(vals) / len(vals) if vals else None


def round3(v):
    return round(v, 3) if v is not None else None


def fmt(v, decimals=3):
    if v is None:
        return "N/A"
    if abs(v) < 1e-3:
        return f"{v:.3e}"
    return f"{v:.{decimals}f}"


def pct_str(baseline, optimized):
    """Return '+X.XX%' / '-X.XX%' / '' string."""
    if baseline is None or optimized is None or baseline == 0:
        return ""
    pct = (optimized - baseline) / baseline * 100
    if abs(pct) < 0.005:
        return ""
    sign = "+" if pct > 0 else "-"
    return f"{sign}{abs(pct):.2f}%"


def latex_opt_cell(b_val, o_val):
    """'VALUE ($\\uparrow/\\downarrow$ PCT\\%)' for the optimized LaTeX cell."""
    if b_val is None or o_val is None or b_val == 0:
        return fmt(o_val)
    pct = (o_val - b_val) / b_val * 100
    arrow = r"$\uparrow$" if pct >= 0 else r"$\downarrow$"
    return rf"{fmt(o_val)} ({arrow} {abs(pct):.2f}\%)"


def metrics(spec, spec_data):
    """Return dict of display-unit metrics (KB / ms) for a spec, averaged over reps."""
    def ms(key):
        v = avg(spec_data[spec][key])
        return round3(v * 1000) if v is not None else None
    def kb(key):
        v = avg(spec_data[spec][key])
        return round3(v * 1000) if v is not None else None
    wl  = ms("wl_t")
    oto = kb("oto")
    ot  = ms("ot_t")
    mb  = kb("mb")
    and_ = ms("and_t")
    total = round3(oto + mb) if (oto is not None and mb is not None) else None
    return {"wl": wl, "oto": oto, "ot": ot, "mb": mb, "and": and_, "total": total}


# ---------------------------------------------------------------------------
# LaTeX table
# ---------------------------------------------------------------------------

def generate_latex_table(pairs, spec_data, output_path):
    I = "    "  # 4-space indent for every inner line

    lines = [
        r"\begin{tabular}{lcccccccc}",
        I + r"\toprule[1.2pt]",
        I + r"\multirow{2}{*}{Benchmark} & \multirow{2}{*}{\#Inputs (G/E)} & \multicolumn{3}{c}{Oblivious Transfer~(OT)} & \multicolumn{2}{c}{Garbled Gates~(GG)} & \multirow{2}{*}{Total Comm. [KB]} \\",
        I + r"\cmidrule(lr){3-5} \cmidrule(lr){6-7}",
        I + r" & & Label [ms] & Comm. [KB] & Time [ms] & Comm. [KB] & Eval. [ms] & \\",
    ]

    for base_name, bspec, ospec in pairs:
        b_g, b_e = extract_inputs(bspec)
        o_g, o_e = extract_inputs(ospec)
        dname = display_name(base_name)
        b = metrics(bspec, spec_data)
        o = metrics(ospec, spec_data)

        lines.append(I + r"\midrule")
        lines.append(
            I + rf"\texttt{{{dname}}} (base) & {b_g}/{b_e}"
            rf" & {fmt(b['wl'])} & {fmt(b['oto'])} & {fmt(b['ot'])}"
            rf" & {fmt(b['mb'])} & {fmt(b['and'])}"
            rf" & {fmt(b['total'])} \\"
        )
        lines.append(
            I + rf"\texttt{{{dname}}} (opt.) & {o_g}/{o_e}"
            rf" & {latex_opt_cell(b['wl'],    o['wl'])}"
            rf" & {latex_opt_cell(b['oto'],   o['oto'])}"
            rf" & {latex_opt_cell(b['ot'],    o['ot'])}"
            rf" & {latex_opt_cell(b['mb'],    o['mb'])}"
            rf" & {latex_opt_cell(b['and'],   o['and'])}"
            rf" & {latex_opt_cell(b['total'], o['total'])} \\"
        )

    lines += [I + r"\bottomrule[1.2pt]", r"\end{tabular}"]

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"\nLaTeX table written to: {output_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

BENCHMARK_ORDER = ["INT-div", "INT-mul", "FP-add", "FP-div", "FP-mul", "FP-eq",
                   "AES", "Encoder", "LiteMIPS"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log-dir", default=DEFAULT_LOG_DIR)
    parser.add_argument("--tex", action="store_true",
                        help="also write a LaTeX table to results/results_table.tex")
    args = parser.parse_args()

    # Collect per-spec metric lists from log files
    spec_data = defaultdict(lambda: {"oto": [], "mb": [], "and_t": [], "ot_t": [], "wl_t": []})
    for path in sorted(glob.glob(os.path.join(args.log_dir, "*.p0.log"))):
        fname = os.path.basename(path)
        m = re.match(r"^(.+)\.rep(\d+)\.p0\.log$", fname)
        if not m:
            continue
        spec = m.group(1)
        oto, mb, and_t, ot_t, wl_t = parse_log(path)
        spec_data[spec]["oto"].append(oto)
        spec_data[spec]["mb"].append(mb)
        spec_data[spec]["and_t"].append(and_t)
        spec_data[spec]["ot_t"].append(ot_t)
        spec_data[spec]["wl_t"].append(wl_t)

    pairs = build_pairs(list(spec_data.keys()))
    pairs.sort(key=lambda t: BENCHMARK_ORDER.index(t[0]) if t[0] in BENCHMARK_ORDER else len(BENCHMARK_ORDER))

    # ── Terminal table ──────────────────────────────────────────────────────
    # Column widths
    CW = [20, 13, 9, 10, 9, 10, 9, 10]  # Benchmark, G/E, then 6 metric cols
    METRIC_COLS = 6

    sep = "─" * (sum(CW) + 2 * METRIC_COLS + 2)

    def row(*cells):
        return (
            f"{cells[0]:<{CW[0]}} {cells[1]:<{CW[1]}}"
            + "".join(f"  {cells[2 + i]:>{CW[2 + i]}}" for i in range(METRIC_COLS))
        )

    # Two-line header (group labels + column labels)
    grp = (
        f"{'Benchmark':<{CW[0]}} {'#Inputs':>{CW[1]}}"
        f"  {'───────── Oblivious Transfer (OT) ─────────':^{CW[2]+CW[3]+CW[4]+4}}"
        f"  {'──── Garbled Gates (GG) ────':^{CW[5]+CW[6]+2}}"
        f"  {'Total':>{CW[7]}}"
    )
    hdr = row("", "(G/E)", "Label", "Comm.", "Time", "Comm.", "Eval.", "Comm.")
    sub = row("", "",      "[ms]",  "[KB]",  "[ms]", "[KB]",  "[ms]", "[KB]")

    print(grp)
    print(hdr)
    print(sub)
    print(sep)

    for base_name, bspec, ospec in pairs:
        b_g, b_e = extract_inputs(bspec)
        o_g, o_e = extract_inputs(ospec)
        dname = display_name(base_name)
        b = metrics(bspec, spec_data)
        o = metrics(ospec, spec_data)
        n_b = len([v for v in spec_data[bspec]["oto"] if v is not None])
        n_o = len([v for v in spec_data[ospec]["oto"] if v is not None])

        keys = ["wl", "oto", "ot", "mb", "and", "total"]

        print()
        print(row(
            f"{dname} (base, n={n_b})", f"{b_g}/{b_e}",
            *[fmt(b[k]) for k in keys]
        ))
        print(row(
            f"{'':>{len(dname)}} (opt.,  n={n_o})", f"{o_g}/{o_e}",
            *[fmt(o[k]) for k in keys]
        ))
        print(row(
            "", "",
            *[pct_str(b[k], o[k]) for k in keys]
        ))

    print()
    print(sep)

    if args.tex:
        tex_path = os.path.join(os.path.dirname(os.path.abspath(args.log_dir)),
                                "results_table.tex")
        generate_latex_table(pairs, spec_data, tex_path)


if __name__ == "__main__":
    main()
