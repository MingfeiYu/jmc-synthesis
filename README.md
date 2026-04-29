# Artifact: [Paper Title]

**[Author names]**  
CCS 2026 (Paper 5220)

---

## Overview

This artifact accompanies the paper on synthesizing efficient Boolean circuits
for garbled-circuit-based multi-party computation (MPC).

The artifact has two components:

| Component | Directory | Purpose |
|-----------|-----------|---------|
| Circuit synthesis | `mockturtle/` | C++ tool that optimizes Boolean circuits and writes Bristol-format output |
| MPC evaluation | `MP-SPDZ/` | Runs the optimized and baseline circuits under Yao's garbled-circuit protocol; measures timing and communication |

**Benchmark suites evaluated:**
- `arith_cf` — arithmetic/control-flow circuits: INT-add, INT-div, INT-mul, FP-add, FP-div, FP-mul, FP-eq
- `crypto_gp` — cryptographic/general-purpose circuits: AES-128, SHA3, Hamming, Encoder, LiteMIPS

---

## Artifact Structure

```
ccs2026_artifact/
├── mockturtle/          # Circuit synthesis tool
│   ├── experiments/
│   │   ├── arith_cf_benchmarks/   # Verilog inputs + baseline Bristol circuits
│   │   │   └── jmc_opt_bristol/   # Optimized circuits (written by synthesis)
│   │   ├── crypto_gp_benchmarks/  # Verilog inputs + baseline Bristol circuits
│   │   │   └── jmc_opt_bristol/   # Optimized circuits (written by synthesis)
│   │   ├── databases/
│   │   │   └── xag_mc_min_6_db    # Precomputed MC lookup table (50 MB)
│   │   └── xag_decomp_max_local.cpp  # Main experiment source
│   └── include/mockturtle/algorithms/
│       └── xag_decomp_max_local.hpp  # Core algorithm
│
├── MP-SPDZ/                       # MPC framework (Yao protocol)
│   ├── Programs/Circuits/         # Bristol circuits used for MPC runs
│   ├── results/                   # CSV results and per-run logs
│   ├── run_mp_spdz_benchmarks.py  # Automates compile+run for all circuits
│   ├── summarize_mp_spdz_results.py
│   └── analyze_benchmarks.py      # Prints baseline-vs-opt comparison table
│
├── scripts/
│   ├── 01_run_synthesis.sh        # Build mockturtle and run synthesis
│   ├── 02_copy_circuits.sh        # Copy circuits to MP-SPDZ/Programs/Circuits/
│   ├── 03_build_mpc.sh            # Build MP-SPDZ yao-party.x
│   ├── 04_run_mpc_eval.sh         # Run garbled-circuit benchmarks
│   ├── 05_analyze_results.sh      # Print comparison table + write LaTeX
│   └── run_all.sh                 # Run all steps end-to-end
│
├── Dockerfile                     # Combined build environment (recommended)
└── README.md                      # This file
```

---

## Option A — Docker (Recommended)

Docker eliminates dependency management and guarantees a reproducible
environment.  All steps run inside a single container.

### Prerequisites
- Docker ≥ 20.10

### Build the image

```bash
docker build -t ccs2026-artifact .
```

> Building the image downloads dependencies and compiles both tools.
> **Expected time: 20–40 minutes** on first build (MP-SPDZ compilation dominates).

### Run the full pipeline

```bash
# Quick evaluation (5 repetitions per circuit, 3-5 min)
docker run --rm -it ccs2026-artifact bash /artifact/scripts/run_all.sh 5

# Full results matching the paper (100 repetitions, 1-2 hours)
docker run --rm -it ccs2026-artifact bash /artifact/scripts/run_all.sh 100
```

### Persist results on the host

```bash
docker run --rm -it \
  -v "$(pwd)/results:/artifact/MP-SPDZ/results" \
  ccs2026-artifact bash /artifact/scripts/run_all.sh 5
```

---

## Option B — Native Installation

### System requirements
- **OS:** Linux (tested on Ubuntu 22.04) or macOS 13+
- **RAM:** ≥ 8 GB recommended
- **Disk:** ~3 GB (source + build artifacts)

### Synthesis tool dependencies
| Package | Minimum version |
|---------|----------------|
| CMake   | 3.14 |
| C++ compiler | GCC 9 / Clang 11 with C++17 |
| Ninja   | any recent |

Ubuntu/Debian:
```bash
sudo apt-get install -y cmake ninja-build build-essential
```

macOS (Homebrew):
```bash
brew install cmake ninja
```

### MP-SPDZ dependencies

Ubuntu/Debian:
```bash
sudo apt-get install -y \
  build-essential clang-11 libssl-dev libgmp-dev \
  libboost-dev libboost-thread-dev libsodium-dev \
  libntl-dev libclang-dev automake libtool
```

### Step 1 — Synthesize circuits

```bash
bash scripts/01_run_synthesis.sh
```

This builds the `xag_decomp_max_local` binary, runs the optimization over both
benchmark suites, and writes the resulting Bristol `.txt` files to
`experiments/{suite}_benchmarks/jmc_opt_bristol/`.

**Expected output** (truncated):
```
[i] Processing FP-add ...
[m] Finished the 1st round of optimization
...
[i] Identifying the new input frontier...
[i] 125 input bits required from the 1st party
[i] 124 input bits required from the 2nd party
```

**Expected time:** 10–20 minutes total for both suites.

### Step 2 — Copy circuits to MP-SPDZ

```bash
bash scripts/02_copy_circuits.sh
```

Copies all baseline and optimized `.txt` circuits to `MP-SPDZ/Programs/Circuits/`.

### Step 3 — Build MP-SPDZ

```bash
bash scripts/03_build_mpc.sh
```

Compiles `yao-party.x`.  **Expected time:** 5 minutes on first build.

### Step 4 — Run MPC benchmarks

```bash
# Quick (5 reps, 3-5 min)
bash scripts/04_run_mpc_eval.sh 5

# Full (100 reps, 1-2 hour)
bash scripts/04_run_mpc_eval.sh 100
```

Excluded by default (trivial): `INT-add`, `Hamming`, `SHA3`.
Pass `--only <name>` via `run_mp_spdz_benchmarks.py` directly to run a subset.

**Expected output per circuit:**
```
[i] AES_128_128: input groups = [128, 128]
[i] AES_opt_1408_128: input groups = [1408, 128]
...
```

Results are appended to `MP-SPDZ/results/raw_results.csv`.

### Step 5 — Analyze results

```bash
bash scripts/05_analyze_results.sh
```

Prints a table like:

```
Benchmark         | AND gates (base / opt) | AND time (base / opt) | Data (base / opt MB)
AES               |   6400 / 1408          |  1.83s / 1.21s        |  ...
FP-add            |    ...
...
```

Also writes `results/summary_results.csv` and `results/results_table.tex`.

> **Note:** Absolute timing numbers depend on the machine.
> The important quantities to verify are AND-gate counts (deterministic) and
> the relative improvement in communication/timing (should match the paper's
> percentage reductions within ~5%).

---

## Pre-generated Circuits

The optimized Bristol circuits have already been generated and committed to the
repository in `MP-SPDZ/Programs/Circuits/`.  If you only want to run the MPC
evaluation without re-running synthesis, skip steps 01–02 and go directly to
step 03.

---

## Troubleshooting

**`cmake` cannot find a C++17 compiler**
Ensure your compiler supports `-std=c++17`.  On Ubuntu, install
`gcc` ≥ 9 or `clang` ≥ 7.

**MP-SPDZ build fails with missing headers**
Make sure all `-dev` packages listed under *MP-SPDZ dependencies* are installed.
On Ubuntu 22.04, `clang-11` can be installed via the LLVM apt repository if not
available in the default repos.

**`yao-party.x: cannot execute binary file`**
The binary in the repository was compiled for the development machine's
architecture.  Run step 03 to rebuild it for your platform.

**`xag_mc_min_6_db: No such file or directory`**
The script `01_run_synthesis.sh` copies the database automatically.
If running the binary manually, copy
`mockturtle/experiments/databases/xag_mc_min_6_db`
into the directory from which you invoke the binary.

**Port conflicts in step 04**
If two benchmark runs overlap on the same port, pass `--start-port <N>` to
`run_mp_spdz_benchmarks.py` with a different starting port number.
