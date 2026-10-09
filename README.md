# Think Locally Before Computing Jointly: Efficient Secure Multi-Party Computation via Maximizing Local Processing

Artifact for the IACR TCHES paper of the same title.

## Overview

Garbled-circuit (GC) based MPC is usually optimized by minimizing the
*multiplicative complexity* (MC) of a circuit, i.e. its number of AND gates,
under the assumption that every AND gate has to be garbled. This work refines
that assumption: a gate whose inputs are known to, or locally computable by, a
single party can be evaluated outside the protocol at no cryptographic cost.
Counting only the AND gates that genuinely require joint evaluation yields the
*joint multiplicative complexity* (JMC), and the artifact implements an
automated, gate-level framework that restructures Boolean circuits to minimize
it.

The artifact has two components:

| Component | Directory | Purpose |
|-----------|-----------|---------|
| Circuit synthesis | `mockturtle/` | Optimizes XAGs for JMC and writes Bristol-format circuits |
| MPC evaluation | `MP-SPDZ/` | Runs baseline and optimized circuits under Yao's protocol, measuring communication and runtime |

Benchmarks: `arith_cf` (INT-add, INT-div, INT-mul, FP-add, FP-div, FP-mul,
FP-eq) and `crypto_gp` (AES-128, SHA3, Hamming, Encoder, LiteMIPS).

## Layout

```
.
├── mockturtle/
│   ├── experiments/
│   │   ├── {arith_cf,crypto_gp}_benchmarks/   # Verilog inputs and baseline Bristol circuits
│   │   │   └── jmc_opt_bristol/               # optimized circuits (written by the synthesis run)
│   │   ├── databases/xag_mc_min_6_db          # MC-optimal XAGs for all functions of up to six variables
│   │   ├── xag_decomp_max_local.cpp           # main synthesis experiment (Tables 1, 2)
│   │   ├── xag_decomp_kappa_sweep.cpp         # cut-size sweep (Table 3)
│   │   ├── xag_kappa{4,6,10,12}.cpp           # the same sweep at other cut sizes
│   │   └── multi_output_mc_check.cpp          # synthesis-quality check (§5.1.3)
│   └── include/mockturtle/
│       ├── algorithms/xag_decomp_max_local.hpp     # optimization framework
│       ├── algorithms/acdecomp_max_local.hpp       # ownership-aware ACD
│       ├── algorithms/mc_analyzer.hpp              # BS/FS synthesis and MUX construction
│       ├── algorithms/exact_mc_synthesis_multi.hpp # exact multi-output MC synthesis
│       └── views/ownership_view.hpp                # node ownership
├── MP-SPDZ/                   # MPC framework and evaluation drivers
├── scripts/                   # end-to-end pipeline, see below
├── results/revision/          # cut-size sweep and synthesis-quality data (see its README)
└── Dockerfile
```

## Setup

### Docker (recommended)

```bash
docker build -t jmc-artifact .                                          # ~20 min
docker run --rm -it jmc-artifact bash /artifact/scripts/run_all.sh 5    # quick
docker run --rm -it jmc-artifact bash /artifact/scripts/run_all.sh 100  # as in the paper
```

### Native

Requires CMake ≥ 3.14 and a C++17 compiler (GCC 9 / Clang 11); MP-SPDZ needs
its usual dependencies:

```bash
sudo apt-get install -y cmake ninja-build build-essential clang-11 libssl-dev \
  libgmp-dev libboost-dev libboost-thread-dev libsodium-dev libntl-dev \
  libclang-dev automake libtool
```

The optimized Bristol circuits are committed under `MP-SPDZ/Programs/Circuits/`,
so the MPC evaluation can be run without repeating the synthesis.

## Reproducing the tables

The synthesis binaries are run from `mockturtle/build`, which must contain a
copy of `experiments/databases/xag_mc_min_6_db`; the scripts do this
automatically.

### Tables 1 and 2 — circuit-level results

```bash
bash scripts/01_run_synthesis.sh
```

Runs the optimization on both suites until saturation, writes the optimized
circuits to `experiments/{suite}_benchmarks/jmc_opt_bristol/`, and generates
`results_arith_cp.tex` and `results_crypto_gp.tex` through
`scripts/gen_synthesis_tables.py`. AND and joint AND counts are deterministic
and should match the paper exactly. Runtime: 10–20 minutes.

### Table 3 — sensitivity to the cut-size bound κ

The cut-size bound is a compile-time constant, so each value of κ is a separate
binary:

```bash
cd mockturtle/build
./experiments/xag_decomp_kappa_sweep 8                   # κ = 8
JMC_CUT_LIMIT=15 ./experiments/xag_decomp_kappa_sweep 8  # κ = 8*, 15 priority cuts per node
./experiments/xag_kappa4 4                               # likewise 6, 10
```

Each run prints one `CSV,` line per benchmark with the AND and joint AND counts,
the runtime, and the total and multiplicative depth. κ = 10 is not supported for
`INT-div` and `FP-div`, whose truth tables exceed the width handled by the
decomposition engine. The collected data is in
`results/revision/kappa_sweep.csv`; the candidate rejections and the
accepted-cut statistics quoted in §6.3 are in `kappa_sweep_rejections.csv` and
`accepted_cuts.csv` of the same directory.

### Table 4 — end-to-end MPC evaluation

```bash
bash scripts/02_copy_circuits.sh
bash scripts/03_build_mpc.sh
bash scripts/04_run_mpc_eval.sh 100   # 5 for a quick check
bash scripts/05_analyze_results.sh
```

Writes `MP-SPDZ/results/results_table.tex` next to the raw measurements in
`raw_results.csv`. Communication figures are deterministic; absolute timings
depend on the machine, so compare relative improvements rather than absolute
milliseconds.

### Table 5 — OT versus garbled-gate trade-off

Derived from the tables above, one row per benchmark:

| Column | Derivation |
|--------|------------|
| ΔEval. inputs | evaluator inputs, optimized − baseline (Table 4, `#Inputs (G/E)`; the evaluator is the party with fewer inputs) |
| ΔOT comm. | `OT Comm.` difference in Table 4; equals 16 B per added evaluator input |
| Δ#GGs | baseline AND count (Tables 1–2) − jointly derived AND count after optimization |
| ΔGG comm. | `GG Comm.` difference in Table 4, where `GG Comm.` = `Total Comm.` − `OT Comm.` |
| ΔNet comm. | `Total Comm.` difference in Table 4 |
| Δ#ANDs | total AND count, optimized − baseline (Tables 1–2) |
| Depth / AND depth, Synth. time | `results/revision/kappa_sweep.csv`, rows with `config = k8` |

### Section 5.1.3 — synthesis quality

How far the per-output database lookup is from the exact multi-output MC
optimum:

```bash
cd mockturtle/build
JMC_DUMP_INSTANCES=instances.txt ./experiments/xag_decomp_kappa_sweep 8
./experiments/multi_output_mc_check instances.txt optcheck.csv 2000000
```

The first run dumps every bound-set and free-set group encountered, the second
solves each distinct group exactly. Reference output is in `results/revision/`.
Expect 476 against 472 AND gates on the bound-set groups (optimal in 98.9 % of
the instances) and 1320 against 1095 on the purely local free-set groups.

## Where each algorithm lives

| Paper | Concept | File | Entity |
|-------|---------|------|--------|
| §4.1.2 | Node ownership, locally derivable vs. jointly derived | `views/ownership_view.hpp` | `owner_view`, `assign_ownership`, `propogate_ownership_local`, `is_local` |
| §4.1.2 | Frontier, extraction of the joint sub-circuit | `algorithms/xag_decomp_max_local.hpp` | `identify_frontier`, `identify_frontier_and_extract_mpc` |
| §4.1.3 | JMC cost function | `algorithms/xag_decomp_max_local.hpp` | `mc_mpc_cost`, `collect_joint_mc_cost` |
| §4.2, Algorithm 1 | Peephole framework and cut enumeration | `algorithms/xag_decomp_max_local.hpp` | `xag_decomp_max_local_in_place::run`, `run_reverse_topo_each`; κ is `JMC_NUM_VARS`, the priority-cut budget is `cut_enum_ps.cut_limit` |
| §5.1 | Joint compute-oriented ACD | `algorithms/acdecomp_max_local.hpp` | `acdecomp_max_local`, `multi_run` |
| §5.1.1, Algorithm 2 | Ownership-aware variable partitioning, free-set functions | `algorithms/acdecomp_max_local.hpp` | `push_into_free_set`, `investigate_decomp`, `compute_decomp` |
| §5.1.2 | Encoding and bound-set function derivation | `algorithms/acdecomp_max_local.hpp` | `compute_encodings`, `solve_encoding_exact`, `solve_encoding_heuristic`, `create_covering_matrix`, `solve_covering_exact` |
| §5.1.3 | Synthesis of the derived functions by canonicalization and lookup | `algorithms/mc_analyzer.hpp`, `experiments/databases/xag_mc_min_6_db` | `mc_analyzer::build_db`, `get_mc`, `min_mc_resyn`, `min_mc_resyn_mpc` |
| §5.1.3 | Exact multi-output MC optimum, used to assess that lookup | `algorithms/exact_mc_synthesis_multi.hpp`, `experiments/multi_output_mc_check.cpp` | `exact_mc_synthesis_multi` |
| §5.2 | Low-MC multiplexer construction | `algorithms/mc_analyzer.hpp`, `algorithms/acdecomp_max_local.hpp` | `construct_low_mc_mux`, `construct_mux_tree`, `compute_top_mux` |

Header paths are relative to `mockturtle/include/mockturtle/`, experiment paths
to `mockturtle/`.

## Instrumentation

Optional, all disabled by default:

| Variable | Effect |
|----------|--------|
| `JMC_DUMP_INSTANCES` | Appends every decomposition instance to the given file |
| `JMC_DUMP_TAG` | Tag written with each dumped instance and trace line |
| `JMC_TRACE_CUTS` | Prints one `ACCEPT,` line per committed rewrite, with cut size and gain |
| `JMC_CUT_LIMIT` | Overrides the priority-cut budget of the sweep binaries |

## Troubleshooting

**`xag_mc_min_6_db: No such file or directory`** — copy
`mockturtle/experiments/databases/xag_mc_min_6_db` into the directory the binary
is invoked from.

**`yao-party.x: cannot execute binary file`** — the committed binary was built
for a different architecture; rerun `scripts/03_build_mpc.sh`.

**MP-SPDZ build fails with missing headers** — install the `-dev` packages
listed under *Native* above.

**Port conflicts during the MPC runs** — pass `--start-port <N>` to
`MP-SPDZ/run_mp_spdz_benchmarks.py`.
