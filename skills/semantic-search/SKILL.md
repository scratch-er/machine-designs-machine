---
name: semantic-search
description: Search the project specifications (`specs/`) using semantic similarity over a pre-built embedding index.
type: prompt
whenToUse: When I need to look up RISC-V or project specifications while designing the emulator or processor core.
arguments: query
---

I have a semantic-search index over `specs/`. Use it whenever I need accurate
details from the RISC-V ISA manual or project specifications while working on
the emulator or processor core.

Run a search, review the top results, and then use the information to answer
the design question or continue implementation.

## Usage

From the repository root:

```bash
uv run tools/semantic-search/search.py --index data/semantic-search/index.db "<natural-language query>"
```

To search only a sub-directory of `specs/`:

```bash
uv run tools/semantic-search/search.py --index data/semantic-search/index.db --subdir specs/riscv-isa-manual "atomic memory operations"
```

To update the index after specs change:

```bash
uv run tools/semantic-search/index.py --source-root specs --index data/semantic-search/index.db
```

## When to run

- Before implementing a new instruction class, CSR, privilege mechanism, or
  memory model behavior.
- When I am unsure about encoding, semantics, or trap behavior.
- When I need to verify that my interpretation of the spec matches the source.

## Good queries

- `RISC-V privilege levels and CSR access rules`
- `atomic memory operations LR SC AMO semantics`
- `mstatus MPP SPP interrupt delegation`
- `RV32I instruction encoding addi lui auipc`
- `physical memory attribution PMA`

Configuration is read from `data/semantic-search/config.toml`. The generated
index is stored in `data/semantic-search/index.db`.

Now run the search for: $query
