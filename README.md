# Think Locally Before Computing Jointly: Efficient Secure Multi-Party Computation via Maximizing Local Processing

Artifact of the paper

> Mingfei Yu, Chenglu Jin, and Giovanni De Micheli. *Think Locally Before
> Computing Jointly: Efficient Secure Multi-Party Computation via Maximizing
> Local Processing.* IACR Transactions on Cryptographic Hardware and Embedded
> Systems (TCHES), 2027(1).

| Author | Affiliation | Contact |
|--------|-------------|---------|
| Mingfei Yu | Stanford University, USA | mfyu@stanford.edu |
| Chenglu Jin | Centrum Wiskunde & Informatica (CWI), The Netherlands | chenglu.jin@cwi.nl |
| Giovanni De Micheli | EPFL, Switzerland | giovanni.demicheli@epfl.ch |

The code is released under the MIT license. The bundled copies of mockturtle
and MP-SPDZ keep their own licenses, see `LICENSE`.

## Overview

In garbled-circuit (GC) based MPC, every AND gate is usually assumed to be
garbled, and circuits are optimized for their multiplicative complexity (MC),
the number of AND gates. A gate whose inputs are known to a single party,
however, can be evaluated by that party outside the protocol. The number of
AND gates that do need joint evaluation is the joint multiplicative complexity
(JMC), and this artifact restructures Boolean circuits to reduce it.

`mockturtle/` contains the synthesis framework, which optimizes XAGs for JMC
and writes Bristol-format circuits. `MP-SPDZ/` runs the baseline and the
optimized circuits under Yao's protocol and measures communication and
runtime.

The benchmarks are `arith_cf` (INT-add, INT-div, INT-mul, FP-add, FP-div,
FP-mul, FP-eq) and `crypto_gp` (AES-128, SHA3, Hamming, Encoder, LiteMIPS).

## Layout

```
.
├── mockturtle/
│   ├── experiments/
│   │   ├── {arith_cf,crypto_gp}_benchmarks/   # Verilog inputs and baseline Bristol circuits
│   │   │   └── jmc_opt_bristol/               # optimized circuits
│   │   ├── databases/xag_mc_min_6_db          # MC-optimal XAGs per spectral class of six-variable functions
│   │   ├── xag_decomp_max_local.cpp           # Tables 1 and 2
│   │   ├── xag_decomp_kappa_sweep.cpp         # Table 3
│   │   ├── xag_kappa{4,6,10,12}.cpp           # Table 3, other cut sizes
│   │   └── multi_output_mc_check.cpp          # Section 5.1.3
│   ├── test/                                  # unit tests (Catch2)
│   └── include/mockturtle/
│       ├── algorithms/xag_decomp_max_local.hpp     # optimization framework
│       ├── algorithms/acdecomp_max_local.hpp       # ownership-aware ACD
│       ├── algorithms/mc_analyzer.hpp              # BS/FS synthesis and MUX construction
│       ├── algorithms/exact_mc_synthesis_multi.hpp # exact multi-output MC synthesis
│       └── views/ownership_view.hpp                # node ownership
├── MP-SPDZ/                   # MPC framework and evaluation scripts
│   └── tests/                 # unit tests of the evaluation scripts
├── scripts/                   # pipeline, see below
├── results/revision/          # data of Table 3 and Section 5.1.3, see its README
├── Dockerfile
└── LICENSE
```

### Third-party code

- `mockturtle/` is based on [mockturtle](https://github.com/lsils/mockturtle)
  (MIT). The files listed under *Source files* below, the experiments in
  `mockturtle/experiments/` and the tests in `mockturtle/test/` were added for
  this work. The documentation, CI configuration and unit tests of mockturtle
  were removed. `mockturtle/test/catch2/catch.hpp` is Catch2 v2.13.7.
- `MP-SPDZ/` is [MP-SPDZ](https://github.com/data61/MP-SPDZ) at commit
  `58d2739a` (0.4.2 plus 13 commits, BSD 3-Clause). The only change to its
  sources is timing of the oblivious transfers and of the wire-label generation
  in `Yao/YaoGarbler.{h,cpp}`, used for the OT time in Table 4. The evaluation
  scripts (`run_mp_spdz_benchmarks.py`, `summarize_mp_spdz_results.py`,
  `analyze_benchmarks.py`) and the circuits in `Programs/Circuits/` were added.

## Setup

### Requirements

The reference environment is the Docker image built from `Dockerfile`
(`python:3.10-bullseye`, Debian 11), with

| Dependency | Version |
|------------|---------|
| CMake | 3.18 |
| Ninja | 1.10 |
| GCC (synthesis) | 10.2 |
| Clang (MP-SPDZ) | 11.0 |
| Boost | 1.74 |
| GMP | 6.2.1 |
| NTL | 11.4.3 |
| libsodium | 1.0.18 |
| OpenSSL | 1.1.1 |
| Python | 3.10, standard library only |

The numbers in the paper were measured natively on an Apple M1 Max with 32 GB
RAM under macOS, compiled with Apple Clang. The synthesis part needs a C++17
compiler and CMake 3.14 or later.

### Docker

```bash
docker build -t jmc-synthesis .                                          # about 20 min
docker run --rm -it jmc-synthesis bash /artifact/scripts/run_all.sh 5    # quick run
docker run --rm -it jmc-synthesis bash /artifact/scripts/run_all.sh 100  # as in the paper
```

### Native

On Debian or Ubuntu:

```bash
sudo apt-get install -y cmake ninja-build build-essential clang-11 libssl-dev \
  libgmp-dev libboost-dev libboost-thread-dev libsodium-dev libntl-dev \
  libclang-dev automake libtool
```

On macOS, MP-SPDZ needs the Homebrew packages listed in `MP-SPDZ/README.md`.

The optimized circuits are also committed under `MP-SPDZ/Programs/Circuits/`,
so the MPC evaluation can be run without the synthesis.

### Running time

The synthesis experiments are single-threaded. Optimization times on the
machine above, without compilation:

| Step | Time |
|------|------|
| Tables 1 and 2, κ = 8 | 4 min |
| Table 3, κ = 4, 6 and 10 | 4 min |
| Table 3, κ = 8* | 12 min |
| Section 5.1.3, exact check with conflict limit 2000000 | 2 min |

With this conflict limit, one instance of the exact check stays open. Without
a limit it took 7.7 h; this is not needed to reproduce the reported numbers.

## Reproducing the results

The synthesis binaries are run from `mockturtle/build`, which must contain a
copy of `experiments/databases/xag_mc_min_6_db`. The scripts take care of
this.

### Tables 1 and 2

```bash
bash scripts/01_run_synthesis.sh
```

Optimizes both suites until no further improvement is found, writes the
optimized circuits to `experiments/{suite}_benchmarks/jmc_opt_bristol/`, and
generates `results_arith_cp.tex` and `results_crypto_gp.tex` with
`scripts/gen_synthesis_tables.py`. The AND and joint AND counts are
deterministic and match the paper.

### Table 3

κ is a compile-time constant, so each value has its own binary:

```bash
cd mockturtle/build
./experiments/xag_decomp_kappa_sweep 8                   # κ = 8
JMC_CUT_LIMIT=15 ./experiments/xag_decomp_kappa_sweep 8  # κ = 8*, 15 priority cuts per node
./experiments/xag_kappa4 4                               # likewise for 6 and 10
```

Each run prints one `CSV,` line per benchmark with the AND and joint AND
counts, the runtime, and the depth and AND depth before and after
optimization. κ = 10 does not run on `INT-div` and `FP-div`. The data used in
the paper is in `results/revision/kappa_sweep.csv`; the rejected candidates
and the sizes of the accepted cuts mentioned in Section 6.3 are in
`kappa_sweep_rejections.csv` and `accepted_cuts.csv`.

### Table 4

```bash
bash scripts/02_copy_circuits.sh
bash scripts/03_build_mpc.sh
bash scripts/04_run_mpc_eval.sh 100   # or 5 for a quick run
bash scripts/05_analyze_results.sh
```

Writes `MP-SPDZ/results/results_table.tex`, next to the measurements in
`raw_results.csv`. The communication is deterministic. Timings depend on the
machine, so the relative changes are the figures to compare.

### Table 5

Derived from the other tables:

| Column | Source |
|--------|--------|
| ΔEval. inputs | evaluator inputs in Table 4, optimized minus baseline; the evaluator is the party with fewer inputs |
| ΔOT comm. | difference of `OT Comm.` in Table 4, 16 B per additional evaluator input |
| Δ#GGs | AND count of the baseline minus joint AND count after optimization (Tables 1 and 2) |
| ΔGG comm. | difference of `Total Comm.` minus `OT Comm.` in Table 4 |
| ΔNet comm. | difference of `Total Comm.` in Table 4 |
| Δ#ANDs | AND count, optimized minus baseline (Tables 1 and 2) |
| Depth, AND depth, synthesis time | `results/revision/kappa_sweep.csv`, rows with `config` `k8` |

### Section 5.1.3

Compares the per-output database lookup with the exact multi-output optimum:

```bash
cd mockturtle/build
JMC_DUMP_INSTANCES=instances.txt ./experiments/xag_decomp_kappa_sweep 8
./experiments/multi_output_mc_check instances.txt optcheck.csv 2000000
```

The first command writes every bound-set and free-set group, the second solves
each distinct group exactly. The bound-set groups use 476 AND gates against an
optimum of 472 and are optimal in 98.9% of the instances; the free-set groups
use 1320 against 1095. Reference output is in `results/revision/`.

The database has 147,998 spectral classes, and canonicalization stops after
100,000 steps (`exact_spectral_canonization_limit` in `mc_analyzer.hpp`). If
canonicalization stops early or the class is not in the database, the cost
estimation returns -1 and the candidate is discarded. Such functions are never
synthesized, but they can hide possible rewrites; the six-input AND is an
example.

## Tests

```bash
cd mockturtle/build
cmake -DMOCKTURTLE_TEST=ON ..
make run_tests
ctest --output-on-failure

cd ../../MP-SPDZ
python3 -m unittest discover -s tests
```

- `ownership_view.cpp`: ownership of inputs and gates, for the default, per-party
  and per-input assignments and for gates added later.
- `acdecomp_max_local.cpp`: ACD of two-party functions with 6 to 8 inputs; the
  free-set functions depend on the inputs of one party only, and the
  synthesized decomposition implements the function.
- `mc_analyzer.cpp`: MC of small functions from the database; synthesis by
  lookup is correct and uses that number of AND gates.
- `exact_mc_synthesis_multi.cpp`: four groups from
  `results/revision/optcheck_k8.csv` reach their optimum, and one AND gate
  fewer is infeasible.
- `xag_decomp_max_local.cpp`: `Encoder` keeps 53 AND gates and goes from 10 to
  6 joint AND gates, is equivalent to the original circuit (SAT-based check),
  and has 5 + 5 frontier inputs.
- `MP-SPDZ/tests/test_run_mp_spdz_benchmarks.py`: parsing of Bristol headers
  and of the MP-SPDZ logs.

The C++ tests take a few seconds.

## Source files

| Paper | Concept | File | Entity |
|-------|---------|------|--------|
| §4.1.2 | Node ownership, locally derivable and jointly derived nodes | `views/ownership_view.hpp` | `owner_view`, `assign_ownership`, `propogate_ownership_local`, `is_local` |
| §4.1.2 | Frontier, extraction of the joint sub-circuit | `algorithms/xag_decomp_max_local.hpp` | `identify_frontier`, `identify_frontier_and_extract_mpc` |
| §4.1.3 | JMC cost function | `algorithms/xag_decomp_max_local.hpp` | `mc_mpc_cost`, `collect_joint_mc_cost` |
| §4.2, Algorithm 1 | Peephole framework and cut enumeration | `algorithms/xag_decomp_max_local.hpp` | `xag_decomp_max_local_in_place::run`, `run_reverse_topo_each`; κ is `JMC_NUM_VARS`, the priority-cut budget `cut_enum_ps.cut_limit` |
| §5.1 | Joint compute-oriented ACD | `algorithms/acdecomp_max_local.hpp` | `acdecomp_max_local`, `multi_run` |
| §5.1.1, Algorithm 2 | Ownership-aware variable partitioning, free-set functions | `algorithms/acdecomp_max_local.hpp` | `push_into_free_set`, `investigate_decomp`, `compute_decomp` |
| §5.1.2 | Encoding and bound-set functions | `algorithms/acdecomp_max_local.hpp` | `compute_encodings`, `solve_encoding_exact`, `solve_encoding_heuristic`, `create_covering_matrix`, `solve_covering_exact` |
| §5.1.3 | Synthesis by canonicalization and lookup | `algorithms/mc_analyzer.hpp`, `experiments/databases/xag_mc_min_6_db` | `mc_analyzer::build_db`, `get_mc`, `min_mc_resyn`, `min_mc_resyn_mpc` |
| §5.1.3 | Exact multi-output MC | `algorithms/exact_mc_synthesis_multi.hpp`, `experiments/multi_output_mc_check.cpp` | `exact_mc_synthesis_multi` |
| §5.2 | Low-MC multiplexers | `algorithms/mc_analyzer.hpp`, `algorithms/acdecomp_max_local.hpp` | `construct_low_mc_mux`, `construct_mux_tree`, `compute_top_mux` |

Header paths are relative to `mockturtle/include/mockturtle/`, experiment paths
to `mockturtle/`.

## Environment variables

All optional:

| Variable | Effect |
|----------|--------|
| `JMC_DUMP_INSTANCES` | appends every decomposition instance to the given file |
| `JMC_DUMP_TAG` | tag written with each instance and trace line |
| `JMC_TRACE_CUTS` | prints an `ACCEPT,` line with cut size and gain for each rewrite |
| `JMC_CUT_LIMIT` | priority-cut budget of the sweep binaries |

## Troubleshooting

- `xag_mc_min_6_db: No such file or directory`: copy
  `mockturtle/experiments/databases/xag_mc_min_6_db` to the directory the
  binary is run from.
- `yao-party.x: cannot execute binary file`: the binary was built for another
  architecture, run `scripts/03_build_mpc.sh` again.
- Missing headers when building MP-SPDZ: install the packages listed under
  *Native*.
- Port conflicts during the MPC runs: pass `--start-port <N>` to
  `MP-SPDZ/run_mp_spdz_benchmarks.py`.
