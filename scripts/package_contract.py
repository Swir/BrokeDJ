#!/usr/bin/env python3
"""Create and verify the BrokeDJ staged Windows development-artifact contract.

The contract is intentionally deterministic: it records only source/version
identity and sorted file metadata. It does not record timestamps, machine names
or local paths.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

SCHEMA_VERSION = 1
ARTIFACT_KIND = "development-windows-x64"
MANIFEST_NAME = "PACKAGE-MANIFEST.json"
SUMS_NAME = "SHA256SUMS.txt"
SOURCE_COMMIT_PATH = "BrokeDJ/SOURCE-COMMIT.txt"

REQUIRED_PATHS = (
    "BrokeDJ/BrokeDJ.exe",
    "BrokeDJ/LICENSE",
    "BrokeDJ/README.md",
    "BrokeDJ/THIRD_PARTY_NOTICES.md",
    "BrokeDJ/licenses/JUCE-LICENSE.md",
    "BrokeDJ/licenses/JUCE.spdx.json",
    "BrokeDJ/VALIDATION.md",
    "BrokeDJ/AUDIO-DIAGNOSTICS.txt",
    "BrokeDJ/GUI-SMOKE.json",
    "BrokeDJ/DEVICE-PROBE-CI.txt",
    "BrokeDJ/DEVICE-PROBE-CI.json",
    SOURCE_COMMIT_PATH,
    "BrokeDJ-source.zip",
    "VERIFY-PACKAGE.py",
)

SHA_RE = re.compile(r"^[0-9a-f]{40}$")
VERSION_RE = re.compile(
    r"project\s*\(\s*BrokeDJ\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    re.IGNORECASE | re.MULTILINE,
)


class ContractError(RuntimeError):
    """Raised when the staged artifact violates the package contract."""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def _payload_files(root: Path) -> list[Path]:
    if not root.is_dir():
        raise ContractError(f"artifact root does not exist: {root}")
    files: list[Path] = []
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ContractError(f"symbolic links are not allowed in the staged artifact: {_relative(root, path)}")
        if not path.is_file():
            continue
        rel = _relative(root, path)
        if rel in {MANIFEST_NAME, SUMS_NAME}:
            continue
        files.append(path)
    return sorted(files, key=lambda item: _relative(root, item))


def _read_project_version(cmake_file: Path) -> str:
    try:
        text = cmake_file.read_text(encoding="utf-8")
    except OSError as exc:
        raise ContractError(f"cannot read CMake project file: {cmake_file}") from exc
    match = VERSION_RE.search(text)
    if match is None:
        raise ContractError("could not derive BrokeDJ project version from CMakeLists.txt")
    return match.group(1)


def _validate_commit(commit: str) -> str:
    value = commit.strip().lower()
    if SHA_RE.fullmatch(value) is None:
        raise ContractError("source commit must be a full 40-character lowercase/uppercase hexadecimal SHA")
    return value


def _required_missing(paths: set[str]) -> list[str]:
    return sorted(path for path in REQUIRED_PATHS if path not in paths)


def _entries(root: Path) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for path in _payload_files(root):
        rel = _relative(root, path)
        result.append(
            {
                "path": rel,
                "bytes": path.stat().st_size,
                "sha256": _sha256(path),
            }
        )
    return result


def _expected_sums(entries: list[dict[str, object]]) -> str:
    return "".join(f"{entry['sha256']}  {entry['path']}\n" for entry in entries)


def create_contract(root: Path, commit: str, version: str) -> None:
    root = root.resolve()
    commit = _validate_commit(commit)
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ContractError(f"invalid semantic project version: {version}")

    source_commit_file = root / SOURCE_COMMIT_PATH
    if not source_commit_file.is_file():
        raise ContractError(f"missing source identity file: {SOURCE_COMMIT_PATH}")
    recorded_commit = source_commit_file.read_text(encoding="utf-8").strip().lower()
    if recorded_commit != commit:
        raise ContractError(
            f"{SOURCE_COMMIT_PATH} records {recorded_commit or '<empty>'}, expected {commit}"
        )

    entries = _entries(root)
    paths = {str(entry["path"]) for entry in entries}
    missing = _required_missing(paths)
    if missing:
        raise ContractError("required staged files are missing: " + ", ".join(missing))

    manifest = {
        "schema_version": SCHEMA_VERSION,
        "artifact_kind": ARTIFACT_KIND,
        "application": "BrokeDJ",
        "version": version,
        "source_commit": commit,
        "file_count": len(entries),
        "files": entries,
        "qualification_note": (
            "Integrity and staged no-build-tree launch contract only; not clean-machine, "
            "audio-device, controller, latency, listening, or live-performance qualification."
        ),
    }
    (root / MANIFEST_NAME).write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    (root / SUMS_NAME).write_text(_expected_sums(entries), encoding="utf-8", newline="\n")


def verify_contract(
    root: Path,
    *,
    expected_commit: str | None = None,
    expected_version: str | None = None,
) -> dict[str, object]:
    root = root.resolve()
    manifest_path = root / MANIFEST_NAME
    sums_path = root / SUMS_NAME
    if not manifest_path.is_file() or not sums_path.is_file():
        raise ContractError(f"{MANIFEST_NAME} and {SUMS_NAME} must both exist")

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ContractError(f"cannot parse {MANIFEST_NAME}") from exc

    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise ContractError("unsupported package manifest schema")
    if manifest.get("artifact_kind") != ARTIFACT_KIND or manifest.get("application") != "BrokeDJ":
        raise ContractError("artifact identity does not match the BrokeDJ Windows development contract")

    commit = _validate_commit(str(manifest.get("source_commit", "")))
    if expected_commit is not None and commit != _validate_commit(expected_commit):
        raise ContractError(f"manifest source commit {commit} does not match expected commit")
    version = str(manifest.get("version", ""))
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ContractError("manifest project version is invalid")
    if expected_version is not None and version != expected_version:
        raise ContractError(f"manifest version {version} does not match expected version {expected_version}")

    files = manifest.get("files")
    if not isinstance(files, list):
        raise ContractError("manifest files entry must be an array")

    manifest_entries: list[dict[str, object]] = []
    manifest_paths: set[str] = set()
    for raw in files:
        if not isinstance(raw, dict):
            raise ContractError("manifest contains a non-object file entry")
        path_value = raw.get("path")
        digest = raw.get("sha256")
        size = raw.get("bytes")
        if not isinstance(path_value, str) or not path_value or "\\" in path_value:
            raise ContractError("manifest file paths must be non-empty normalized forward-slash paths")
        candidate = Path(path_value)
        if candidate.is_absolute() or ".." in candidate.parts:
            raise ContractError(f"unsafe manifest path: {path_value}")
        if path_value in manifest_paths:
            raise ContractError(f"duplicate manifest path: {path_value}")
        if not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
            raise ContractError(f"invalid SHA-256 for {path_value}")
        if not isinstance(size, int) or size < 0:
            raise ContractError(f"invalid byte size for {path_value}")
        manifest_paths.add(path_value)
        manifest_entries.append({"path": path_value, "bytes": size, "sha256": digest})

    if manifest.get("file_count") != len(manifest_entries):
        raise ContractError("manifest file_count does not match files array")
    missing_required = _required_missing(manifest_paths)
    if missing_required:
        raise ContractError("manifest omits required staged files: " + ", ".join(missing_required))

    actual_paths = {_relative(root, path) for path in _payload_files(root)}
    missing = sorted(manifest_paths - actual_paths)
    extras = sorted(actual_paths - manifest_paths)
    if missing:
        raise ContractError("manifested files are missing from artifact: " + ", ".join(missing))
    if extras:
        raise ContractError("artifact contains unmanifested files: " + ", ".join(extras))

    for entry in manifest_entries:
        path = root / str(entry["path"])
        actual_size = path.stat().st_size
        if actual_size != entry["bytes"]:
            raise ContractError(f"size mismatch for {entry['path']}")
        actual_digest = _sha256(path)
        if actual_digest != entry["sha256"]:
            raise ContractError(f"SHA-256 mismatch for {entry['path']}")

    source_commit_file = root / SOURCE_COMMIT_PATH
    recorded_commit = source_commit_file.read_text(encoding="utf-8").strip().lower()
    if recorded_commit != commit:
        raise ContractError(
            f"{SOURCE_COMMIT_PATH} records {recorded_commit or '<empty>'}, manifest records {commit}"
        )

    expected_sums = _expected_sums(manifest_entries)
    actual_sums = sums_path.read_text(encoding="utf-8")
    if actual_sums.replace("\r\n", "\n") != expected_sums:
        raise ContractError(f"{SUMS_NAME} does not match the manifest")

    return manifest


def self_test() -> None:
    commit = "0123456789abcdef0123456789abcdef01234567"
    with tempfile.TemporaryDirectory(prefix="brokedj-package-contract-") as tmp:
        root = Path(tmp) / "artifact"
        root.mkdir()
        for rel in REQUIRED_PATHS:
            path = root / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            if rel == SOURCE_COMMIT_PATH:
                path.write_text(commit, encoding="utf-8")
            elif rel == "VERIFY-PACKAGE.py":
                shutil.copyfile(Path(__file__), path)
            else:
                path.write_bytes(("fixture:" + rel).encode("utf-8"))

        create_contract(root, commit, "0.1.0")
        verified = verify_contract(root, expected_commit=commit, expected_version="0.1.0")
        if verified.get("file_count") != len(REQUIRED_PATHS):
            raise ContractError("self-test manifest count did not match fixture count")

        tampered = root / "BrokeDJ" / "README.md"
        tampered.write_text("tampered", encoding="utf-8")
        try:
            verify_contract(root, expected_commit=commit)
        except ContractError:
            pass
        else:
            raise ContractError("self-test failed to detect a tampered payload")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    create = sub.add_parser("create", help="create deterministic manifest and SHA-256 sums")
    create.add_argument("--root", type=Path, required=True, help="artifact root (normally dist)")
    create.add_argument("--commit", required=True, help="full source commit SHA")
    version_source = create.add_mutually_exclusive_group(required=True)
    version_source.add_argument("--version", help="explicit BrokeDJ semantic version")
    version_source.add_argument("--cmake", type=Path, help="read BrokeDJ version from CMakeLists.txt")

    verify = sub.add_parser("verify", help="verify manifest, hashes and required payload")
    verify.add_argument("--root", type=Path, required=True, help="artifact root")
    verify.add_argument("--expected-commit", help="require this exact source commit SHA")
    verify.add_argument("--expected-version", help="require this exact semantic version")

    sub.add_parser("self-test", help="run deterministic create/tamper/verify regression checks")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "create":
            version = args.version if args.version is not None else _read_project_version(args.cmake)
            create_contract(args.root, args.commit, version)
            print(f"created {args.root / MANIFEST_NAME} and {args.root / SUMS_NAME}")
        elif args.command == "verify":
            manifest = verify_contract(
                args.root,
                expected_commit=args.expected_commit,
                expected_version=args.expected_version,
            )
            print(
                "verified BrokeDJ staged artifact: "
                f"{manifest['file_count']} files, version {manifest['version']}, "
                f"commit {manifest['source_commit']}"
            )
        else:
            self_test()
            print("package contract self-test passed")
    except (ContractError, OSError, UnicodeError) as exc:
        print(f"package contract failure: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
