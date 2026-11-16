#!/usr/bin/env python3
"""Tiny local server: serves ocean-planner.html and runs the C++ engine on POST /allocate."""
import json, os, subprocess, sys, tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "marinealloc.cpp")
EXE = os.path.join(HERE, "marinealloc" + (".exe" if os.name == "nt" else ""))
PAGE = os.path.join(HERE, "ocean-planner.html")
PORT = 8000


def build():
    """Compile the C++ engine if the binary is missing or older than the source."""
    if os.path.exists(EXE) and os.path.getmtime(EXE) >= os.path.getmtime(SRC):
        return
    print("Compiling marinealloc.cpp ...")
    r = subprocess.run(["g++", "-std=c++17", "-O2", SRC, "-o", EXE], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr)
        sys.exit("Compile failed. Is g++ installed?")


def one_line(s, default):
    return " ".join(str(s).split()) or default


def to_input(p):
    """Turn the page's JSON into the plain-text format marinealloc.cpp reads (1-based coords)."""
    grid = p["grid"]
    R, C = len(grid), len(grid[0])
    out = [f"{R} {C} {float(p['cap'])}"]
    out += [" ".join(str(float(v)) for v in row) for row in grid]
    out.append(str(len(p["bans"])))
    for b in p["bans"]:
        out.append(f"{int(b['s'])} {int(b['e'])}")
        out.append(one_line(b["reason"], "Ban"))
    out.append(str(len(p["fleets"])))
    for f in p["fleets"]:
        out.append(one_line(f["id"], "REQ"))
        out.append(one_line(f["name"], "Fleet"))
        out.append(f"{int(f['r1'])+1} {int(f['c1'])+1} {int(f['r2'])+1} {int(f['c2'])+1} "
                   f"{int(f['d1'])} {int(f['d2'])} {float(f['tons'])} {int(f['urg'])}")
    return "\n".join(out) + "\n"


def run_engine(payload):
    with tempfile.TemporaryDirectory() as d:
        inp, outp = os.path.join(d, "in.txt"), os.path.join(d, "out.json")
        with open(inp, "w") as fh:
            fh.write(to_input(payload))
        r = subprocess.run([EXE, "--input", inp, "--output", outp],
                           capture_output=True, text=True, timeout=30)
        if r.returncode != 0:
            raise RuntimeError(r.stderr.strip() or "C++ engine failed")
        with open(outp) as fh:
            return fh.read()


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, body, ctype="application/json"):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path in ("/", "/ocean-planner.html"):
            with open(PAGE, "rb") as fh:
                self._send(200, fh.read(), "text/html; charset=utf-8")
        else:
            self._send(404, json.dumps({"error": "Not found"}))

    def do_POST(self):
        if self.path != "/allocate":
            return self._send(404, json.dumps({"error": "Not found"}))
        try:
            n = int(self.headers.get("Content-Length", 0))
            payload = json.loads(self.rfile.read(n))
            self._send(200, run_engine(payload))
        except (KeyError, ValueError, TypeError, IndexError) as e:
            self._send(400, json.dumps({"error": f"Bad input: {e}"}))
        except Exception as e:
            self._send(500, json.dumps({"error": str(e)}))

    def log_message(self, fmt, *args):
        print(self.address_string(), fmt % args)


if __name__ == "__main__":
    build()
    print(f"Open http://localhost:{PORT} in your browser (Ctrl+C to stop)")
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
