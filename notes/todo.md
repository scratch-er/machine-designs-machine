# TODO

## Current state

- Project uses a single `uv` workspace at the repo root (`machine-designs-machine`).
- `tools/*` are workspace members; dependencies resolve into one root `.venv`.
- `semantic-search` is the only tool so far.
- Full spec index exists at `data/semantic-search/index.db`.
- Emulator design written in `notes/emulator-design.md`.
- Difftest design written in `notes/difftest-design.md`.
- Core design written in `notes/core-design.md`.
- Overall plan updated in `notes/plan.md`.

## Reminders for future work

- Run `uv sync --all-packages` from the repo root when dependencies change.
- Run `uv run tools/semantic-search/search.py --index data/semantic-search/index.db "query"` to search specs.
- Run `uv run tools/semantic-search/index.py --source-root specs --index data/semantic-search/index.db` to update the index after spec changes.
- Add future Python tools as workspace members under `tools/`.
- The `semantic-search` skill is available for spec questions.

## Pending tasks

- Set up the emulator build system in `emulator/`.
- Implement the decoder and basic RV32E interpreter.
- Add memory, CLINT, UART, and shell.
- Create a small assembly workload and run it on the emulator.
- Then begin the processor core.
