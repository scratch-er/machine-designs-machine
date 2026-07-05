#!/usr/bin/env python3
"""Incremental indexer for the semantic-search skill.

Only changed or new files are re-embedded, and files deleted within the scanned
scope are removed from the index.  This behaves like a Makefile: work is done
only for inputs that changed.
"""

from __future__ import annotations

import argparse
import sqlite3
import struct
import sys
import time
from pathlib import Path

from chunking import chunk_text, discover_files, hash_file
from client import EmbeddingClient, load_config


DB_VERSION = "1"


def init_db(conn: sqlite3.Connection) -> None:
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS meta (
            key TEXT PRIMARY KEY,
            value TEXT
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS documents (
            path TEXT PRIMARY KEY,
            mtime REAL,
            hash TEXT
        )
        """
    )
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS chunks (
            id TEXT PRIMARY KEY,
            path TEXT NOT NULL,
            text TEXT,
            embedding BLOB
        )
        """
    )
    conn.execute("CREATE INDEX IF NOT EXISTS idx_chunks_path ON chunks(path)")
    conn.commit()


def pack_embedding(vec: list[float]) -> bytes:
    return struct.pack(f"{len(vec)}f", *vec)


def load_existing_docs(conn: sqlite3.Connection) -> dict[str, dict[str, float | str]]:
    rows = conn.execute("SELECT path, mtime, hash FROM documents").fetchall()
    return {r[0]: {"mtime": r[1], "hash": r[2]} for r in rows}


def main() -> int:
    parser = argparse.ArgumentParser(description="Build or update the semantic search index")
    parser.add_argument("--source-root", dest="source_root", help="directory to index (default: source_root from config)")
    parser.add_argument("--index", dest="index_path", help="path to the SQLite index (default: index_path from config)")
    parser.add_argument("--subdir", help="only index files under this sub-directory of source_root")
    parser.add_argument("--rebuild", action="store_true", help="drop the existing index and rebuild")
    parser.add_argument("--dry-run", action="store_true", help="list files that would be indexed and exit")
    args = parser.parse_args()

    cfg = load_config()
    root: Path = Path(args.source_root) if args.source_root else cfg["source_root"]
    root = root.resolve()
    index_path: Path = Path(args.index_path) if args.index_path else cfg["index_path"]
    index_path = index_path.resolve()
    index_path.parent.mkdir(parents=True, exist_ok=True)

    exts = set(cfg["text_extensions"])
    max_chunk = int(cfg["max_chunk_chars"])
    overlap = int(cfg["chunk_overlap_chars"])

    print(f"source root: {root}", file=sys.stderr)
    print(f"index path:  {index_path}", file=sys.stderr)

    conn = sqlite3.connect(index_path)
    init_db(conn)

    if args.rebuild:
        conn.execute("DELETE FROM documents")
        conn.execute("DELETE FROM chunks")
        conn.commit()

    existing = load_existing_docs(conn)
    files = discover_files(root, exts, args.subdir)

    if args.dry_run:
        ext_counts: dict[str, int] = {}
        for rel in files:
            ext = Path(rel).suffix.lower() or "(no ext)"
            ext_counts[ext] = ext_counts.get(ext, 0) + 1
        print(f"Would index {len(files)} text files under {root}", file=sys.stderr)
        if args.subdir:
            print(f"  scoped to sub-directory: {args.subdir}", file=sys.stderr)
        print("Extension counts:", file=sys.stderr)
        for ext, count in sorted(ext_counts.items(), key=lambda x: -x[1]):
            print(f"  {ext}: {count}", file=sys.stderr)
        return 0

    changed: list[tuple[str, str, float]] = []
    unchanged: set[str] = set()
    for rel in files:
        h, mtime = hash_file(root, rel)
        if rel in existing and existing[rel]["hash"] == h:
            unchanged.add(rel)
        else:
            changed.append((rel, h, mtime))

    # Files removed within the current scope.
    changed_set = {c[0] for c in changed}
    if args.subdir:
        prefix = args.subdir.rstrip("/") + "/"
        removed = [
            p
            for p in existing
            if p.startswith(prefix) and p not in unchanged and p not in changed_set
        ]
    else:
        removed = [p for p in existing if p not in unchanged and p not in changed_set]

    print(
        f"scanning {len(files)} files: {len(changed)} changed/new, {len(removed)} removed",
        file=sys.stderr,
    )

    client = EmbeddingClient(cfg)

    for p in removed:
        conn.execute("DELETE FROM documents WHERE path = ?", (p,))
        conn.execute("DELETE FROM chunks WHERE path = ?", (p,))

    for rel, h, mtime in changed:
        conn.execute("DELETE FROM chunks WHERE path = ?", (rel,))
        text = (root / rel).read_text(encoding="utf-8", errors="ignore")
        chunks = chunk_text(text, max_chunk, overlap)
        if chunks:
            print(f"indexing {rel}: {len(chunks)} chunks", file=sys.stderr)
            embeddings = client.embed(chunks)
            for i, (txt, emb) in enumerate(zip(chunks, embeddings)):
                cid = f"{rel}#{i}"
                conn.execute(
                    "INSERT OR REPLACE INTO chunks(id, path, text, embedding) VALUES (?, ?, ?, ?)",
                    (cid, rel, txt, pack_embedding(emb)),
                )
        conn.execute(
            "INSERT OR REPLACE INTO documents(path, mtime, hash) VALUES (?, ?, ?)",
            (rel, mtime, h),
        )
        conn.commit()

    conn.execute("INSERT OR REPLACE INTO meta(key, value) VALUES (?, ?)", ("version", DB_VERSION))
    conn.execute(
        "INSERT OR REPLACE INTO meta(key, value) VALUES (?, ?)",
        ("updated", str(int(time.time()))),
    )
    conn.execute("INSERT OR REPLACE INTO meta(key, value) VALUES (?, ?)", ("model", cfg["model"]))
    conn.commit()
    conn.close()

    print(f"index ready: {index_path}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
