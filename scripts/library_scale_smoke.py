#!/usr/bin/env python3
"""Seed and qualify BrokeDJ's native no-audio lifecycle against a large local library.

This harness is intentionally destructive to the selected *CI fixture* database. It refuses
normal local execution unless --allow-local is supplied. It never opens or modifies music
files: every synthetic track path points at a deliberately nonexistent CI-only location.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
from typing import Any

TRACK_COUNT = 5_000
HISTORY_COUNT = 12_000
SMOKE_TIMEOUT_SECONDS = 30


def fail(message: str) -> "NoReturn":
    raise RuntimeError(message)


def run_gui_smoke(executable: Path, workdir: Path) -> tuple[float, dict[str, Any]]:
    report_path = workdir / "BrokeDJ-gui-smoke.json"
    if report_path.exists():
        report_path.unlink()

    started = time.perf_counter()
    try:
        completed = subprocess.run(
            [str(executable), "--smoke-test"],
            cwd=workdir,
            check=False,
            timeout=SMOKE_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as exc:
        fail(f"native GUI smoke exceeded {SMOKE_TIMEOUT_SECONDS}s: {exc}")
    elapsed_ms = (time.perf_counter() - started) * 1000.0

    if completed.returncode != 0:
        fail(f"native GUI smoke returned {completed.returncode}")
    if not report_path.is_file():
        fail("native GUI smoke did not produce BrokeDJ-gui-smoke.json")

    payload = json.loads(report_path.read_text(encoding="utf-8-sig"))
    if payload.get("schema_version") != 1:
        fail("unexpected GUI smoke schema")
    if payload.get("mode") != "native-resize-lifecycle":
        fail("unexpected GUI smoke mode")
    if payload.get("plays_audio") is not False or payload.get("opens_audio_device") is not False:
        fail("large-library smoke must remain no-audio and must not open an audio device")
    if payload.get("success") is not True or payload.get("step_count") != 4:
        fail("native GUI smoke did not complete all resize/lifecycle steps")
    steps = payload.get("steps")
    if not isinstance(steps, list) or len(steps) != 4:
        fail("native GUI smoke step array is inconsistent")
    for step in steps:
        if not isinstance(step, dict):
            fail("native GUI smoke step is not an object")
        if int(step.get("window_width", 0)) != int(step.get("requested_width", -1)):
            fail("native GUI smoke width mismatch")
        if int(step.get("window_height", 0)) != int(step.get("requested_height", -1)):
            fail("native GUI smoke height mismatch")
        if int(step.get("content_width", 0)) <= 0 or int(step.get("content_height", 0)) <= 0:
            fail("native GUI smoke content bounds are invalid")
    return elapsed_ms, payload


def require_production_schema(connection: sqlite3.Connection) -> int:
    required_tables = {"tracks", "history", "tags", "track_tags", "playlists", "playlist_items"}
    tables = {
        str(row[0])
        for row in connection.execute("SELECT name FROM sqlite_master WHERE type='table';")
    }
    missing = sorted(required_tables - tables)
    if missing:
        fail("production library schema is incomplete: " + ", ".join(missing))

    track_columns = {
        str(row[1])
        for row in connection.execute("PRAGMA table_info(tracks);")
    }
    expected_track_columns = {
        "id", "path", "file_size", "modified_ns", "title", "artist", "album",
        "duration_seconds", "bpm", "musical_key", "added_at_ms", "last_seen_at_ms",
        "content_hash", "missing",
    }
    if not expected_track_columns.issubset(track_columns):
        fail("tracks table does not match the production schema expected by the smoke harness")

    version = int(connection.execute("PRAGMA user_version;").fetchone()[0])
    if version <= 0:
        fail("production library schema has no positive user_version")
    quick_check = str(connection.execute("PRAGMA quick_check;").fetchone()[0])
    if quick_check != "ok":
        fail(f"SQLite quick_check failed before fixture seeding: {quick_check}")
    return version


def seed_fixture(database: Path) -> tuple[int, int, int]:
    connection = sqlite3.connect(database, timeout=2.5)
    try:
        version = require_production_schema(connection)
        existing_tracks = int(connection.execute("SELECT COUNT(*) FROM tracks;").fetchone()[0])
        existing_history = int(connection.execute("SELECT COUNT(*) FROM history;").fetchone()[0])
        if existing_tracks != 0 or existing_history != 0:
            fail(
                "refusing to seed a non-empty library database "
                f"(tracks={existing_tracks}, history={existing_history})"
            )

        tracks = []
        for index in range(TRACK_COUNT):
            synthetic_path = f"C:/BrokeDJ-CI/nonexistent/track-{index:05d}.wav"
            tracks.append(
                (
                    synthetic_path,
                    123,
                    42,
                    f"Scale Track {index:05d}",
                    f"Fixture Artist {index % 64:02d}",
                    f"Fixture Album {index % 32:02d}",
                    180.0 + (index % 180),
                    120.0 + (index % 9),
                    "",
                    1000 + index,
                    1000 + index,
                    f"{index + 1:064x}",
                    0,
                )
            )

        with connection:
            connection.executemany(
                "INSERT INTO tracks(path,file_size,modified_ns,title,artist,album,"
                "duration_seconds,bpm,musical_key,added_at_ms,last_seen_at_ms,content_hash,missing) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);",
                tracks,
            )
            history = [
                ((index % TRACK_COUNT) + 1, 100_000 + index)
                for index in range(HISTORY_COUNT)
            ]
            connection.executemany(
                "INSERT INTO history(track_id,played_at_ms) VALUES(?,?);",
                history,
            )

        track_count = int(connection.execute("SELECT COUNT(*) FROM tracks;").fetchone()[0])
        history_count = int(connection.execute("SELECT COUNT(*) FROM history;").fetchone()[0])
        if track_count != TRACK_COUNT or history_count != HISTORY_COUNT:
            fail(
                "large-library fixture count mismatch "
                f"(tracks={track_count}, history={history_count})"
            )
        quick_check = str(connection.execute("PRAGMA quick_check;").fetchone()[0])
        if quick_check != "ok":
            fail(f"SQLite quick_check failed after fixture seeding: {quick_check}")
        return version, track_count, history_count
    finally:
        connection.close()


def verify_fixture(database: Path) -> tuple[int, int, int, str]:
    connection = sqlite3.connect(database, timeout=2.5)
    try:
        version = require_production_schema(connection)
        track_count = int(connection.execute("SELECT COUNT(*) FROM tracks;").fetchone()[0])
        history_count = int(connection.execute("SELECT COUNT(*) FROM history;").fetchone()[0])
        quick_check = str(connection.execute("PRAGMA quick_check;").fetchone()[0])
        if track_count != TRACK_COUNT or history_count != HISTORY_COUNT:
            fail(
                "native application changed deterministic fixture cardinality unexpectedly "
                f"(tracks={track_count}, history={history_count})"
            )
        if quick_check != "ok":
            fail(f"SQLite quick_check failed after native lifecycle smoke: {quick_check}")
        return version, track_count, history_count, quick_check
    finally:
        connection.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--workdir", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument(
        "--allow-local",
        action="store_true",
        help="Allow destructive use outside GitHub Actions. Never point this at a real library.",
    )
    args = parser.parse_args()

    if os.environ.get("GITHUB_ACTIONS", "").lower() != "true" and not args.allow_local:
        fail("refusing destructive fixture seeding outside GitHub Actions without --allow-local")

    executable = args.exe.resolve()
    database = args.database.resolve()
    workdir = args.workdir.resolve()
    report = args.report.resolve()
    if not executable.is_file():
        fail(f"BrokeDJ executable does not exist: {executable}")
    workdir.mkdir(parents=True, exist_ok=True)
    report.parent.mkdir(parents=True, exist_ok=True)

    preflight_ms, _preflight = run_gui_smoke(executable, workdir)
    if not database.is_file():
        fail(
            "preflight GUI launch did not create the expected production library database: "
            f"{database}"
        )

    schema_version, seeded_tracks, seeded_history = seed_fixture(database)
    loaded_ms, loaded = run_gui_smoke(executable, workdir)
    schema_after, tracks_after, history_after, quick_check = verify_fixture(database)
    if schema_after != schema_version:
        fail("schema version changed unexpectedly during the native lifecycle smoke")

    payload = {
        "schema_version": 1,
        "mode": "native-large-library-lifecycle",
        "plays_audio": False,
        "opens_audio_device": False,
        "fixture": {
            "tracks": seeded_tracks,
            "history_rows": seeded_history,
            "database_schema_version": schema_version,
            "synthetic_paths_only": True,
            "music_files_created_or_modified": False,
        },
        "preflight_gui_elapsed_ms": round(preflight_ms, 3),
        "large_library_gui_elapsed_ms": round(loaded_ms, 3),
        "post_smoke_tracks": tracks_after,
        "post_smoke_history_rows": history_after,
        "sqlite_quick_check": quick_check,
        "gui_contract": {
            "mode": loaded.get("mode"),
            "step_count": loaded.get("step_count"),
            "success": loaded.get("success"),
        },
        "success": True,
        "qualification_note": (
            "Automated Windows no-audio startup/resize evidence with a 5,000-track/12,000-history "
            "local SQLite fixture. Elapsed time is diagnostic only. This does not replace an "
            "interactive Windows 11 import/search review, HiDPI review, physical audio-device "
            "qualification, controller testing, listening, or live-readiness gates."
        ),
    }
    report.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # CI contract: one clear fatal diagnostic and non-zero exit.
        print(f"library-scale-smoke: {exc}", file=sys.stderr)
        sys.exit(2)
