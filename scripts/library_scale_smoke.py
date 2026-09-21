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
from typing import Any, NoReturn

TRACK_COUNT = 5_000
HISTORY_COUNT = 12_000
PLAYLIST_COUNT = 8
TAG_COUNT = 32
PLAYLIST_ITEM_COUNT = TRACK_COUNT
TRACK_TAG_COUNT = TRACK_COUNT
PLAYLIST_BROWSE_LIMIT = 500
SMOKE_TIMEOUT_SECONDS = 30
DUPLICATE_HASH = "d" * 64


def fail(message: str) -> NoReturn:
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


def playlist_query(
    connection: sqlite3.Connection,
    playlist_id: int,
    query: str,
    limit: int = PLAYLIST_BROWSE_LIMIT,
) -> list[int]:
    """Mirror the bounded SQL semantics used by the native playlist browser."""
    limit = max(1, min(int(limit), PLAYLIST_BROWSE_LIMIT))
    prefix = (
        "SELECT t.id FROM playlist_items pi JOIN tracks t ON t.id=pi.track_id "
        "WHERE pi.playlist_id=? "
    )
    parameters: list[Any] = [playlist_id]
    if query == "is:duplicate":
        predicate = (
            "AND t.missing=0 AND t.content_hash<>'' "
            "AND t.content_hash IN(SELECT content_hash FROM tracks WHERE missing=0 "
            "AND content_hash<>'' GROUP BY content_hash HAVING COUNT(*)>1) "
        )
    elif query == "is:missing":
        predicate = "AND t.missing=1 "
    else:
        predicate = (
            "AND (?='' OR instr(lower(t.title),lower(?))>0 "
            "OR instr(lower(t.artist),lower(?))>0 "
            "OR instr(lower(t.album),lower(?))>0 "
            "OR instr(lower(t.path),lower(?))>0 "
            "OR EXISTS(SELECT 1 FROM track_tags tt JOIN tags g ON g.id=tt.tag_id "
            "WHERE tt.track_id=t.id AND instr(lower(g.name),lower(?))>0)) "
        )
        parameters.extend([query] * 6)
    parameters.append(limit)
    rows = connection.execute(
        prefix + predicate + "ORDER BY pi.position,pi.rowid LIMIT ?;",
        parameters,
    ).fetchall()
    return [int(row[0]) for row in rows]


def validate_playlist_browse_contract(connection: sqlite3.Connection) -> dict[str, int]:
    playlist_one_count = int(
        connection.execute(
            "SELECT COUNT(*) FROM playlist_items WHERE playlist_id=1;"
        ).fetchone()[0]
    )
    if playlist_one_count <= PLAYLIST_BROWSE_LIMIT:
        fail("playlist fixture must exceed the native browse limit")

    bounded = playlist_query(connection, 1, "")
    if len(bounded) != PLAYLIST_BROWSE_LIMIT:
        fail("playlist browse did not enforce the 500-row result bound")

    title_match = playlist_query(connection, 1, "Scale Track 00000")
    if title_match != [1]:
        fail(f"playlist title search mismatch: {title_match}")

    tag_match = playlist_query(connection, 1, "tag-00")
    if not tag_match or 1 not in tag_match:
        fail("playlist tag search did not resolve the seeded tag relationship")

    duplicate_match = playlist_query(connection, 1, "is:duplicate")
    if duplicate_match != [1, 9]:
        fail(f"playlist duplicate directive mismatch: {duplicate_match}")

    missing_match = playlist_query(connection, 1, "is:missing")
    if missing_match != [17]:
        fail(f"playlist missing directive mismatch: {missing_match}")

    return {
        "playlist_1_rows": playlist_one_count,
        "bounded_rows": len(bounded),
        "title_matches": len(title_match),
        "tag_matches": len(tag_match),
        "duplicate_matches": len(duplicate_match),
        "missing_matches": len(missing_match),
    }


def seed_fixture(database: Path) -> tuple[int, dict[str, int]]:
    connection = sqlite3.connect(database, timeout=2.5)
    try:
        version = require_production_schema(connection)
        existing = {
            "tracks": int(connection.execute("SELECT COUNT(*) FROM tracks;").fetchone()[0]),
            "history": int(connection.execute("SELECT COUNT(*) FROM history;").fetchone()[0]),
            "playlists": int(connection.execute("SELECT COUNT(*) FROM playlists;").fetchone()[0]),
            "playlist_items": int(connection.execute("SELECT COUNT(*) FROM playlist_items;").fetchone()[0]),
            "tags": int(connection.execute("SELECT COUNT(*) FROM tags;").fetchone()[0]),
            "track_tags": int(connection.execute("SELECT COUNT(*) FROM track_tags;").fetchone()[0]),
        }
        if any(existing.values()):
            fail(f"refusing to seed a non-empty library database: {existing}")

        tracks = []
        for index in range(TRACK_COUNT):
            synthetic_path = f"C:/BrokeDJ-CI/nonexistent/track-{index:05d}.wav"
            content_hash = DUPLICATE_HASH if index in (0, 8) else f"{index + 1:064x}"
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
                    content_hash,
                    1 if index == 16 else 0,
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
            playlists = [
                (index + 1, f"Scale Playlist {index + 1:02d}", 200_000 + index)
                for index in range(PLAYLIST_COUNT)
            ]
            connection.executemany(
                "INSERT INTO playlists(id,name,created_at_ms) VALUES(?,?,?);",
                playlists,
            )
            playlist_positions = [0] * PLAYLIST_COUNT
            playlist_items = []
            for index in range(TRACK_COUNT):
                playlist_index = index % PLAYLIST_COUNT
                playlist_items.append(
                    (playlist_index + 1, index + 1, playlist_positions[playlist_index])
                )
                playlist_positions[playlist_index] += 1
            connection.executemany(
                "INSERT INTO playlist_items(playlist_id,track_id,position) VALUES(?,?,?);",
                playlist_items,
            )
            tags = [(index + 1, f"tag-{index:02d}") for index in range(TAG_COUNT)]
            connection.executemany("INSERT INTO tags(id,name) VALUES(?,?);", tags)
            track_tags = [
                (index + 1, (index % TAG_COUNT) + 1)
                for index in range(TRACK_COUNT)
            ]
            connection.executemany(
                "INSERT INTO track_tags(track_id,tag_id) VALUES(?,?);",
                track_tags,
            )

        counts = {
            "tracks": int(connection.execute("SELECT COUNT(*) FROM tracks;").fetchone()[0]),
            "history": int(connection.execute("SELECT COUNT(*) FROM history;").fetchone()[0]),
            "playlists": int(connection.execute("SELECT COUNT(*) FROM playlists;").fetchone()[0]),
            "playlist_items": int(connection.execute("SELECT COUNT(*) FROM playlist_items;").fetchone()[0]),
            "tags": int(connection.execute("SELECT COUNT(*) FROM tags;").fetchone()[0]),
            "track_tags": int(connection.execute("SELECT COUNT(*) FROM track_tags;").fetchone()[0]),
        }
        expected = {
            "tracks": TRACK_COUNT,
            "history": HISTORY_COUNT,
            "playlists": PLAYLIST_COUNT,
            "playlist_items": PLAYLIST_ITEM_COUNT,
            "tags": TAG_COUNT,
            "track_tags": TRACK_TAG_COUNT,
        }
        if counts != expected:
            fail(f"large-library fixture count mismatch: got={counts} expected={expected}")
        playlist_contract = validate_playlist_browse_contract(connection)
        quick_check = str(connection.execute("PRAGMA quick_check;").fetchone()[0])
        if quick_check != "ok":
            fail(f"SQLite quick_check failed after fixture seeding: {quick_check}")
        return version, counts | playlist_contract
    finally:
        connection.close()


def verify_fixture(database: Path) -> tuple[int, dict[str, int], str]:
    connection = sqlite3.connect(database, timeout=2.5)
    try:
        version = require_production_schema(connection)
        counts = {
            "tracks": int(connection.execute("SELECT COUNT(*) FROM tracks;").fetchone()[0]),
            "history": int(connection.execute("SELECT COUNT(*) FROM history;").fetchone()[0]),
            "playlists": int(connection.execute("SELECT COUNT(*) FROM playlists;").fetchone()[0]),
            "playlist_items": int(connection.execute("SELECT COUNT(*) FROM playlist_items;").fetchone()[0]),
            "tags": int(connection.execute("SELECT COUNT(*) FROM tags;").fetchone()[0]),
            "track_tags": int(connection.execute("SELECT COUNT(*) FROM track_tags;").fetchone()[0]),
        }
        expected = {
            "tracks": TRACK_COUNT,
            "history": HISTORY_COUNT,
            "playlists": PLAYLIST_COUNT,
            "playlist_items": PLAYLIST_ITEM_COUNT,
            "tags": TAG_COUNT,
            "track_tags": TRACK_TAG_COUNT,
        }
        if counts != expected:
            fail(
                "native application changed deterministic fixture cardinality unexpectedly "
                f"(got={counts} expected={expected})"
            )
        playlist_contract = validate_playlist_browse_contract(connection)
        quick_check = str(connection.execute("PRAGMA quick_check;").fetchone()[0])
        if quick_check != "ok":
            fail(f"SQLite quick_check failed after native lifecycle smoke: {quick_check}")
        return version, counts | playlist_contract, quick_check
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

    schema_version, seeded = seed_fixture(database)
    loaded_ms, loaded = run_gui_smoke(executable, workdir)
    schema_after, after, quick_check = verify_fixture(database)
    if schema_after != schema_version:
        fail("schema version changed unexpectedly during the native lifecycle smoke")

    payload = {
        "schema_version": 2,
        "mode": "native-large-library-lifecycle",
        "plays_audio": False,
        "opens_audio_device": False,
        "fixture": {
            "tracks": seeded["tracks"],
            "history_rows": seeded["history"],
            "playlists": seeded["playlists"],
            "playlist_items": seeded["playlist_items"],
            "tags": seeded["tags"],
            "track_tags": seeded["track_tags"],
            "synthetic_paths_only": True,
            "music_files_created_or_modified": False,
        },
        "playlist_browse_contract": {
            "result_limit": PLAYLIST_BROWSE_LIMIT,
            "playlist_1_rows": seeded["playlist_1_rows"],
            "bounded_rows": seeded["bounded_rows"],
            "title_matches": seeded["title_matches"],
            "tag_matches": seeded["tag_matches"],
            "duplicate_matches": seeded["duplicate_matches"],
            "missing_matches": seeded["missing_matches"],
        },
        "preflight_gui_elapsed_ms": round(preflight_ms, 3),
        "large_library_gui_elapsed_ms": round(loaded_ms, 3),
        "post_smoke": after,
        "database_schema_version": schema_version,
        "sqlite_quick_check": quick_check,
        "gui_contract": {
            "mode": loaded.get("mode"),
            "step_count": loaded.get("step_count"),
            "success": loaded.get("success"),
        },
        "success": True,
        "qualification_note": (
            "Automated Windows no-audio startup/resize evidence with a 5,000-track/12,000-history "
            "local SQLite fixture plus deterministic playlist/tag membership and bounded playlist-query "
            "semantics. Elapsed time is diagnostic only. This does not replace an interactive Windows 11 "
            "import/search/tag/playlist review, HiDPI review, physical audio-device qualification, controller "
            "testing, listening, or live-readiness gates."
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
