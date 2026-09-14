#!/usr/bin/env python3
"""Run localization regressions in a temporary localhost-only open.mp server."""

import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-root", type=Path, required=True, help="open.mp installation containing omp-server, qawno, components and plugins")
    args = parser.parse_args()
    repo = args.server_root.resolve()
    library = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="lwd-pp-hooks-") as temporary:
        runtime = Path(temporary)
        for directory in ("components", "plugins", "gamemodes", "scriptfiles"):
            (runtime / directory).mkdir()
        for component in (repo / "components/LINUX/default").glob("*.so"):
            (runtime / "components" / component.name).symlink_to(component)
        (runtime / "components/sscanf.so").symlink_to(repo / "components/LINUX/sscanf.so")
        for plugin in ("crashdetect", "PawnPlus", "FileManager"):
            (runtime / "plugins" / f"{plugin}.so").symlink_to(repo / "plugins" / f"{plugin}.so")

        languages = {
            "en_English": "FORMAT %s has %d at %.2f%%.\nPLAIN 100%% ready\nTEMPLATE %s owes %d\nDYNAMIC %S has {1:d} at {2:.2f}%%.\nLONG {0:S}\nNAMED Total: {$env[\"amount\"]:d}\n# end\n",
            "pt_Portuguese": "OTHER Outro\n# end\n",
        }
        for language, content in languages.items():
            directory = runtime / "scriptfiles/languages" / language
            directory.mkdir(parents=True)
            (directory / "tests.txt").write_text(content)

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as port_probe:
            port_probe.bind(("127.0.0.1", 0))
            port = port_probe.getsockname()[1]
        config = {
            "name": "LWD isolated pp-hooks tests",
            "announce": False,
            "max_players": 1,
            "network": {"bind": "127.0.0.1", "port": port},
            "pawn": {
                "legacy_plugins": ["crashdetect", "PawnPlus", "FileManager"],
                "main_scripts": ["pp_hooks_runtime"],
                "side_scripts": [],
            },
            "rcon": {"enable": False, "password": "isolated-test-only"},
        }
        (runtime / "config.json").write_text(json.dumps(config, indent=2))
        environment = os.environ.copy()
        environment["LD_LIBRARY_PATH"] = str(repo / "qawno")
        build = subprocess.run(
            [
                str(repo / "qawno/pawncc"),
                str(library / "tests/runtime.pwn"),
                f"-i{library / 'includes'}",
                "-Dgamemodes", "-;+", "-(+", "-d3", "-Z+",
                f"-o{runtime / 'gamemodes/pp_hooks_runtime.amx'}",
            ],
            cwd=repo, env=environment, capture_output=True, text=True, timeout=60,
        )
        print(build.stdout + build.stderr, end="")
        if build.returncode or "warning " in build.stdout + build.stderr:
            raise SystemExit("Fixture compilation failed or produced warnings")
        try:
            run = subprocess.run(
                [str(repo / "omp-server")], cwd=runtime,
                capture_output=True, text=True, timeout=30,
            )
        except subprocess.TimeoutExpired as error:
            print(error.stdout or "")
            print(error.stderr or "")
            raise SystemExit("Test server did not finish within 30 seconds") from error
        output = run.stdout + run.stderr
        if (
            run.returncode
            or "PP_LANGUAGE_TEST_RESULT failures=0" not in output
            or "FAIL:" in output
            or "Run time error" in output
            or "[Error]" in output
        ):
            print(output)
            raise SystemExit("Runtime regression tests failed")
        print("PASS: legacy and dynamic formatting, named maps, fallback, cache reload, and string ownership")


if __name__ == "__main__":
    main()
