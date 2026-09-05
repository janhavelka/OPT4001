#!/usr/bin/env python3
"""Ensure documented CLI commands alone cannot satisfy executable parity."""

from __future__ import annotations

import contextlib
import io
import unittest
from unittest.mock import patch

import check_idf_example_contract as contract


class IdfExampleContractTest(unittest.TestCase):
    def test_help_entry_cannot_replace_command_handler(self) -> None:
        main_path = contract.IDF_MAIN / "main.cpp"
        original_read = contract.read
        main_text = original_read(main_path, "native ESP-IDF main")
        # Keep the real help entry, but remove the command from dispatch.
        self.assertIn('"diag"', main_text[:main_text.index("void processCommand")])
        mutated = main_text.replace('strcmp(cmd, "diag")', 'strcmp(cmd, "removed_diag")')
        self.assertNotEqual(main_text, mutated)

        def read(path, label):
            return mutated if path == main_path else original_read(path, label)

        output = io.StringIO()
        with patch.object(contract, "read", side_effect=read), contextlib.redirect_stdout(output):
            with self.assertRaises(SystemExit) as failure:
                contract.main()
        self.assertEqual(1, failure.exception.code)
        self.assertIn("missing command handler 'diag'", output.getvalue())


if __name__ == "__main__":
    unittest.main(verbosity=2)
