"""Text-file discovery and paragraph chunking for semantic search."""

from __future__ import annotations

import hashlib
import re
from pathlib import Path

DEFAULT_TEXT_EXTS = {
    ".md",
    ".rst",
    ".adoc",
    ".asciidoc",
    ".txt",
    ".yaml",
    ".yml",
    ".json",
    ".toml",
}


def is_text_file(path: Path, sample_size: int = 1024) -> bool:
    """Heuristic: file is text if it decodes as UTF-8 and contains no NUL bytes."""
    try:
        with open(path, "rb") as f:
            chunk = f.read(sample_size)
    except OSError:
        return False
    if b"\x00" in chunk:
        return False
    try:
        chunk.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


def discover_files(root: Path, exts: set[str] | None = None, subdir: str | None = None) -> list[str]:
    """Return relative paths of all text files under *root* (optionally scoped to *subdir*)."""
    root = Path(root).resolve()
    start = root
    if subdir:
        start = root / subdir
        if not start.exists():
            raise FileNotFoundError(f"sub-directory not found: {start}")

    exts = set(exts) if exts else DEFAULT_TEXT_EXTS

    files: list[str] = []
    for p in start.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix.lower() not in exts:
            continue
        if not is_text_file(p):
            continue
        files.append(p.relative_to(root).as_posix())

    return sorted(files)


def hash_file(root: Path, rel_path: str) -> tuple[str, float]:
    """Return (sha256_hex, mtime) for *rel_path* under *root*."""
    p = root / rel_path
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while True:
            data = f.read(65536)
            if not data:
                break
            h.update(data)
    return h.hexdigest(), p.stat().st_mtime


def chunk_text(text: str, max_chunk_chars: int = 1500, overlap_chars: int = 100) -> list[str]:
    """Split text into paragraph-sized chunks, respecting *max_chunk_chars*."""
    paragraphs = re.split(r"\n\s*\n", text.strip())
    chunks: list[str] = []
    for para in paragraphs:
        para = re.sub(r"\s+", " ", para).strip()
        if not para:
            continue
        if len(para) <= max_chunk_chars:
            chunks.append(para)
            continue

        # Long paragraph: overlap-split it.
        start = 0
        while start < len(para):
            end = start + max_chunk_chars
            piece = para[start:end]
            chunks.append(piece)
            if end >= len(para):
                break
            start = end - overlap_chars
    return chunks
