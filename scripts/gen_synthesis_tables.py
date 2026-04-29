#!/usr/bin/env python3
"""
Generate LaTeX tables summarizing Boolean circuit optimization results.

Reads the two JSON result files written by xag_decomp_max_local (via
experiments::experiment::save()) and produces:

  mockturtle/experiments/results_arith_cp.tex
  mockturtle/experiments/results_crypto_gp.tex

These files correspond directly to Tables 1&2 in the
paper.  Call this script after 01_run_synthesis.sh finishes.

Usage:
  python3 scripts/gen_synthesis_tables.py [--experiments-dir PATH]
"""

import argparse
import json
import pathlib

INDENT = "    "

# Display-name overrides: internal benchmark name -> \texttt{...} label
DISPLAY_NAMES = {
    "AES":      r"\texttt{AES-128}",
    "SHA3":     r"\texttt{SHA3}",
    "Hamming":  r"\texttt{Hamming}",
    "Encoder":  r"\texttt{Encoder}",
    "LiteMIPS": r"\texttt{LiteMIPS}",
    "INT-add":  r"\texttt{INT-add}",
    "INT-div":  r"\texttt{INT-div}",
    "INT-mul":  r"\texttt{INT-mul}",
    "FP-add":   r"\texttt{FP-add}",
    "FP-div":   r"\texttt{FP-div}",
    "FP-mul":   r"\texttt{FP-mul}",
    "FP-eq":    r"\texttt{FP-eq}",
}


def load_latest(json_path: pathlib.Path) -> list:
    """Return the entry list from the most-recent version in a JSON result file."""
    data = json.loads(json_path.read_text(encoding="utf-8"))
    return data[-1]["entries"]


def fmt_int(n: int) -> str:
    """Format an integer with LaTeX thin-space thousands separator (\\,)."""
    s = f"{n:,}"          # e.g. "48,408"
    return s.replace(",", r"\,")   # e.g. "48\,408"


def fmt_reduction(and_base: int, jand_opt: int) -> str:
    """Percentage reduction of joint AND cost relative to the original AND count."""
    if and_base == 0:
        return "0.00"
    return f"{(and_base - jand_opt) / and_base * 100:.2f}"


def generate_table(entries: list, output_path: pathlib.Path) -> None:
    """Write a booktabs LaTeX table to output_path."""

    # Pre-compute display strings to determine column widths for alignment.
    rows = []
    for e in entries:
        name    = DISPLAY_NAMES.get(e["benchmark"],
                                    rf'\texttt{{{e["benchmark"]}}}')
        and_b   = fmt_int(e["AND count"])
        jand_b  = fmt_int(e["joint AND count"])
        and_o   = fmt_int(e["opt. AND count"])
        jand_o  = fmt_int(e["opt. joint AND count"])
        red     = fmt_reduction(e["AND count"], e["opt. joint AND count"])
        rows.append((name, and_b, jand_b, and_o, jand_o, red))

    # Column widths (max across all rows, at least the header label width).
    w_name = max(len(r[0]) for r in rows)
    w_num  = max(max(len(r[i]) for r in rows) for i in range(1, 5))
    w_red  = max(max(len(r[5]) for r in rows), 5)   # at least "XX.XX"

    def data_row(name, and_b, jand_b, and_o, jand_o, red):
        return (
            INDENT
            + f"{name:<{w_name}} & "
            + f"{and_b:>{w_num}}  & "
            + f"{jand_b:>{w_num}} & "
            + f"{and_o:>{w_num}}  & "
            + f"{jand_o:>{w_num}} & "
            + f"{red:>{w_red}}  \\\\"
        )

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

    for row in rows:
        lines.append(data_row(*row))

    lines += [
        INDENT + r"\bottomrule[1.2pt]",
        r"\end{tabular}",
    ]

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[ok] LaTeX table written to: {output_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--experiments-dir",
        default=None,
        help="Path to mockturtle/experiments/ "
             "(default: auto-detected relative to this script)",
    )
    args = ap.parse_args()

    if args.experiments_dir:
        exp_dir = pathlib.Path(args.experiments_dir)
    else:
        script_dir = pathlib.Path(__file__).resolve().parent
        exp_dir = script_dir.parent / "mockturtle" / "experiments"

    if not exp_dir.is_dir():
        raise SystemExit(f"[error] experiments directory not found: {exp_dir}")

    tasks = [
        ("hybrid_arith_cf_max_local.json",  "results_arith_cp.tex"),
        ("hybrid_crypto_gp_max_local.json", "results_crypto_gp.tex"),
    ]

    for json_name, tex_name in tasks:
        json_path = exp_dir / json_name
        if not json_path.exists():
            print(f"[warn] {json_path} not found — skipping (run synthesis first)")
            continue
        entries = load_latest(json_path)
        generate_table(entries, exp_dir / tex_name)


if __name__ == "__main__":
    main()
