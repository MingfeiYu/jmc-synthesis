"""Unit tests for the parsing helpers of run_mp_spdz_benchmarks.py.

Run from the MP-SPDZ directory:
    python3 -m unittest discover -s tests
"""
import sys
import tempfile
import unittest
from pathlib import Path

MP_SPDZ_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(MP_SPDZ_DIR))

import run_mp_spdz_benchmarks as bench  # noqa: E402

# Excerpt of the log written by party 0 for Encoder_opt_5_5.
PARTY0_LOG = """\
Compiler: ./compile.py -B 32 auto_Encoder_opt_5_5
\t\tCommunication took time 0.000721
\t\tMatrix transpose took time 4e-06
Number of AND gates: 6
AND time: 1.2e-05
AND process timer: 8e-06
AND main thread timer: 3.042e-06
AND prepare timer: 0.000000/0.000000
AND wait timer: 0.000000/0.000000
OT time: 0.022559
Wire label time: 2.8e-05
The following benchmarks are including preprocessing (offline phase).
Time = 0.002644 seconds
Data sent = 0.004516 MB in ~12 rounds (party 0 only)
Global data sent = 0.010704 MB (all parties)
"""


class ParseBristolHeaderTest(unittest.TestCase):
    def test_committed_circuit(self):
        path = MP_SPDZ_DIR / "Programs" / "Circuits" / "Encoder_opt_5_5.txt"
        info = bench.parse_bristol_header(path)
        self.assertEqual(info.name, "Encoder_opt_5_5")
        self.assertEqual(info.input_groups, [5, 5])
        self.assertEqual(len(info.output_groups), 1)
        self.assertGreater(info.gates, 0)
        self.assertGreater(info.wires, sum(info.input_groups))

    def test_malformed_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bad.txt"
            path.write_text("3 7\n2 1 1 1\n1 1\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                bench.parse_bristol_header(path)


class ParseLogTextTest(unittest.TestCase):
    def test_party0_log(self):
        out = bench.parse_log_text(PARTY0_LOG)
        self.assertEqual(out["and_gates"], 6)
        self.assertAlmostEqual(out["time_seconds"], 0.002644)
        self.assertAlmostEqual(out["data_sent_mb"], 0.004516)
        self.assertAlmostEqual(out["global_data_sent_mb"], 0.010704)
        self.assertAlmostEqual(out["ot_comm_time_sum"], 0.000721)
        self.assertEqual(out["and_prepare_timer"], "0.000000/0.000000")

    def test_empty_log(self):
        out = bench.parse_log_text("")
        self.assertIsNone(out["and_gates"])
        self.assertIsNone(out["global_data_sent_mb"])
        self.assertIsNone(out["ot_comm_time_sum"])


class SplitBenchmarkVariantTest(unittest.TestCase):
    def test_variants(self):
        self.assertEqual(bench.split_benchmark_variant("Encoder_opt_5_5"), ("Encoder", "opt"))
        self.assertEqual(bench.split_benchmark_variant("Encoder_16_16"), ("Encoder_16_16", "orig"))


if __name__ == "__main__":
    unittest.main()
