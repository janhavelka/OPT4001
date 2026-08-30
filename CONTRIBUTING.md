# Contributing

Thank you for considering contributing to this project.

**Read [AGENTS.md](AGENTS.md) before touching `include/` or `src/`.** It holds the
binding rules for this repository: the core stays framework-neutral (no Arduino,
ESP-IDF, FreeRTOS, `Wire`, `String`, or logging), I2C is injected and non-owning,
every fallible API returns `Status`, and there is no heap allocation, `delay()`,
or unbounded loop in steady-state driver code. CI enforces several of these.

## Quick Start

1. Fork the repository.
2. Create a feature branch: `git checkout -b feature/my-feature`.
3. Make your changes.
4. Run the gates below.
5. Commit with a [Conventional Commits](https://www.conventionalcommits.org/)
   message: `git commit -m "fix: reject inverted threshold windows"`.
6. Push and open a Pull Request.

## Before You Open A PR

CI runs all of the following, and every one of them can fail your PR. Run them
locally first. On Windows use `.\scripts\pio.cmd` instead of `pio`; it selects
the existing VS Code-managed PlatformIO Core.

```sh
python tools/check_core_timing_guard.py        # core stays framework-neutral
python tools/check_cli_contract.py             # Arduino example command contract
python tools/check_ci_action_pins.py           # actions pinned to commit SHAs
python tools/check_idf_example_contract.py     # native ESP-IDF example boundary
python tools/check_version_header_contract.py  # generated version header shape
python tools/check_clean_consumer_package.py   # packaged library imports cleanly
python scripts/generate_version.py check       # generated files are in sync
python tools/hil_opt4001_runner.py --parser-self-test
python tools/test_hil_opt4001_runner_parser.py

pio test -e native                             # fake-transport unit tests
pio run -e native_core_no_arduino              # strict -Werror framework-neutral build
pio run -e esp32s3dev
pio run -e esp32s2dev
pio pkg pack                                   # then delete the OPT4001-*.tar.gz
```

CI also finishes with `git diff --check` and `git diff --exit-code`, so **any**
uncommitted change a generator produces fails the build.

### Generated files — never hand-edit

`library.json` is the single version source of truth. `scripts/generate_version.py`
regenerates all of these from it:

- `include/OPT4001/Version.h`
- `idf_component.yml` (`version:`)
- `Doxyfile` (`PROJECT_NUMBER`)
- `SECURITY.md` (the supported-version row)

To change the version, edit `library.json` (or use
`python scripts/generate_version.py bump patch|minor|major`) and then run
`python scripts/generate_version.py sync`.

## Guidelines

### Code Style

- Follow the existing style; `.clang-format` is authoritative.
- C++17. The core must also compile clean under
  `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`
  (that is what `native_core_no_arduino` checks).
- `static constexpr` for constants, never macros. Macros only for conditional
  compilation or logging helpers.
- Member variables `_camelCase`, methods `camelCase`, constants `CAPS_CASE`.
- Prefer explicit over implicit; prefer deleting code over adding it.

### Pull Requests

- Keep PRs focused: one feature or fix per PR.
- Update the documentation when behaviour changes — including the Doxygen
  comment on any public method you touch, since that is the contract.
- Add a `CHANGELOG.md` entry under `## Unreleased`.
- Add or update a test in `test/test_basic.cpp` for anything behavioural.

### What We Accept

- Bug fixes, especially ones with a test that fails before and passes after.
- Documentation corrections, particularly anything that disagrees with
  `docs/reference/OPT4001_datasheet.pdf`.
- Simplification that removes code without removing capability.
- New examples that demonstrate a common integration.

### What We Probably Won't Accept

- Breaking API changes without prior discussion.
- Heavy dependencies.
- Platform-specific code in `include/` or `src/`.
- Heap allocation, `String`, or unbounded loops in steady-state paths.

## Questions?

Open a GitHub Discussion or Issue.
