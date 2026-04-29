#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, hashlib, random, re, shutil, socket, subprocess, sys, time
from dataclasses import dataclass, asdict
from pathlib import Path

HEADER_RE = re.compile(r"^\s*(\d+)\s+(\d+)\s*$")
TIME_RE = re.compile(r"Time\s*=\s*([0-9.eE+-]+)\s+seconds")
DATA_SENT_RE = re.compile(r"Data sent\s*=\s*([0-9.eE+-]+)\s+MB")
GLOBAL_DATA_RE = re.compile(r"Global data sent\s*=\s*([0-9.eE+-]+)\s+MB")
AND_GATES_RE = re.compile(r"Number of AND gates:\s*([0-9]+)")
AND_TIME_RE = re.compile(r"AND time:\s*([0-9.eE+-]+)")
AND_PROC_RE = re.compile(r"AND process timer:\s*([0-9.eE+-]+)")
AND_MAIN_RE = re.compile(r"AND main thread timer:\s*([0-9.eE+-]+)")
AND_PREP_RE = re.compile(r"AND prepare timer:\s*([0-9.eE+-/]+)")
AND_WAIT_RE = re.compile(r"AND wait timer:\s*([0-9.eE+-/]+)")
XOR_TIME_RE = re.compile(r"XOR time:\s*([0-9.eE+-]+)")
VM_ROUNDS_RE = re.compile(r"([0-9]+)\s+virtual machine rounds")
OT_COMM_TIME_RE = re.compile(r"Communication took time\s+([0-9.eE+-]+)")
OT_MATRIX_RE = re.compile(r"Matrix transpose took time\s+([0-9.eE+-]+)")

@dataclass
class CircuitInfo:
    path: Path
    name: str
    input_groups: list[int]
    output_groups: list[int]
    gates: int
    wires: int

@dataclass
class RunMetrics:
    benchmark: str
    base_benchmark: str
    variant: str
    repetition: int
    port: int
    compile_ok: bool
    run_ok: bool
    compile_returncode: int
    p0_returncode: int
    p1_returncode: int
    compile_seconds: float
    run_seconds: float
    time_seconds: float | None
    data_sent_mb_p0: float | None
    data_sent_mb_p1: float | None
    global_data_sent_mb: float | None
    and_gates: int | None
    and_time: float | None
    and_process_time: float | None
    and_main_thread_time: float | None
    and_prepare_timer: str | None
    and_wait_timer: str | None
    xor_time: float | None
    vm_rounds: int | None
    ot_comm_time_sum: float | None
    ot_matrix_time_sum: float | None
    lower_level_lines_p0: str
    lower_level_lines_p1: str
    compile_log: str
    p0_log: str
    p1_log: str

def parse_bristol_header(path: Path) -> CircuitInfo:
    with path.open("r", encoding="utf-8") as f:
        first = f.readline().strip()
        m1 = HEADER_RE.match(first)
        if not m1:
            raise ValueError(f"Invalid first header line in {path}: {first!r}")
        gates, wires = int(m1.group(1)), int(m1.group(2))
        second = f.readline().strip().split()
        n_inputs = int(second[0]); input_groups = [int(x) for x in second[1:]]
        if len(input_groups) != n_inputs:
            raise ValueError(f"Invalid input-group line in {path}: {second}")
        third = f.readline().strip().split()
        n_outputs = int(third[0]); output_groups = [int(x) for x in third[1:]]
        if len(output_groups) != n_outputs:
            raise ValueError(f"Invalid output-group line in {path}: {third}")
    return CircuitInfo(path, path.stem, input_groups, output_groups, gates, wires)

def generate_wrapper_text(info: CircuitInfo) -> str:
    lines = ["from circuit import Circuit", "from Compiler.GC.types import sbitvec", "", f"ckt = Circuit({info.name!r})", ""]
    for i, width in enumerate(info.input_groups):
        lines.append(f"sbv{i} = sbitvec.get_type({width})")
        lines.append(f"in{i} = sbv{i}.get_input_from({i})")
        lines.append("")
    args = ", ".join(f"in{i}" for i in range(len(info.input_groups)))
    lines.append(f"out = ckt({args})")
    lines.append("out.reveal().print_reg()")
    lines.append("")
    return "\n".join(lines)

def deterministic_bits(n: int, seed_text: str) -> list[int]:
    seed = int(hashlib.sha256(seed_text.encode("utf-8")).hexdigest(), 16) % (2**32)
    rng = random.Random(seed)
    return [rng.randint(0, 1) for _ in range(n)]

def write_input_file(path: Path, bits):
    with path.open("w", encoding="utf-8") as f:
        for b in bits:
            f.write(f"{int(b)}\n")

def free_port(start_port: int) -> int:
    port = start_port
    while port < 65535:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(("127.0.0.1", port))
                return port
            except OSError:
                port += 1
    raise RuntimeError("No free port found")

def run_cmd(cmd, cwd: Path, log_path: Path, timeout: int | None = None):
    start = time.time()
    with log_path.open("w", encoding="utf-8") as log:
        proc = subprocess.run(cmd, cwd=str(cwd), stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
    return proc.returncode, time.time() - start

def launch_two_party_yao(mp_spdz_dir: Path, program_name: str, port: int, p0_log: Path, p1_log: Path, timeout: int):
    start = time.time()
    with p0_log.open("w", encoding="utf-8") as log0, p1_log.open("w", encoding="utf-8") as log1:
        p0 = subprocess.Popen(["./yao-party.x", "-p", "0", "-pn", str(port), program_name], cwd=str(mp_spdz_dir), stdout=log0, stderr=subprocess.STDOUT)
        time.sleep(1.0)
        p1 = subprocess.Popen(["./yao-party.x", "-p", "1", "-h", "127.0.0.1", "-pn", str(port), program_name], cwd=str(mp_spdz_dir), stdout=log1, stderr=subprocess.STDOUT)
        try:
            p1_rc = p1.wait(timeout=timeout)
            p0_rc = p0.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            for proc in (p0, p1):
                if proc.poll() is None:
                    proc.kill()
            raise
    return p0_rc, p1_rc, time.time() - start

def parse_log_text(text: str) -> dict:
    out = {"time_seconds": None, "data_sent_mb": None, "global_data_sent_mb": None, "and_gates": None,
           "and_time": None, "and_process_time": None, "and_main_thread_time": None,
           "and_prepare_timer": None, "and_wait_timer": None, "xor_time": None, "vm_rounds": None,
           "ot_comm_time_sum": 0.0, "ot_matrix_time_sum": 0.0}
    for regex, key, cast in [
        (TIME_RE, "time_seconds", float),
        (DATA_SENT_RE, "data_sent_mb", float),
        (GLOBAL_DATA_RE, "global_data_sent_mb", float),
        (AND_GATES_RE, "and_gates", int),
        (AND_TIME_RE, "and_time", float),
        (AND_PROC_RE, "and_process_time", float),
        (AND_MAIN_RE, "and_main_thread_time", float),
        (AND_PREP_RE, "and_prepare_timer", str),
        (AND_WAIT_RE, "and_wait_timer", str),
        (XOR_TIME_RE, "xor_time", float),
        (VM_ROUNDS_RE, "vm_rounds", int),
    ]:
        m = regex.search(text)
        if m:
            out[key] = cast(m.group(1))
    for m in OT_COMM_TIME_RE.finditer(text):
        out["ot_comm_time_sum"] += float(m.group(1))
    for m in OT_MATRIX_RE.finditer(text):
        out["ot_matrix_time_sum"] += float(m.group(1))
    if out["ot_comm_time_sum"] == 0.0:
        out["ot_comm_time_sum"] = None
    if out["ot_matrix_time_sum"] == 0.0:
        out["ot_matrix_time_sum"] = None
    return out

def extract_lower_level_lines(text: str) -> str:
    keys = ("Receiving directly", "Receiving one-to-one", "Sending directly", "Sending one-to-one",
            "Sending by number", "Communication took time", "Matrix transpose took time",
            "Number of AND gates", "AND time:", "XOR time:")
    return " || ".join(line.strip() for line in text.splitlines() if any(k in line for k in keys))

def parse_optional_float(x):
    if x is None or x == "" or str(x).lower() == "none":
        return None
    return float(x)

def split_benchmark_variant(name: str):
    if "_opt_" in name:
        return name.split("_opt_", 1)[0], "opt"
    return name, "orig"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mp-spdz", required=True)
    ap.add_argument("--circuits-dir", required=True)
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--exclude", nargs="*", default=["INT-add", "Hamming", "SHA3"],
                    help="base benchmark names to skip (default: INT-add Hamming SHA3)")
    ap.add_argument("--program-prefix", default="auto_")
    ap.add_argument("--bitlen", type=int, default=32)
    ap.add_argument("--repetitions", type=int, default=100)
    ap.add_argument("--start-port", type=int, default=5001)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--results-csv", default="results/raw_results.csv")
    ap.add_argument("--keep-generated", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    mp_spdz = Path(args.mp_spdz).resolve()
    circuits_dir = Path(args.circuits_dir).resolve()
    results_csv = Path(args.results_csv).resolve()
    results_csv.parent.mkdir(parents=True, exist_ok=True)
    logs_dir = results_csv.parent / "logs"; logs_dir.mkdir(parents=True, exist_ok=True)
    programs_source = mp_spdz / "Programs" / "Source"
    programs_circuits = mp_spdz / "Programs" / "Circuits"
    player_data = mp_spdz / "Player-Data"
    programs_source.mkdir(parents=True, exist_ok=True)
    programs_circuits.mkdir(parents=True, exist_ok=True)
    player_data.mkdir(parents=True, exist_ok=True)

    paths = sorted(circuits_dir.glob("*.txt"))
    if args.only:
        wanted = set(args.only)
        paths = [p for p in paths if p.stem in wanted or p.name in wanted]
    if args.exclude:
        excluded = set(args.exclude)
        paths = [p for p in paths if not any(p.stem == e or p.stem.startswith(e + "_") or p.stem.startswith(e + "_opt_") for e in excluded)]
    if not paths:
        raise SystemExit(f"No Bristol circuits found in {circuits_dir}")

    fieldnames = list(RunMetrics.__annotations__.keys())
    write_header = not results_csv.exists()

    with results_csv.open("a", newline="", encoding="utf-8") as csvf:
        writer = csv.DictWriter(csvf, fieldnames=fieldnames)
        if write_header:
            writer.writeheader()
        for circ_path in paths:
            info = parse_bristol_header(circ_path)
            if args.verbose:
                print(f"[i] {info.name}: input groups = {info.input_groups}")
            target_circuit = programs_circuits / circ_path.name
            if circ_path.resolve() != target_circuit.resolve():
                shutil.copy2(circ_path, target_circuit)
            program_name = f"{args.program_prefix}{info.name}"
            mpc_path = programs_source / f"{program_name}.mpc"
            mpc_path.write_text(generate_wrapper_text(info), encoding="utf-8")

            compile_log = logs_dir / f"{info.name}.compile.log"
            compile_rc, compile_secs = run_cmd(
                ["./compile.py", "-B", str(args.bitlen), program_name],
                mp_spdz, compile_log, args.timeout)
            if compile_rc != 0:
                base_benchmark, variant = split_benchmark_variant(info.name)
                row = RunMetrics(info.name, base_benchmark, variant, 0, -1, False, False, compile_rc, -1, -1, compile_secs, 0.0,
                                 None, None, None, None, None, None, None, None, None, None, None, None, None, None,
                                 "", "", str(compile_log), "", "")
                writer.writerow(asdict(row)); csvf.flush()
                if not args.keep_generated:
                    try: mpc_path.unlink()
                    except FileNotFoundError: pass
                continue

            for rep in range(1, args.repetitions + 1):
                for party, width in enumerate(info.input_groups):
                    bits = deterministic_bits(width, f"{info.name}|party{party}|rep{rep}")
                    write_input_file(player_data / f"Input-P{party}-0", bits)

                p0_log = logs_dir / f"{info.name}.rep{rep}.p0.log"
                p1_log = logs_dir / f"{info.name}.rep{rep}.p1.log"

                port = free_port(args.start_port)
                try:
                    p0_rc, p1_rc, run_secs = launch_two_party_yao(mp_spdz, program_name, port, p0_log, p1_log, args.timeout)
                    run_ok = (p0_rc == 0 and p1_rc == 0)
                except subprocess.TimeoutExpired:
                    p0_rc = p1_rc = -999; run_secs = float(args.timeout); run_ok = False

                p0_text = p0_log.read_text(encoding="utf-8", errors="replace") if p0_log.exists() else ""
                p1_text = p1_log.read_text(encoding="utf-8", errors="replace") if p1_log.exists() else ""
                p0 = parse_log_text(p0_text); p1 = parse_log_text(p1_text)
                global_data = p0["global_data_sent_mb"] if p0["global_data_sent_mb"] is not None else p1["global_data_sent_mb"]
                time_seconds = p0["time_seconds"] if p0["time_seconds"] is not None else p1["time_seconds"]
                vm_rounds = p0["vm_rounds"] if p0["vm_rounds"] is not None else p1["vm_rounds"]
                vals = [x for x in (p0["ot_comm_time_sum"], p1["ot_comm_time_sum"]) if x is not None]
                ot_comm_sum = sum(vals) if vals else None
                vals2 = [x for x in (p0["ot_matrix_time_sum"], p1["ot_matrix_time_sum"]) if x is not None]
                ot_matrix_sum = sum(vals2) if vals2 else None

                base_benchmark, variant = split_benchmark_variant(info.name)
                row = RunMetrics(
                    benchmark=info.name, base_benchmark=base_benchmark, variant=variant,
                    repetition=rep, port=port, compile_ok=True, run_ok=run_ok,
                    compile_returncode=compile_rc, p0_returncode=p0_rc, p1_returncode=p1_rc,
                    compile_seconds=compile_secs, run_seconds=run_secs, time_seconds=time_seconds,
                    data_sent_mb_p0=p0["data_sent_mb"], data_sent_mb_p1=p1["data_sent_mb"],
                    global_data_sent_mb=global_data, and_gates=p0["and_gates"], and_time=p0["and_time"],
                    and_process_time=p0["and_process_time"], and_main_thread_time=p0["and_main_thread_time"],
                    and_prepare_timer=p0["and_prepare_timer"], and_wait_timer=p0["and_wait_timer"],
                    xor_time=p0["xor_time"], vm_rounds=vm_rounds, ot_comm_time_sum=ot_comm_sum,
                    ot_matrix_time_sum=ot_matrix_sum, lower_level_lines_p0=extract_lower_level_lines(p0_text),
                    lower_level_lines_p1=extract_lower_level_lines(p1_text), compile_log=str(compile_log),
                    p0_log=str(p0_log), p1_log=str(p1_log)
                )
                writer.writerow(asdict(row)); csvf.flush()

            if not args.keep_generated:
                try: mpc_path.unlink()
                except FileNotFoundError: pass

if __name__ == "__main__":
    main()
