"""Shared config loader and OpenAI-compatible embedding client."""

from __future__ import annotations

import sys
import time
import tomllib
from pathlib import Path

import requests


def _repo_root() -> Path:
    # tools/semantic-search/<script> -> tools -> repo root
    return Path(__file__).resolve().parents[2]


def load_config() -> dict:
    repo = _repo_root()
    config_path = repo / "data" / "semantic-search" / "config.toml"
    if not config_path.exists():
        raise FileNotFoundError(f"config not found: {config_path}")

    with open(config_path, "rb") as f:
        cfg = tomllib.load(f)

    cfg.setdefault("api_url", "http://127.0.0.1:8080")
    cfg.setdefault("api_key", "not-needed")
    cfg.setdefault("model", "Qwen3-Embedding-0.6B-Q8_0.gguf")
    cfg.setdefault("batch_size", 32)
    cfg.setdefault("max_retries", 3)
    cfg.setdefault("request_timeout", 120)
    cfg.setdefault("source_root", "specs")
    cfg.setdefault("index_path", "index.db")
    cfg.setdefault("text_extensions", [".md", ".rst", ".txt"])
    cfg.setdefault("max_chunk_chars", 1500)
    cfg.setdefault("chunk_overlap_chars", 100)
    cfg.setdefault("topn_default", 5)

    # Resolve paths relative to sensible anchors.
    source_root = Path(cfg["source_root"])
    if not source_root.is_absolute():
        source_root = repo / source_root
    cfg["source_root"] = source_root.resolve()

    index_path = Path(cfg["index_path"])
    if not index_path.is_absolute():
        index_path = config_path.parent / index_path
    cfg["index_path"] = index_path.resolve()

    return cfg


class EmbeddingClient:
    def __init__(self, cfg: dict):
        self.base_url = cfg["api_url"].rstrip("/")
        self.api_key = cfg["api_key"]
        self.model = cfg["model"]
        self.batch_size = int(cfg["batch_size"])
        self.max_retries = int(cfg["max_retries"])
        self.timeout = int(cfg["request_timeout"])
        self.retry_backoff = float(cfg.get("retry_backoff_seconds", 2))

    def _post_with_retry(self, url: str, payload: dict, headers: dict) -> dict:
        last_err: Exception | None = None
        for attempt in range(self.max_retries):
            try:
                resp = requests.post(url, json=payload, headers=headers, timeout=self.timeout)
                try:
                    resp.raise_for_status()
                except requests.HTTPError as http_err:
                    if resp.status_code == 501:
                        raise RuntimeError(
                            "The embedding server does not have embeddings enabled. "
                            "If you are using llama-server, restart it with `--embeddings` "
                            "(or `--embedding`)."
                        ) from http_err
                    raise
                return resp.json()
            except requests.ConnectionError as e:
                last_err = e
                # The local server may be restarting; wait a bit longer.
                time.sleep(self.retry_backoff * (2 ** attempt))
            except Exception as e:  # noqa: BLE001
                last_err = e
                time.sleep(self.retry_backoff * (2 ** attempt))
        raise RuntimeError(f"embedding failed after {self.max_retries} retries: {last_err}")

    def embed(self, texts: list[str]) -> list[list[float]]:
        if not texts:
            return []

        url = f"{self.base_url}/v1/embeddings"
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        all_embeddings: list[list[float]] = []
        total = len(texts)

        for i in range(0, total, self.batch_size):
            batch = texts[i : i + self.batch_size]
            payload = {"input": batch, "model": self.model}
            data = self._post_with_retry(url, payload, headers)["data"]
            data.sort(key=lambda x: x.get("index", 0))
            embeddings = [d["embedding"] for d in data]
            if len(embeddings) != len(batch):
                raise RuntimeError(
                    f"embedding count mismatch: got {len(embeddings)}, expected {len(batch)}"
                )
            all_embeddings.extend(embeddings)
            print(
                f"embedded {min(i + self.batch_size, total):5d} / {total}",
                file=sys.stderr,
            )

        return all_embeddings
