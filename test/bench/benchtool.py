#!/usr/bin/env python3
"""Cross-platform bench harness for iot-broker.

Lifecycle handled in-process:
    1. locate iot-broker binary (default: ./build/iot-broker)
    2. spawn it with the requested --port/--workers
    3. poll TCP until the listen socket is ready
    4. fork N concurrent ping-pong workers against it
    5. tear the broker down cleanly (SIGTERM, then SIGKILL)

Pass criteria:
  - zero wrong replies
  - failure rate <= failure_threshold_pct (default 2%)

Exit code: 0 = pass, 1 = fail (suitable for CI / automated targets).

Usage:
    python3 tools/benchtool.py
    python3 tools/benchtool.py --clients 1000 --port 7777 --workers 8
"""

from __future__ import annotations

import argparse
import os
import signal
import socket
import subprocess
import sys
import threading
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.dirname(_HERE)

DEFAULT_BROKER = os.path.join(_PROJECT_ROOT, "build", "iot-broker")
DEFAULT_PORT = 65183
DEFAULT_WORKERS = 4
DEFAULT_CLIENTS = 500
CONNECT_TIMEOUT = 5.0
RECV_TIMEOUT = 5.0
STARTUP_TIMEOUT = 5.0
SHUTDOWN_TIMEOUT = 3.0
FAILURE_THRESHOLD_PCT = 2.0

PAYLOAD = b"ping-from-bench\n"


class BrokerHandle:
    """Owns the iot-broker subprocess for the duration of the benchmark."""

    def __init__(self, *, cmd: list[str], suppress_output: bool = True):
        self.cmd = cmd
        self._devnull = open(os.devnull, "w") if suppress_output else None
        self.proc: subprocess.Popen | None = None

    def __enter__(self) -> "BrokerHandle":
        self.proc = subprocess.Popen(
            self.cmd,
            cwd=_PROJECT_ROOT,
            stdout=self._devnull if self._devnull else subprocess.PIPE,
            stderr=subprocess.STDOUT,
            preexec_fn=os.setsid if hasattr(os, "setsid") else None,
        )
        self._await_port()
        return self

    def __exit__(self, exc_type, exc, tb):
        if self.proc is not None:
            self._stop()
        if self._devnull is not None:
            try:
                self._devnull.close()
            except Exception:
                pass

    def _await_port(self):
        deadline = time.monotonic() + STARTUP_TIMEOUT
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError(
                    f"iot-broker exited early (rc={self.proc.returncode})"
                )
            try:
                with socket.create_connection(
                    ("127.0.0.1", self._port()), timeout=0.4
                ):
                    return
            except OSError:
                time.sleep(0.1)
        raise RuntimeError("broker did not become ready in time")

    def _port(self) -> int:
        for i, tok in enumerate(self.cmd):
            if tok == "--port" and i + 1 < len(self.cmd):
                return int(self.cmd[i + 1])
        return DEFAULT_PORT

    def _stop(self):
        try:
            self.proc.send_signal(signal.SIGTERM)
            self.proc.wait(timeout=SHUTDOWN_TIMEOUT)
            return
        except subprocess.TimeoutExpired:
            pass
        except Exception:
            return
        try:
            if hasattr(os, "killpg"):
                os.killpg(os.getpgid(self.proc.pid), signal.SIGKILL)
            else:
                self.proc.kill()
            self.proc.wait(timeout=2)
        except Exception:
            pass


def worker(host: str, port: int, idx: int, failures: list[int]):
    try:
        s = socket.create_connection((host, port), timeout=CONNECT_TIMEOUT)
    except Exception as e:
        failures[idx] = 1
        print(f"  [client {idx}] connect failed: {e}", file=sys.stderr)
        return
    try:
        s.sendall(PAYLOAD)
        s.settimeout(RECV_TIMEOUT)
        got = bytearray()
        while len(got) < len(PAYLOAD):
            chunk = s.recv(len(PAYLOAD) - len(got))
            if not chunk:
                break
            got += chunk
        if bytes(got) != PAYLOAD:
            failures[idx] = 1
            print(f"  [client {idx}] payload mismatch: got {got!r}", file=sys.stderr)
    except Exception as e:
        failures[idx] = 1
        print(f"  [client {idx}] recv failed: {e}", file=sys.stderr)
    finally:
        try:
            s.close()
        except Exception:
            pass


def run_bench(*, broker: str, port: int, workers: int, clients: int) -> bool:
    cmd = [broker, "--port", str(port), "--workers", str(workers)]
    print(
        f"bench: launching {cmd}\n"
        f"       clients={clients} failure_threshold={FAILURE_THRESHOLD_PCT}%"
    )
    failures = [0] * clients
    start = time.monotonic()
    with BrokerHandle(cmd=cmd):
        threads = [
            threading.Thread(
                target=worker,
                args=("127.0.0.1", port, i, failures),
                daemon=True,
            )
            for i in range(clients)
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=CONNECT_TIMEOUT + RECV_TIMEOUT + 1)
    elapsed = time.monotonic() - start

    failed = sum(failures)
    rate = (failed / clients) * 100 if clients else 0.0
    print(f"result: {failed}/{clients} failed ({rate:.2f}%) in {elapsed:.2f}s")
    ok = rate <= FAILURE_THRESHOLD_PCT
    print("status: PASS" if ok else "status: FAIL")
    return ok


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Start iot-broker, ping-pong N clients, report the failure rate."
    )
    p.add_argument("--broker", default=DEFAULT_BROKER, help="path to iot-broker binary")
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    p.add_argument("--clients", type=int, default=DEFAULT_CLIENTS)
    p.add_argument(
        "--threshold",
        type=float,
        default=FAILURE_THRESHOLD_PCT,
        help="max allowed failure percentage",
    )
    p.add_argument(
        "--no-restart",
        action="store_true",
        help="connect to an already-running broker on --port (don't spawn one)",
    )
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    global FAILURE_THRESHOLD_PCT
    FAILURE_THRESHOLD_PCT = args.threshold

    if not args.no_restart and not os.path.isfile(args.broker):
        print(f"error: broker not found: {args.broker}", file=sys.stderr)
        return 1

    if args.no_restart:
        failures = [0] * args.clients
        start = time.monotonic()
        threads = [
            threading.Thread(
                target=worker,
                args=("127.0.0.1", args.port, i, failures),
                daemon=True,
            )
            for i in range(args.clients)
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=CONNECT_TIMEOUT + RECV_TIMEOUT + 1)
        elapsed = time.monotonic() - start
        failed = sum(failures)
        rate = (failed / args.clients) * 100 if args.clients else 0.0
        print(
            f"result (external broker): {failed}/{args.clients} failed "
            f"({rate:.2f}%) in {elapsed:.2f}s"
        )
        return 0 if rate <= FAILURE_THRESHOLD_PCT else 1

    ok = run_bench(
        broker=args.broker,
        port=args.port,
        workers=args.workers,
        clients=args.clients,
    )
    return 0 if ok else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("bench interrupted", file=sys.stderr)
        sys.exit(130)
    except RuntimeError as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
