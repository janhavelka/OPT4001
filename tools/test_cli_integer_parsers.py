#!/usr/bin/env python3
"""Exercise the example integer parsers verbatim on the host C++ runtime."""

from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def parser_function(path: pathlib.Path, name: str) -> str:
    source = path.read_text(encoding="utf-8")
    match = re.search(rf"^bool {name}\([^\n]+\) \{{.*?^\}}", source, re.MULTILINE | re.DOTALL)
    if match is None:
        raise AssertionError(f"Cannot find {name} definition in {path}")
    return match.group(0)


class CliIntegerParsersTest(unittest.TestCase):
    def test_integer_limits_signs_and_complete_tokens(self) -> None:
        compiler = shutil.which("g++")
        self.assertIsNotNone(compiler, "g++ is required for the CLI parser regression")
        arduino = ROOT / "examples/01_basic_bringup_cli/main.cpp"
        idf = ROOT / "examples/esp_idf/basic/main/main.cpp"
        source = """#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "CliText.h"
namespace arduino {
""" + parser_function(arduino, "parseU32") + "\n" + parser_function(arduino, "parseI32")
        source += "\n}\nnamespace idf {\n" + parser_function(idf, "parseU32") + "\n}\n"
        source += r'''
struct UnsignedCase { const char* text; bool valid; uint32_t value; };
const UnsignedCase UNSIGNED_CASES[] = {
    {"0", true, 0U}, {"42", true, 42U}, {"0x2a", true, 42U},
    {"+42", true, 42U}, {" \t42", true, 42U},
    {"4294967295", true, UINT32_MAX}, {"0xffffffff", true, UINT32_MAX},
    {"", false, 0U}, {" \t", false, 0U}, {"-1", false, 0U},
    {" \t-1", false, 0U}, {"\v-4294967295", false, 0U},
    {"4294967296", false, 0U}, {"0x100000000", false, 0U},
    {"9999999999999999999999999999999", false, 0U},
    {"42junk", false, 0U}, {"42 7", false, 0U}, {"0x", false, 0U},
};

template <typename Parser> bool checkUnsigned(const char* label, Parser parse) {
  for (const auto& item : UNSIGNED_CASES) {
    uint32_t value = 17U;
    errno = ERANGE;  // A previous conversion must not poison this call.
    const bool valid = parse(item.text, value);
    if (valid != item.valid || value != (item.valid ? item.value : 17U)) {
      std::fprintf(stderr, "%s rejected/accepted or changed output for [%s]\n", label, item.text);
      return false;
    }
  }
  return true;
}

int main() {
  if (!checkUnsigned("Arduino", [](const char* text, uint32_t& value) {
        return arduino::parseU32(cli_shell::FixedText(text), value);
      }) || !checkUnsigned("IDF", idf::parseU32)) return 1;
  uint32_t unsignedValue = 17U;
  if (idf::parseU32(nullptr, unsignedValue) || unsignedValue != 17U) return 2;

  struct SignedCase { const char* text; bool valid; int32_t value; };
  const SignedCase signedCases[] = {
      {"-1", true, -1}, {" \t-1", true, -1}, {"42", true, 42}, {"0x2a", true, 42},
      {"2147483647", true, INT32_MAX}, {"-2147483648", true, INT32_MIN},
      {"-0x80000000", true, INT32_MIN}, {"2147483648", false, 0},
      {"-2147483649", false, 0}, {"9999999999999999999999999999999", false, 0},
      {"-9999999999999999999999999999999", false, 0},
      {"", false, 0}, {"-", false, 0}, {"42junk", false, 0}, {"42 7", false, 0},
  };
  for (const auto& item : signedCases) {
    int32_t value = 17;
    errno = ERANGE;
    const bool valid = arduino::parseI32(cli_shell::FixedText(item.text), value);
    if (valid != item.valid || value != (item.valid ? item.value : 17)) {
      std::fprintf(stderr, "Arduino signed parser rejected/accepted or changed output for [%s]\n", item.text);
      return 3;
    }
  }
}
'''
        with tempfile.TemporaryDirectory(prefix="opt4001-cli-parsers-") as directory:
            cpp = pathlib.Path(directory) / "parsers.cpp"
            binary = pathlib.Path(directory) / "parsers.exe"
            cpp.write_text(source, encoding="utf-8")
            build = subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "examples/common"), str(cpp), "-o", str(binary)],
                capture_output=True, text=True, timeout=30, check=False,
            )
            self.assertEqual(0, build.returncode, build.stdout + build.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=5, check=False)
            self.assertEqual(0, run.returncode, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
