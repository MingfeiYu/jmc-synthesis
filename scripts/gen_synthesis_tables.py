#!/usr/bin/env python3
"""Writes Tables 1 and 2 of the paper from the JSON files of xag_decomp_max_local.

Usage: gen_synthesis_tables.py [--experiments-dir PATH]
"""

import argparse
import json
import pathlib

INDENT = "    "

DISPLAY_NAMES = {"AES": "AES-128"}


def fmt_int(n):
    return f"{n:,}".replace(",", r"\,")


def fmt_reduction(num_ands, num_joint_ands_opt):
    if num_ands == 0:
        return "0.00"
    return f"{(num_ands - num_joint_ands_opt) / num_ands * 100:.2f}"


def generate_table(entries, output_path):
    rows = []
    for e in entries:
        name = r"\texttt{" + DISPLAY_NAMES.get(e["benchmark"], e["benchmark"]) + "}"
        rows.append((name,
                     fmt_int(e["AND count"]),
                     fmt_int(e["joint AND count"]),
                     fmt_int(e["opt. AND count"]),
                     fmt_int(e["opt. joint AND count"]),
                     fmt_reduction(e["AND count"], e["opt. joint AND count"])))

    w_name = max(len(r[0]) for r in rows)
    w_num = max(len(r[i]) for r in rows for i in range(1, 5))
    w_red = max(max(len(r[5]) for r in rows), 5)

    lines = [
        r"\begin{tabular}{lrrrrr}",
        INDENT + r"\toprule[1.2pt]",
        INDENT + r"Benchmark &",
        INDENT + r"\makecell{\#ANDs\\(w/o opt.)} &",
        INDENT + r"\makecell{\#Joint ANDs\\(w/o opt.)} &",
        INDENT + r"\makecell{\#ANDs\\(w/ opt.)} &",
        INDENT + r"\makecell{\#Joint ANDs\\(w/ opt.)} &",
        INDENT + r"\makecell{Red.\\{} [\%]} \\",
        INDENT + r"\midrule",
    ]
    for name, and_b, jand_b, and_o, jand_o, red in rows:
        lines.append(f"{INDENT}{name:<{w_name}} & {and_b:>{w_num}}  & {jand_b:>{w_num}} & "
                     f"{and_o:>{w_num}}  & {jand_o:>{w_num}} & {red:>{w_red}}  \\\\")
    lines += [INDENT + r"\bottomrule[1.2pt]", r"\end{tabular}"]

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[i] wrote {output_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--experiments-dir", default=None,
                    help="mockturtle/experiments/ (default: relative to this script)")
    args = ap.parse_args()

    if args.experiments_dir:
        exp_dir = pathlib.Path(args.experiments_dir)
    else:
        exp_dir = pathlib.Path(__file__).resolve().parent.parent / "mockturtle" / "experiments"

    for json_name, tex_name in [("hybrid_arith_cf_max_local.json", "results_arith_cp.tex"),
                                ("hybrid_crypto_gp_max_local.json", "results_crypto_gp.tex")]:
        json_path = exp_dir / json_name
        if not json_path.exists():
            print(f"[w] {json_path} not found, skipped")
            continue
        # the experiment appends one version per run; use the latest
        entries = json.loads(json_path.read_text(encoding="utf-8"))[-1]["entries"]
        generate_table(entries, exp_dir / tex_name)


if __name__ == "__main__":
    main()
