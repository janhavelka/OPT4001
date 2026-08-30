# Security Policy

## Supported Versions

<!-- The supported row is generated from library.json by
     scripts/generate_version.py. Do not hand-edit it. -->

| Version | Supported          |
| ------- | ------------------ |
| 1.2.x   | :white_check_mark: |
| 1.1.x   | :x:                |
| 1.0.x   | :x:                |
| < 1.0   | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability within this library, please follow responsible disclosure:

1. **Do NOT** open a public GitHub issue.
2. Email the maintainer at: `info@thymos.cz`.
3. Include:
   - A description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Any suggested fixes (optional)

This is a single-maintainer project, so response is best-effort. Expect an acknowledgement of receipt before any fix, and please do not disclose publicly until a fix or mitigation is available.

## Scope

This library is designed for embedded systems. Security considerations include:
- No dynamic memory allocation in steady state (reduces attack surface)
- No network code (networking is out of scope for this library)
- No persistent storage in the driver core

## Security Best Practices for Users

- Always validate external inputs before passing to `Config`
- Use hardware watchdogs in production deployments
- Keep dependencies updated
