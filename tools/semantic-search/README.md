# semantic-search

Semantic search over the `specs/` documentation using an OpenAI-compatible
embedding API.

## Setup

This tool is part of the `machine-designs-machine` uv workspace. All
dependencies are managed from the repository root.

1. Install [uv](https://docs.astral.sh/uv/getting-started/installation/).

2. From the repository root, sync the workspace dependencies:

   ```bash
   uv sync --all-packages
   ```

   This creates a single `.venv/` at the repository root containing the
dependencies for all workspace members, including `semantic-search`.

## Quick start

1. Make sure the embedding server is running with embeddings enabled.
   For `llama-server` that means starting it with `--embeddings`:

   ```bash
   llama-server -m Qwen3-Embedding-0.6B-Q8_0.gguf --embeddings --port 8080
   ```

   > **macOS / Apple Silicon users:** `llama-server` builds around b9140–b9870
   > crash with heap corruption when running Qwen3-Embedding models on the
   > Metal backend (see [ggml-org/llama.cpp#23072](https://github.com/ggml-org/llama.cpp/issues/23072)).
   > The reliable workaround is to force CPU inference with `-ngl 0`:
   >
   > ```bash
   > llama-server -m Qwen3-Embedding-0.6B-Q8_0.gguf --embeddings --port 8080 -ngl 0
   > ```

2. Adjust `data/semantic-search/config.toml` if your server URL, model name,
   or API key differ.

3. Build the index:

   ```bash
   uv run tools/semantic-search/index.py \
       --source-root specs --index data/semantic-search/index.db
   ```

   The indexer is incremental: only changed or new files are re-embedded, and
   files deleted within the scanned scope are removed.  Use `--dry-run` to see
   which files would be indexed without doing any work.

4. Search:

   ```bash
   uv run tools/semantic-search/search.py \
       --index data/semantic-search/index.db "RISC-V privilege levels"
   uv run tools/semantic-search/search.py \
       --index data/semantic-search/index.db --subdir specs/riscv-isa-manual "atomic memory operations"
   ```

## Usage

All commands below are run from the repository root.

### Build or update the index

```bash
uv run tools/semantic-search/index.py --source-root specs --index data/semantic-search/index.db
```

### Rebuild the index from scratch

```bash
uv run tools/semantic-search/index.py --source-root specs --index data/semantic-search/index.db --rebuild
```

### Preview what would be indexed

```bash
uv run tools/semantic-search/index.py --source-root specs --index data/semantic-search/index.db --dry-run
```

### Search the whole index

```bash
uv run tools/semantic-search/search.py --index data/semantic-search/index.db "your question"
```

### Search a subset of specs

```bash
uv run tools/semantic-search/search.py --index data/semantic-search/index.db \
    --subdir specs/riscv-isa-manual "atomic memory operations"
```

## Index format

The index is a SQLite database with three tables:

- `meta(key, value)` – version, last update timestamp, model name.
- `documents(path PRIMARY KEY, mtime, hash)` – one row per source file.  The
  hash is SHA-256 of the file contents; it is used to skip unchanged files.
- `chunks(id PRIMARY KEY, path, text, embedding)` – one row per text chunk.
  The `embedding` column stores a compact binary `float32` vector.

## Configuration

See `data/semantic-search/config.toml`.  Notable options:

- `api_url`, `api_key`, `model` – embedding server settings.
- `source_root` – directory to index, relative to the repo root.
- `index_path` – SQLite database location, relative to the config file.
- `text_extensions` – which file types are treated as text.
- `max_chunk_chars` / `chunk_overlap_chars` – paragraph chunking parameters.
- `batch_size` – how many chunks are sent to the embedding API at once.

## Troubleshooting

### `llama-server` crashes with `Connection refused` during indexing

If the embedding server dies part-way through indexing and the client starts
reporting connection-refused errors, check the server log/crash report.  On
macOS with Apple Silicon the likely cause is the Metal-backend heap-corruption
bug documented in [ggml-org/llama.cpp#23072](https://github.com/ggml-org/llama.cpp/issues/23072).

**Fix:** restart `llama-server` with `-ngl 0` (CPU-only).  The indexer is
incremental, so after restarting the server you can simply re-run `index.py`
and it will continue from the last committed file.

If you prefer to keep GPU inference, try reducing `batch_size` in
`data/semantic-search/config.toml`, but note that this only delays the crash
on affected builds; CPU inference is the known-stable workaround.

### `ModuleNotFoundError: No module named 'numpy'`

Run `uv sync --all-packages` from the repository root. The workspace installs
all member dependencies into the shared `.venv` only when `--all-packages` is
used.
