#!/usr/bin/env python3
"""Validate the packaged BrokeDJ M1 launcher contract without touching hardware."""

from __future__ import annotations

import argparse
from pathlib import Path


ROUTE = 'if /I "%~1"=="M1" goto m1_only'
LABEL = ':m1_only'
BINDING = 'set "M1_WITNESS=%ROOT%M1-HARDWARE-WITNESS.ps1"'
INVOCATION = (
    '"%POWERSHELL%" -NoLogo -NoProfile -File "%M1_WITNESS%" '
    '-AppPath "%APP%" '
    '-EvidencePath "%M1_EVIDENCE%\\BrokeDJ-M1-Hardware-Witness.json" '
    '-ProbePath "%M1_EVIDENCE%\\BrokeDJ-device-probe.json"'
)


class ContractError(RuntimeError):
    """Raised when the launcher no longer satisfies the M1 package contract."""


def _normalized_lines(text: str) -> list[str]:
    return [line.strip() for line in text.splitlines()]


def _index(lines: list[str], expected: str, name: str) -> int:
    try:
        return lines.index(expected)
    except ValueError as exc:
        raise ContractError(f"missing {name}: {expected}") from exc


def _assert_sequence(lines: list[str], sequence: list[str], name: str) -> None:
    width = len(sequence)
    if not any(lines[index : index + width] == sequence for index in range(len(lines) - width + 1)):
        raise ContractError(f"missing grouped {name} sequence")


def validate_launcher_text(text: str) -> None:
    lines = _normalized_lines(text)

    route_index = _index(lines, ROUTE, "M1 route")
    label_index = _index(lines, LABEL, "M1 label")
    if route_index >= label_index:
        raise ContractError("M1 route must appear before :m1_only")

    binding_index = _index(lines, BINDING, "M1 witness binding")
    invocation_index = _index(lines, INVOCATION, "M1 witness invocation")
    if binding_index >= invocation_index:
        raise ContractError("M1 witness binding must appear before invocation")

    for variable in ("BETA_EVIDENCE", "M1_EVIDENCE"):
        _assert_sequence(
            lines,
            [
                f'if not exist "%{variable}%" (',
                f'mkdir "%{variable}%"',
                "if errorlevel 1 (",
            ],
            variable,
        )


def validate_launcher(path: Path) -> None:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ContractError(f"cannot read launcher: {path}") from exc
    validate_launcher_text(text)


def _fixture() -> str:
    return "\n".join(
        [
            '@echo off',
            'set "ROOT=%~dp0"',
            BINDING,
            ROUTE,
            'if not exist "%BETA_EVIDENCE%" (',
            'mkdir "%BETA_EVIDENCE%"',
            'if errorlevel 1 (',
            ')',
            ')',
            LABEL,
            'if not exist "%M1_EVIDENCE%" (',
            'mkdir "%M1_EVIDENCE%"',
            'if errorlevel 1 (',
            ')',
            ')',
            INVOCATION,
        ]
    )


def _expect_failure(text: str, name: str) -> None:
    try:
        validate_launcher_text(text)
    except ContractError:
        return
    raise ContractError(f"self-test mutation unexpectedly passed: {name}")


def self_test() -> None:
    good = _fixture()
    validate_launcher_text(good)

    _expect_failure(good.replace(ROUTE, "rem M1 route removed"), "missing route")

    lines = good.splitlines()
    lines.remove(LABEL)
    route_index = lines.index(ROUTE)
    lines.insert(route_index, LABEL)
    _expect_failure("\n".join(lines), "label before route")

    _expect_failure(
        good.replace(BINDING, 'set "M1_WITNESS=%ROOT%WRONG.ps1"'),
        "bad witness binding",
    )
    _expect_failure(
        good.replace("BrokeDJ-device-probe.json", "wrong-probe.json"),
        "bad probe binding",
    )
    _expect_failure(
        good.replace(
            'mkdir "%BETA_EVIDENCE%"\nif errorlevel 1 (',
            'mkdir "%BETA_EVIDENCE%"\nrem break grouping\nif errorlevel 1 (',
        ),
        "ungrouped Beta mkdir",
    )
    _expect_failure(
        good.replace(
            'mkdir "%M1_EVIDENCE%"\nif errorlevel 1 (',
            'mkdir "%M1_EVIDENCE%"\nrem break grouping\nif errorlevel 1 (',
        ),
        "ungrouped M1 mkdir",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "launcher",
        nargs="?",
        type=Path,
        default=Path(__file__).with_name("start_beta_qualification.cmd"),
    )
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    try:
        if args.self_test:
            self_test()
            print("M1 launcher contract self-test passed")
        else:
            validate_launcher(args.launcher)
            print(f"M1 launcher contract passed: {args.launcher}")
    except ContractError as exc:
        print(f"M1 launcher contract failure: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
