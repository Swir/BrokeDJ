#!/usr/bin/env python3
"""Create and verify BrokeDJ Windows development and portable-artifact contracts.

The development contract is deterministic and records only source/version identity
plus sorted file metadata. The portable Beta Preview is a deterministic runtime
subset rooted at ``BrokeDJ/``; source archives and development verifier metadata
remain in the outer development artifact instead of being copied into the user
portable ZIP.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import stat
import sys
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

SCHEMA_VERSION = 1
ARTIFACT_KIND = "development-windows-x64"
MANIFEST_NAME = "PACKAGE-MANIFEST.json"
SUMS_NAME = "SHA256SUMS.txt"
SOURCE_COMMIT_PATH = "BrokeDJ/SOURCE-COMMIT.txt"
PORTABLE_ZIP_NAME = "BrokeDJ-Beta-Preview-Windows-x64.zip"
PORTABLE_SUM_NAME = f"{PORTABLE_ZIP_NAME}.sha256"
PORTABLE_FIXED_TIME = (1980, 1, 1, 0, 0, 0)
PORTABLE_MAX_UNCOMPRESSED_BYTES = 8 * 1024 * 1024 * 1024
PORTABLE_FILE_MODE = (stat.S_IFREG | 0o644) << 16
PORTABLE_STAGE_PREFIX = "BrokeDJ/"

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

PORTABLE_REQUIRED_PATHS = tuple(path for path in REQUIRED_PATHS if path.startswith(PORTABLE_STAGE_PREFIX))
PORTABLE_FORBIDDEN_ROOT_FILES = {
    "BrokeDJ-source.zip",
    MANIFEST_NAME,
    SUMS_NAME,
    "VERIFY-PACKAGE.py",
}

SHA_RE = re.compile(r"^[0-9a-f]{40}$")
VERSION_RE = re.compile(
    r"project\s*\(\s*BrokeDJ\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    re.IGNORECASE | re.MULTILINE,
)


class ContractError(RuntimeError):
    """Raised when a staged or portable artifact violates its contract."""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _sha256_stream(handle) -> str:
    digest = hashlib.sha256()
    for block in iter(lambda: handle.read(1024 * 1024), b""):
        digest.update(block)
    return digest.hexdigest()


def _relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def _is_derived_outer_file(rel: str) -> bool:
    return rel in {MANIFEST_NAME, SUMS_NAME, PORTABLE_ZIP_NAME, PORTABLE_SUM_NAME}


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
        if _is_derived_outer_file(rel):
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
        raise ContractError("source commit must be a full 40-character hexadecimal SHA")
    return value


def _required_missing(paths: set[str]) -> list[str]:
    return sorted(path for path in REQUIRED_PATHS if path not in paths)


def _portable_required_missing(paths: set[str]) -> list[str]:
    return sorted(path for path in PORTABLE_REQUIRED_PATHS if path not in paths)


def _entries(root: Path) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for path in _payload_files(root):
        rel = _relative(root, path)
        result.append({"path": rel, "bytes": path.stat().st_size, "sha256": _sha256(path)})
    return result


def _expected_sums(entries: list[dict[str, object]]) -> str:
    return "".join(f"{entry['sha256']}  {entry['path']}\n" for entry in entries)


def _portable_root(version: str) -> str:
    return f"BrokeDJ-{version}-Beta-Preview-Windows-x64"


def _portable_members(root: Path) -> list[Path]:
    """Return only the staged BrokeDJ runtime tree for the consumer ZIP."""
    files = [
        path
        for path in _payload_files(root)
        if _relative(root, path).startswith(PORTABLE_STAGE_PREFIX)
    ]
    rels = {_relative(root, path) for path in files}
    missing = _portable_required_missing(rels)
    if missing:
        raise ContractError("portable runtime subset is missing required staged files: " + ", ".join(missing))
    return sorted(files, key=lambda item: _relative(root, item))


def _portable_identity(root: Path) -> dict[str, tuple[int, str]]:
    return {
        _relative(root, path): (path.stat().st_size, _sha256(path))
        for path in _portable_members(root)
    }


def _write_portable_archive(root: Path, version: str) -> None:
    archive = root / PORTABLE_ZIP_NAME
    sidecar = root / PORTABLE_SUM_NAME
    archive.unlink(missing_ok=True)
    sidecar.unlink(missing_ok=True)
    prefix = _portable_root(version)

    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as handle:
        for path in _portable_members(root):
            rel = _relative(root, path)
            info = zipfile.ZipInfo(f"{prefix}/{rel}", PORTABLE_FIXED_TIME)
            info.create_system = 3
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = PORTABLE_FILE_MODE
            info.flag_bits = 0
            handle.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)

    sidecar.write_text(f"{_sha256(archive)}  {archive.name}\n", encoding="utf-8", newline="\n")


def _validate_portable_member(name: str, prefix: str) -> PurePosixPath:
    if not name or "\\" in name or "\x00" in name:
        raise ContractError(f"unsafe portable archive member name: {name!r}")
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts:
        raise ContractError(f"unsafe portable archive path: {name}")
    if len(path.parts) < 2 or path.parts[0] != prefix:
        raise ContractError(f"portable archive member escaped expected root {prefix}: {name}")
    return path


def _verify_extracted_portable_payload(
    staged_root: Path,
    extracted_root: Path,
    *,
    version: str,
) -> None:
    staged_root = staged_root.resolve()
    extracted_root = extracted_root.resolve()
    expected_name = _portable_root(version)
    if extracted_root.name != expected_name:
        raise ContractError(
            f"extracted portable root must be named {expected_name}, got {extracted_root.name}"
        )
    if not extracted_root.is_dir():
        raise ContractError(f"extracted portable root does not exist: {extracted_root}")

    expected_identity = _portable_identity(staged_root)
    expected_paths = set(expected_identity)
    actual_paths: set[str] = set()
    total_bytes = 0

    for path in extracted_root.rglob("*"):
        if path.is_symlink():
            raise ContractError(
                f"extracted portable contains a symbolic link: {_relative(extracted_root, path)}"
            )
        if not path.is_file():
            continue
        rel = _relative(extracted_root, path)
        if rel in PORTABLE_FORBIDDEN_ROOT_FILES:
            raise ContractError(f"development-only file leaked into portable root: {rel}")
        actual_paths.add(rel)
        total_bytes += path.stat().st_size
        if total_bytes > PORTABLE_MAX_UNCOMPRESSED_BYTES:
            raise ContractError("extracted portable exceeds the bounded uncompressed-size limit")

    missing = sorted(expected_paths - actual_paths)
    extras = sorted(actual_paths - expected_paths)
    if missing or extras:
        pieces: list[str] = []
        if missing:
            pieces.append("missing=" + ", ".join(missing))
        if extras:
            pieces.append("extra=" + ", ".join(extras))
        raise ContractError("extracted portable file set differs from staged runtime subset: " + "; ".join(pieces))

    for rel, (expected_size, expected_digest) in expected_identity.items():
        path = extracted_root / rel
        if path.stat().st_size != expected_size:
            raise ContractError(f"extracted portable size differs from staged runtime: {rel}")
        if _sha256(path) != expected_digest:
            raise ContractError(f"extracted portable content differs from staged runtime: {rel}")


def verify_extracted_portable(
    staged_root: Path,
    extracted_root: Path,
    *,
    expected_commit: str | None = None,
    expected_version: str | None = None,
) -> dict[str, object]:
    """Verify an already extracted consumer ZIP against its staged runtime source."""
    manifest = verify_contract(
        staged_root,
        expected_commit=expected_commit,
        expected_version=expected_version,
        verify_portable=True,
    )
    version = str(manifest["version"])
    _verify_extracted_portable_payload(staged_root, extracted_root, version=version)
    return manifest


def _verify_portable_archive(root: Path, *, commit: str, version: str) -> None:
    archive = root / PORTABLE_ZIP_NAME
    sidecar = root / PORTABLE_SUM_NAME
    if archive.is_file() != sidecar.is_file():
        raise ContractError(f"{PORTABLE_ZIP_NAME} and {PORTABLE_SUM_NAME} must either both exist or both be absent")
    if not archive.is_file():
        raise ContractError("portable Beta Preview archive is missing")

    sidecar_text = sidecar.read_text(encoding="utf-8").replace("\r\n", "\n")
    expected_sidecar = f"{_sha256(archive)}  {archive.name}\n"
    if sidecar_text != expected_sidecar:
        raise ContractError(f"{PORTABLE_SUM_NAME} does not match the portable archive")

    prefix = _portable_root(version)
    expected_identity = _portable_identity(root)
    expected_files = set(expected_identity)
    seen: set[str] = set()
    total_uncompressed = 0

    with zipfile.ZipFile(archive, "r") as handle:
        infos = handle.infolist()
        for info in infos:
            if info.is_dir():
                raise ContractError(f"portable archive contains an unexpected directory entry: {info.filename}")
            if info.flag_bits & 0x1:
                raise ContractError(f"portable archive contains an encrypted member: {info.filename}")
            member = _validate_portable_member(info.filename, prefix)
            rel = PurePosixPath(*member.parts[1:]).as_posix()
            if rel in PORTABLE_FORBIDDEN_ROOT_FILES:
                raise ContractError(f"development-only file leaked into portable archive: {rel}")
            if rel in seen:
                raise ContractError(f"portable archive contains duplicate member: {rel}")
            mode = (info.external_attr >> 16) & 0o170000
            if mode == stat.S_IFLNK:
                raise ContractError(f"portable archive contains a symbolic link: {rel}")
            if info.date_time != PORTABLE_FIXED_TIME:
                raise ContractError(f"portable archive member has non-deterministic timestamp metadata: {rel}")
            if info.compress_type != zipfile.ZIP_DEFLATED:
                raise ContractError(f"portable archive member has unexpected compression method: {rel}")
            if info.create_system != 3 or info.external_attr != PORTABLE_FILE_MODE:
                raise ContractError(f"portable archive member has non-deterministic file metadata: {rel}")
            seen.add(rel)
            total_uncompressed += info.file_size
            if total_uncompressed > PORTABLE_MAX_UNCOMPRESSED_BYTES:
                raise ContractError("portable archive exceeds the bounded uncompressed-size limit")

            expected = expected_identity.get(rel)
            if expected is None:
                continue
            expected_size, expected_digest = expected
            if info.file_size != expected_size:
                raise ContractError(f"portable archive member size differs from staged runtime: {rel}")
            with handle.open(info, "r") as source:
                archive_digest = _sha256_stream(source)
            if archive_digest != expected_digest:
                raise ContractError(f"portable archive member content differs from staged runtime: {rel}")

        if seen != expected_files:
            missing = sorted(expected_files - seen)
            extras = sorted(seen - expected_files)
            pieces: list[str] = []
            if missing:
                pieces.append("missing=" + ", ".join(missing))
            if extras:
                pieces.append("extra=" + ", ".join(extras))
            raise ContractError("portable archive member set does not match staged runtime subset: " + "; ".join(pieces))

        with tempfile.TemporaryDirectory(prefix="brokedj-portable-verify-") as tmp:
            extracted_root = Path(tmp) / prefix
            for info in infos:
                member = _validate_portable_member(info.filename, prefix)
                rel = PurePosixPath(*member.parts[1:])
                destination = extracted_root.joinpath(*rel.parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                with handle.open(info, "r") as source, destination.open("wb") as output:
                    shutil.copyfileobj(source, output)
            _verify_extracted_portable_payload(root, extracted_root, version=version)


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
            "Development-artifact integrity plus deterministic runtime-subset portable packaging; "
            "not clean-machine, audio-device, controller, latency, listening, or live-performance qualification."
        ),
    }
    (root / MANIFEST_NAME).write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    (root / SUMS_NAME).write_text(_expected_sums(entries), encoding="utf-8", newline="\n")
    _write_portable_archive(root, version)


def verify_contract(
    root: Path,
    *,
    expected_commit: str | None = None,
    expected_version: str | None = None,
    verify_portable: bool = True,
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
        if path.stat().st_size != entry["bytes"]:
            raise ContractError(f"size mismatch for {entry['path']}")
        if _sha256(path) != entry["sha256"]:
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

    if verify_portable:
        _verify_portable_archive(root, commit=commit, version=version)

    return manifest


def _write_fixture_payload(root: Path, commit: str) -> None:
    for rel in REQUIRED_PATHS:
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        if rel == SOURCE_COMMIT_PATH:
            path.write_text(commit, encoding="utf-8")
        elif rel == "VERIFY-PACKAGE.py":
            shutil.copyfile(Path(__file__), path)
        else:
            path.write_bytes(("fixture:" + rel).encode("utf-8"))


def _extract_portable(root: Path, version: str, destination: Path) -> Path:
    prefix = _portable_root(version)
    with zipfile.ZipFile(root / PORTABLE_ZIP_NAME, "r") as handle:
        handle.extractall(destination)
    return destination / prefix


def _rewrite_archive_metadata(archive: Path, *, date_time: tuple[int, int, int, int, int, int]) -> None:
    replacement = archive.with_suffix(".metadata-test.zip")
    with zipfile.ZipFile(archive, "r") as source, zipfile.ZipFile(
        replacement, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as output:
        for original in source.infolist():
            info = zipfile.ZipInfo(original.filename, date_time)
            info.create_system = original.create_system
            info.compress_type = original.compress_type
            info.external_attr = original.external_attr
            info.flag_bits = 0
            output.writestr(info, source.read(original), compress_type=original.compress_type, compresslevel=9)
    replacement.replace(archive)


def self_test() -> None:
    commit = "0123456789abcdef0123456789abcdef01234567"
    version = "0.1.0"
    with tempfile.TemporaryDirectory(prefix="brokedj-package-contract-") as tmp:
        tmp_path = Path(tmp)
        root = tmp_path / "artifact"
        root.mkdir()
        _write_fixture_payload(root, commit)

        create_contract(root, commit, version)
        first_archive = (root / PORTABLE_ZIP_NAME).read_bytes()
        first_digest = _sha256(root / PORTABLE_ZIP_NAME)
        verified = verify_contract(root, expected_commit=commit, expected_version=version)
        if verified.get("file_count") != len(REQUIRED_PATHS):
            raise ContractError("self-test manifest count did not match fixture count")

        with zipfile.ZipFile(root / PORTABLE_ZIP_NAME, "r") as handle:
            prefix = _portable_root(version) + "/"
            rels = {
                PurePosixPath(info.filename).relative_to(_portable_root(version)).as_posix()
                for info in handle.infolist()
            }
            if any(rel in PORTABLE_FORBIDDEN_ROOT_FILES for rel in rels):
                raise ContractError("self-test portable leaked development-only root files")
            if not rels or any(not rel.startswith(PORTABLE_STAGE_PREFIX) for rel in rels):
                raise ContractError("self-test portable was not limited to the staged BrokeDJ runtime tree")
            if any(not info.filename.startswith(prefix) for info in handle.infolist()):
                raise ContractError("self-test portable member escaped versioned root")

        extracted = _extract_portable(root, version, tmp_path / "extract-ok")
        verify_extracted_portable(root, extracted, expected_commit=commit, expected_version=version)

        create_contract(root, commit, version)
        if (root / PORTABLE_ZIP_NAME).read_bytes() != first_archive or _sha256(root / PORTABLE_ZIP_NAME) != first_digest:
            raise ContractError("self-test portable archive is not deterministic for identical input")
        verify_contract(root, expected_commit=commit, expected_version=version)

        tampered = root / "BrokeDJ" / "README.md"
        tampered.write_text("tampered", encoding="utf-8")
        try:
            verify_contract(root, expected_commit=commit)
        except ContractError:
            pass
        else:
            raise ContractError("self-test failed to detect a tampered staged payload")

        _write_fixture_payload(root, commit)
        create_contract(root, commit, version)
        extracted_bad = _extract_portable(root, version, tmp_path / "extract-tamper")
        (extracted_bad / "BrokeDJ" / "README.md").write_text("tampered after extraction", encoding="utf-8")
        try:
            verify_extracted_portable(root, extracted_bad, expected_commit=commit)
        except ContractError:
            pass
        else:
            raise ContractError("self-test failed to reject tampered extracted portable")

        extracted_extra = _extract_portable(root, version, tmp_path / "extract-extra")
        (extracted_extra / "VERIFY-PACKAGE.py").write_text("leak", encoding="utf-8")
        try:
            verify_extracted_portable(root, extracted_extra, expected_commit=commit)
        except ContractError:
            pass
        else:
            raise ContractError("self-test failed to reject development-only file in extracted portable")

        _write_fixture_payload(root, commit)
        create_contract(root, commit, version)
        malicious = root / PORTABLE_ZIP_NAME
        with zipfile.ZipFile(malicious, "a", compression=zipfile.ZIP_DEFLATED) as handle:
            handle.writestr("../escape.txt", b"bad")
        (root / PORTABLE_SUM_NAME).write_text(
            f"{_sha256(malicious)}  {malicious.name}\n", encoding="utf-8", newline="\n"
        )
        try:
            verify_contract(root, expected_commit=commit)
        except ContractError:
            pass
        else:
            raise ContractError("self-test failed to reject portable archive path traversal")

        _write_fixture_payload(root, commit)
        create_contract(root, commit, version)
        divergent_root = tmp_path / "divergent-artifact"
        divergent_root.mkdir()
        _write_fixture_payload(divergent_root, commit)
        (divergent_root / "BrokeDJ" / "README.md").write_text("self-consistent divergent payload", encoding="utf-8")
        create_contract(divergent_root, commit, version)
        shutil.copyfile(divergent_root / PORTABLE_ZIP_NAME, root / PORTABLE_ZIP_NAME)
        shutil.copyfile(divergent_root / PORTABLE_SUM_NAME, root / PORTABLE_SUM_NAME)
        try:
            verify_contract(root, expected_commit=commit)
        except ContractError:
            pass
        else:
            raise ContractError("self-test failed to reject archive that differs from staged runtime")

        _write_fixture_payload(root, commit)
        create_contract(root, commit, version)
        metadata_archive = root / PORTABLE_ZIP_NAME
        _rewrite_archive_metadata(metadata_archive, date_time=(1981, 1, 1, 0, 0, 0))
        (root / PORTABLE_SUM_NAME).write_text(
            f"{_sha256(metadata_archive)}  {metadata_archive.name}\n", encoding="utf-8", newline="\n"
        )
        try:
            verify_contract(root, expected_commit=commit)
        except ContractError:
            pass
        else:
            raise ContractError("self-test failed to reject non-deterministic portable archive metadata")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    create = sub.add_parser("create", help="create development manifest/hashes plus runtime-subset portable ZIP")
    create.add_argument("--root", type=Path, required=True, help="artifact root (normally dist)")
    create.add_argument("--commit", required=True, help="full source commit SHA")
    version_source = create.add_mutually_exclusive_group(required=True)
    version_source.add_argument("--version", help="explicit BrokeDJ semantic version")
    version_source.add_argument("--cmake", type=Path, help="read BrokeDJ version from CMakeLists.txt")

    verify = sub.add_parser("verify", help="verify development manifest, hashes, portable ZIP and required payload")
    verify.add_argument("--root", type=Path, required=True, help="artifact root")
    verify.add_argument("--expected-commit", help="require this exact source commit SHA")
    verify.add_argument("--expected-version", help="require this exact semantic version")

    extracted = sub.add_parser(
        "verify-extracted-portable",
        help="verify an extracted portable root against the staged runtime subset",
    )
    extracted.add_argument("--root", type=Path, required=True, help="full staged development-artifact root")
    extracted.add_argument("--extracted-root", type=Path, required=True, help="versioned root extracted from the portable ZIP")
    extracted.add_argument("--expected-commit", help="require this exact source commit SHA")
    extracted.add_argument("--expected-version", help="require this exact semantic version")

    sub.add_parser("self-test", help="run deterministic create/tamper/archive/runtime-subset regression checks")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "create":
            version = args.version if args.version is not None else _read_project_version(args.cmake)
            create_contract(args.root, args.commit, version)
            print(
                f"created {args.root / MANIFEST_NAME}, {args.root / SUMS_NAME}, "
                f"{args.root / PORTABLE_ZIP_NAME} and {args.root / PORTABLE_SUM_NAME}"
            )
        elif args.command == "verify":
            manifest = verify_contract(
                args.root,
                expected_commit=args.expected_commit,
                expected_version=args.expected_version,
            )
            print(
                "verified BrokeDJ staged artifact and runtime-subset portable bundle: "
                f"{manifest['file_count']} staged files, version {manifest['version']}, "
                f"commit {manifest['source_commit']}"
            )
        elif args.command == "verify-extracted-portable":
            manifest = verify_extracted_portable(
                args.root,
                args.extracted_root,
                expected_commit=args.expected_commit,
                expected_version=args.expected_version,
            )
            print(
                "verified extracted BrokeDJ runtime subset: "
                f"version {manifest['version']}, commit {manifest['source_commit']}"
            )
        else:
            self_test()
            print("package contract self-test passed")
    except (ContractError, OSError, UnicodeError, zipfile.BadZipFile) as exc:
        print(f"package contract failure: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
