#!/usr/bin/env python3
"""Regression tests for the OPT4001 HIL runner classifier and dry-run plan."""

from __future__ import annotations

import contextlib
import io
import unittest

import hil_opt4001_runner as hil


class EmptySerial:
    def __init__(self) -> None:
        self.written = b""

    @property
    def in_waiting(self) -> int:
        return 0

    def write(self, data: bytes) -> int:
        self.written += data
        return len(data)

    def flush(self) -> None:
        pass

    def read(self, size: int) -> bytes:
        return b""


class HilOpt4001RunnerParserTest(unittest.TestCase):
    def run_main(self, args: list[str]) -> tuple[int, str]:
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            code = hil.main(args)
        return code, stdout.getvalue()

    def test_smoke_plan_covers_common_contract_with_opt4001_aliases(self) -> None:
        arduino = hil.selected_commands(hil.parse_args(["--cli", "arduino", "--dry-run"]))
        idf = hil.selected_commands(hil.parse_args(["--cli", "idf", "--dry-run"]))

        for commands in (arduino, idf):
            with self.subTest(commands=commands):
                self.assertIn("version", commands)
                self.assertIn("scan", commands)
                self.assertIn("probe", commands)
                self.assertIn("id", commands)
                self.assertIn("cfg", commands)  # settings snapshot/readback command

        self.assertIn("state", arduino)  # health/state alias
        self.assertIn("drv", idf)        # health/state alias

    def test_dry_run_lists_commands_without_hardware_pass_claim(self) -> None:
        code, output = self.run_main(["--dry-run", "--cli", "arduino", "--group", "smoke"])

        self.assertEqual(0, code)
        lines = output.strip().splitlines()
        self.assertIn("version", lines)
        self.assertIn("scan", lines)
        self.assertIn("probe", lines)
        self.assertIn("cfg", lines)
        self.assertNotIn("PASS", output)
        self.assertNotIn("FAIL", output)

    def test_missing_port_is_rejected_before_serial_import(self) -> None:
        code, output = self.run_main([])

        self.assertEqual(2, code)
        self.assertIn("--port is required", output)

    def test_failure_tokens_are_failures(self) -> None:
        samples = (
            "Status: DEVICE_NOT_FOUND",
            "Status: DEVICE_ID_MISMATCH",
            "Status: I2C_ERROR",
            "Status: I2C_NACK_ADDR",
            "Status: I2C_NACK_DATA",
            "Status: I2C_TIMEOUT",
            "Status: I2C_BUS",
            "Status: TIMEOUT",
            "Status: NOT_INITIALIZED",
            "Status: INVALID_CONFIG",
            "Status: INVALID_PARAM",
            "Status: BUSY",
            "selftest failed=1",
            "Stress complete failures: 2",
        )

        for sample in samples:
            with self.subTest(sample=sample):
                status, reason = hil.classify_output(sample)
                self.assertEqual("FAIL", status)
                self.assertNotEqual("", reason)

    def test_zero_failure_counters_are_not_failures(self) -> None:
        samples = (
            "selftest fail=0 OK",
            "Stress complete failures: 0",
            "Last error: never\nStatus: OK\n",
        )

        for sample in samples:
            with self.subTest(sample=sample):
                status, _ = hil.classify_output(sample)
                self.assertEqual("PASS", status)

    def test_warning_tokens_are_warnings(self) -> None:
        for sample in ("Status: CRC_ERROR", "Status: MEASUREMENT_NOT_READY"):
            with self.subTest(sample=sample):
                status, reason = hil.classify_output(sample)
                self.assertEqual("WARN", status)
                self.assertIn("ERROR" if "CRC" in sample else "READY", reason)

    def test_expected_address_and_device_id_output_can_pass_when_no_error_token(self) -> None:
        text = (
            "I2C scan:\n"
            "  found 0x45\n"
            "DEVICE_ID raw=0x0121 didh=0x121 didl=0 reserved_clear=true match=true\n"
            "Status: OK\n"
        )

        status, reason = hil.classify_output(text)
        self.assertEqual("PASS", status)
        self.assertEqual("response captured", reason)

    def test_no_serial_response_is_failure(self) -> None:
        serial = EmptySerial()

        result = hil.run_command(serial, "probe", timeout_s=0.001, idle_s=0.0)

        self.assertEqual(b"probe\n", serial.written)
        self.assertEqual("FAIL", result.status)
        self.assertEqual("no response before timeout", result.reason)

    def test_ansi_escape_sequences_do_not_hide_failure_tokens(self) -> None:
        clean = hil.strip_ansi("\x1b[31mStatus: I2C_TIMEOUT\x1b[0m")

        status, reason = hil.classify_output(clean)
        self.assertEqual("FAIL", status)
        self.assertEqual("I2C_TIMEOUT", reason)


if __name__ == "__main__":
    unittest.main(verbosity=2)
