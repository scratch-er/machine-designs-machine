#!/usr/bin/env python3
"""Semantic search CLI for the semantic-search skill."""

from __future__ import annotations

import argparse
import sqlite3
import sys
from pathlib import Path

import numpy as np

from client import EmbeddingClient, load_config


def unpack_embedding(blob: bytes) -> np.ndarray:
    return np.frombuffer(blob, dtype=np.float32)


def normalize(v: np.ndarray) -> np.ndarray:
    norm = float(np.linalg.norm(v))
    return v if norm == 0 else v / norm


def normalize_subdir(subdir: str, root: Path) -> str:
    """Return a prefix relative to *root*, stripping a leading source-root component if present."""
    sub = Path(subdir)
    # If the user passed an absolute or repo-relative path that starts with the
    # source root, strip it so the prefix matches the stored relative paths.
    try:
        rel = sub.relative_to(root)
        return str(rel)
    except ValueError:
        pass
    root_name = root.name
    parts = sub.parts
    if parts and parts[0] == root_name:
        return str(Path(*parts[1:]))
    return subdir


def main() -> int:
    parser = argparse.ArgumentParser(description="Search the specs index by semantic similarity")
    parser.add_argument("--index", dest="index_path", help="path to the SQLite index (default: index_path from config)")
    parser.add_argument("--subdir", help="restrict results to a sub-directory of source_root")
    parser.add_argument("-n", "--topn", type=int, help="number of results to show")
    parser.add_argument("query", nargs="+", help="natural-language query")
    args = parser.parse_args()

    cfg = load_config()
    index_path: Path = Path(args.index_path) if args.index_path else cfg["index_path"]
    index_path = index_path.resolve()
    root: Path = cfg["source_root"]
    print(f"index path: {index_path}", file=sys.stderr)
    if not index_path.exists():
        print("Index not found. Run `uv run python index.py` first.", file=sys.stderr)
        return 1

    topn = args.topn if args.topn is not None else int(cfg["topn_default"])
    query = " ".join(args.query)

    conn = sqlite3.connect(index_path)
    if args.subdir:
        prefix = normalize_subdir(args.subdir, root).rstrip("/") + "/"
        print(f"sub-directory prefix: {prefix}", file=sys.stderr)
        rows = conn.execute(
            "SELECT id, path, text, embedding FROM chunks WHERE path LIKE ?", (prefix + "%",)
        ).fetchall()
    else:
        rows = conn.execute("SELECT id, path, text, embedding FROM chunks").fetchall()

    if not rows:
        print("No chunks found for the requested scope.", file=sys.stderr)
        return 0

    client = EmbeddingClient(cfg)
    qvec = normalize(np.array(client.embed([query])[0], dtype=np.float32))

    ids: list[str] = []
    paths: list[str] = []
    texts: list[str] = []
    vectors: list[np.ndarray] = []
    for cid, path, text, blob in rows:
        ids.append(cid)
        paths.append(path)
        texts.append(text)
        vectors.append(normalize(unpack_embedding(blob)))

    mat = np.vstack(vectors)
    scores = mat @ qvec
    top_idx = np.argsort(scores)[::-1][:topn]

    print(f"Query: {query}\n")
    for rank, idx in enumerate(top_idx, 1):
        print(f"{rank}. [{scores[idx]:.4f}] {paths[idx]}  ({ids[idx]})")
        snippet = texts[idx].replace("\n", " ")[:400]
        print(f"   {snippet}\n")

    return 0


if __name__ == "__main__":
    sys.exit(main())
