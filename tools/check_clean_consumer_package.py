#!/usr/bin/env python3
"""Build a clean PlatformIO consumer against the packed OPT4001 package."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str, output: str = "") -> int:
    print(f"Clean consumer package check FAILED: {message}", file=sys.stderr)
    if output.strip():
        print(output.strip(), file=sys.stderr)
    return 1


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
    )


def ensure_platformio() -> bool:
    result = run([sys.executable, "-m", "platformio", "--version"], ROOT)
    return result.returncode == 0


def main() -> int:
    if not ensure_platformio():
        return fail("PlatformIO is not available through the active Python")

    with tempfile.TemporaryDirectory(prefix="opt4001-clean-consumer-") as temp_name:
        temp = Path(temp_name)
        package_path = temp / "OPT4001-clean-consumer.tar.gz"
        consumer = temp / "consumer"
        src_dir = consumer / "src"
        src_dir.mkdir(parents=True)

        pack = run(
            [
                sys.executable,
                "-m",
                "platformio",
                "pkg",
                "pack",
                "-o",
                str(package_path),
                ".",
            ],
            ROOT,
        )
        if pack.returncode != 0:
            return fail("package pack failed", pack.stdout + pack.stderr)
        if not package_path.exists():
            return fail(f"package artifact was not created: {package_path}")

        (consumer / "platformio.ini").write_text(
            textwrap.dedent(
                f"""\
                [env:native]
                platform = native
                framework =
                build_flags =
                  -std=c++17
                  -Wall
                  -Wextra
                  -Werror=return-type
                lib_deps =
                  {package_path.as_posix()}
                """
            ),
            encoding="utf-8",
            newline="\n",
        )
        (src_dir / "main.cpp").write_text(
            textwrap.dedent(
                """\
                #include <type_traits>

                #include "OPT4001/CommandTable.h"
                #include "OPT4001/Config.h"
                #include "OPT4001/OPT4001.h"
                #include "OPT4001/Status.h"
                #include "OPT4001/Version.h"

                static_assert(OPT4001::VERSION_MAJOR >= 1, "version header must be available");
                static_assert(std::is_default_constructible<OPT4001::OPT4001>::value,
                              "driver must be default constructible");

                int main() {
                  OPT4001::OPT4001 sensor;
                  OPT4001::Config config;
                  OPT4001::Status status = OPT4001::Status::Ok();
                  const float lux = sensor.adcCodesToLux(0);
                  return (config.i2cAddress == OPT4001::cmd::I2C_ADDR_DEFAULT &&
                          status.ok() && lux == 0.0f) ? 0 : 1;
                }
                """
            ),
            encoding="utf-8",
            newline="\n",
        )

        build = run([sys.executable, "-m", "platformio", "run", "-e", "native"], consumer)
        if build.returncode != 0:
            return fail("clean consumer build failed", build.stdout + build.stderr)

    print("Clean consumer package check PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
