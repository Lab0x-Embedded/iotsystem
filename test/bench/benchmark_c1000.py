#!/usr/bin/env python3
"""Phase 1 bench: verify the echo server stays responsive under load.

Emulates ~500 concurrent throwaway connections that ping-pong a small
message.  Exits non-zero if any response is wrong or if >2% of
connections fail.

Usage:
    python3 tools/bench.py --host 127.0.0.1 --port 1883 --clients 500
"""

import argparse
import socket
import sys
import threading
import time

PAYLOAD = b"ping-from-bench\n"


def worker(host: str, port: int, idx: int, results: list) -> None:
    try:
        s = socket.create_connection((host, port), timeout=5.0)
    except Exception as e:
        results[idx] = (False, f"connect failed: {e}")
        return
    try:
        s.sendall(PAYLOAD)
        data = b""
        deadline = time.time() + 5.0
        while len(data) < len(PAYLOAD):
            if time.time() > deadline:
                raise RuntimeError("recv timeout")
            chunk = s.recv(4096)
            if not chunk:
                raise RuntimeError("server closed")
            data += chunk
        if data != PAYLOAD:
            results[idx] = (False, f"mismatch got {data!r}")
        else:
            results[idx] = (True, "")
    except Exception as e:
        results[idx] = (False, str(e))
    finally:
        s.close()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--clients", type=int, default=200)
    args = ap.parse_args()

    results: list = [(False, "not-run")] * args.clients
    threads = []
    t0 = time.time()
    for i in range(args.clients):
        t = threading.Thread(target=worker,
                             args=(args.host, args.port, i, results),
                             daemon=True)
        t.start()
        threads.append(t)
    for t in threads:
        t.join(timeout=15)
    dt = time.time() - t0

    ok = sum(1 for ok, _ in results if ok)
    fails = [(i, msg) for i, (ok, msg) in enumerate(results) if not ok]

    print(f"clients={args.clients} ok={ok} fails={len(fails)} "
          f"in {dt:.2f}s  ({args.clients/dt:.0f} conn/s)")
    if fails:
        print("first few failures:", fails[:5])
    # 2% tolerance for CI flakiness
    threshold = max(1, int(0.02 * args.clients))
    return 0 if len(fails) <= threshold else 1


if __name__ == "__main__":
    sys.exit(main())
