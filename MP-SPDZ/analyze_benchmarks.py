#!/usr/bin/env python3
"""Compares baseline and optimized circuits from the party-0 logs of
run_mp_spdz_benchmarks.py and prints Table 4 of the paper; with --tex, the
table is also written to results/results_table.tex.

Usage: analyze_benchmarks.py [--log-dir PATH] [--tex]
"""

import argparse
import glob
import os
import re
from collections import defaultdict

DEFAULT_LOG_DIR = os.path.join(os.path.dirname(__file__), "results", "logs")

DISPLAY_NAMES = {"AES": "AES-128"}

BENCHMARK_ORDER = ["INT-div", "INT-mul", "FP-add", "FP-div", "FP-mul", "FP-eq",
                   "AES", "Encoder", "LiteMIPS"]


def display_name(name):
    return DISPLAY_NAMES.get(name, name)


def parse_log(path):
    """Returns the OT and garbled-gate traffic [MB] and the AND, OT and wire-label times [s]."""
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


def parse_spec(spec):
    """Splits `name_opt_G_E` or `name_G_E` into (name, is_opt, "G_E")."""
    if "_opt_" in spec:
        idx = spec.index("_opt_")
        return spec[:idx], True, spec[idx + 5:]
    m = re.match(r"^(.+)_(\d+_\d+)$", spec)
    if m:
        return m.group(1), False, m.group(2)
    return spec, False, ""


def build_pairs(all_specs):
    pairs = []
    for ospec in sorted(s for s in all_specs if "_opt_" in s):
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
    parts = parse_spec(spec)[2].split("_")
    if len(parts) == 2:
        return parts[0], parts[1]
    return "?", "?"


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
    if baseline is None or optimized is None or baseline == 0:
        return ""
    pct = (optimized - baseline) / baseline * 100
    if abs(pct) < 0.005:
        return ""
    return f"{'+' if pct > 0 else '-'}{abs(pct):.2f}%"


def latex_opt_cell(b_val, o_val):
    if b_val is None or o_val is None or b_val == 0:
        return fmt(o_val)
    pct = (o_val - b_val) / b_val * 100
    arrow = r"$\uparrow$" if pct >= 0 else r"$\downarrow$"
    return rf"{fmt(o_val)} ({arrow} {abs(pct):.2f}\%)"


def metrics(spec, spec_data):
    """Averages over the repetitions, in KB and ms."""
    def scaled(key):
        v = avg(spec_data[spec][key])
        return round3(v * 1000) if v is not None else None
    oto = scaled("oto")
    mb = scaled("mb")
    total = round3(oto + mb) if (oto is not None and mb is not None) else None
    return {"wl": scaled("wl_t"), "oto": oto, "ot": scaled("ot_t"), "mb": mb,
            "and": scaled("and_t"), "total": total}


def generate_latex_table(pairs, spec_data, output_path):
    I = "    "
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
    print(f"\n[i] wrote {output_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log-dir", default=DEFAULT_LOG_DIR)
    parser.add_argument("--tex", action="store_true",
                        help="also write results/results_table.tex")
    args = parser.parse_args()

    spec_data = defaultdict(lambda: {"oto": [], "mb": [], "and_t": [], "ot_t": [], "wl_t": []})
    for path in sorted(glob.glob(os.path.join(args.log_dir, "*.p0.log"))):
        m = re.match(r"^(.+)\.rep(\d+)\.p0\.log$", os.path.basename(path))
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

    # benchmark, inputs, then OT label/comm./time, GG comm./eval., total comm.
    CW = [20, 13, 9, 10, 9, 10, 9, 10]
    keys = ["wl", "oto", "ot", "mb", "and", "total"]
    sep = "-" * (sum(CW) + 2 * len(keys) + 2)

    def row(*cells):
        return (f"{cells[0]:<{CW[0]}} {cells[1]:<{CW[1]}}"
                + "".join(f"  {cells[2 + i]:>{CW[2 + i]}}" for i in range(len(keys))))

    print(f"{'Benchmark':<{CW[0]}} {'#Inputs':>{CW[1]}}"
          f"  {'Oblivious Transfer (OT)':^{CW[2] + CW[3] + CW[4] + 4}}"
          f"  {'Garbled Gates (GG)':^{CW[5] + CW[6] + 2}}"
          f"  {'Total':>{CW[7]}}")
    print(row("", "(G/E)", "Label", "Comm.", "Time", "Comm.", "Eval.", "Comm."))
    print(row("", "", "[ms]", "[KB]", "[ms]", "[KB]", "[ms]", "[KB]"))
    print(sep)

    for base_name, bspec, ospec in pairs:
        b_g, b_e = extract_inputs(bspec)
        o_g, o_e = extract_inputs(ospec)
        dname = display_name(base_name)
        b = metrics(bspec, spec_data)
        o = metrics(ospec, spec_data)
        n_b = len([v for v in spec_data[bspec]["oto"] if v is not None])
        n_o = len([v for v in spec_data[ospec]["oto"] if v is not None])

        print()
        print(row(f"{dname} (base, n={n_b})", f"{b_g}/{b_e}", *[fmt(b[k]) for k in keys]))
        print(row(f"{'':>{len(dname)}} (opt.,  n={n_o})", f"{o_g}/{o_e}", *[fmt(o[k]) for k in keys]))
        print(row("", "", *[pct_str(b[k], o[k]) for k in keys]))

    print()
    print(sep)

    if args.tex:
        tex_path = os.path.join(os.path.dirname(os.path.abspath(args.log_dir)), "results_table.tex")
        generate_latex_table(pairs, spec_data, tex_path)


if __name__ == "__main__":
    main()
