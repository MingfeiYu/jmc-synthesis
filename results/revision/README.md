# Cut-size sweep and multi-output optimality check

Data behind §5.1.3 and §6.3 of the paper, produced with the experiments in
`mockturtle/experiments` (run from the build directory, which must contain
`xag_mc_min_6_db`).

## Cut-size sweep

    ./experiments/xag_decomp_kappa_sweep 8           # kappa = 8
    JMC_CUT_LIMIT=15 ./experiments/xag_decomp_kappa_sweep 8   # kappa = 8*
    ./experiments/xag_kappa4 4                       # likewise for 6 and 10

- `kappa_sweep.csv`: one row per benchmark and configuration, with the AND and
  joint AND counts before and after optimization, the runtime, and the total
  and multiplicative depth. Source of Table 3 and of the depth columns of
  Table 5.
- `kappa_sweep_stats.csv`: decomposition attempts, successes and committed
  rewrites per optimization round.
- `kappa_sweep_rejections.csv`: candidates rejected because a bound-set
  function exceeds the six-variable limit of the synthesis database.
- `accepted_cuts.csv`: size distribution of the committed rewrites on
  `LiteMIPS`, obtained with `JMC_TRACE_CUTS=1`.

`kappa = 10` is not supported for `INT-div` and `FP-div`, whose truth tables
exceed the width handled by the decomposition engine.

## Multi-output optimality check

    JMC_DUMP_INSTANCES=instances_k8.txt ./experiments/xag_decomp_kappa_sweep 8
    ./experiments/multi_output_mc_check instances_k8.txt optcheck_k8.csv 2000000

- `instances_k8.txt`: every free-set and bound-set group encountered at
  `kappa = 8`, in the format described in `multi_output_mc_check.cpp`.
- `optcheck_k8.csv`: per instance, the AND count of the database lookup, the
  single-output lower bound and the exact multi-output optimum.

Aggregated over the distinct instances: the bound-set groups use 476 AND gates
against an optimum of 472 and are exactly optimal in 98.9% of the cases, while
the free-set groups use 1320 against 1095 and are optimal in 90.9% of the
cases. The single instance left `unknown` under the conflict limit of 2000000
was solved separately without a limit, giving an optimum of four AND gates.
