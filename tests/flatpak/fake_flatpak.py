#!/usr/bin/python3
import os
import pathlib
import signal
import sys
import time

args = sys.argv[1:]
state = pathlib.Path(os.environ["RO_STORE_TEST_STATE"])
mode = os.environ.get("RO_STORE_TEST_MODE", "normal")
command = args[0] if args and args[0] in ("remote-info", "info") else (args[1] if len(args) > 1 else "")
scope = args[0] if args else ""
if command == "remotes":
    if scope == "--user":
        print("flathub\thttps://dl.flathub.org/repo/")
elif command == "list":
    if mode == "read-fail-after-install" and state.exists():
        print("simulated read failure", file=sys.stderr)
        sys.exit(5)
    if scope == "--user" and state.exists():
        print("app.test.Test\tTest\t1.0\tflathub\tstable\tx86_64\t1 MB")
elif command == "remote-ls":
    if "--app" in args:
        print("app.test.Test\tTest\t1.0\tstable")
elif command in ("remote-info", "info"):
    if mode == "permission-fail":
        print("permission metadata unavailable", file=sys.stderr)
        sys.exit(3)
    if mode == "permission-slow":
        time.sleep(2)
    print("[Context]\nshared=network;\nfilesystems=xdg-download;")
elif command == "install":
    if mode == "mutation-fail":
        print("simulated network failure", file=sys.stderr)
        sys.exit(9)
    if mode == "disk-full":
        print("No space left on device", file=sys.stderr)
        sys.exit(28)
    if mode == "crash":
        os.kill(os.getpid(), signal.SIGKILL)
    if mode == "slow-install":
        time.sleep(2)
    state.write_text("installed")
elif command == "uninstall":
    state.unlink(missing_ok=True)
else:
    sys.exit(7)
