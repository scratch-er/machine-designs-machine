# Agent Skills

Agent Skills are a lightweight mechanism for extending model capabilities. A Skill is a Markdown document with YAML frontmatter that describes a specialized area of knowledge or a workflow — for example, a project's code style guidelines, a PR review process, or a commit message format.

## Creating a Skill

Skill files must be placed in a [known scan directory](#skill-locations). Two file structures are supported:

- **Directory form (recommended)**: Create a subdirectory under the Skills directory, name the main file `SKILL.md`, and place scripts, reference materials, and other supporting files in the same directory. When both `<name>/SKILL.md` and a same-named `<name>.md` exist in the same directory, the subdirectory takes precedence.
- **Flat form**: Use a single `.md` file directly; the Skill name is taken from the filename (minus `.md`).

### File Format

`SKILL.md` consists of two parts: YAML frontmatter and a Markdown body:

```markdown
---
name: code-style
description: Project code style guidelines defining naming, indentation, comments, and file organization
type: prompt
whenToUse: When the user asks me to write, modify, or review project source code
disableModelInvocation: false
arguments:
  - target
  - mode
---

Please handle code according to the following guidelines:

- Use 2-space indentation
- Variable names use `camelCase`, type names use `PascalCase`
- Public functions must have TSDoc comments
- Lines must not exceed 100 characters
```

### Frontmatter Fields

| Field | Description |
| --- | --- |
| `name` | Skill name. Required in a directory-form `SKILL.md`; when omitted in a flat `.md` file, the filename is used. Names are case-insensitive |
| `description` | A one-line summary; the model uses this to decide when to use the Skill. Required in a directory-form `SKILL.md`; when omitted in a flat `.md` file, falls back to the first non-empty line of the body (up to 240 characters) |
| `type` | Skill type: `prompt` (default), `inline` (same semantics as `prompt`), `flow` (manual invocation only; not available for automatic model invocation). Other values are skipped |
| `whenToUse` | Description of when the Skill should be triggered. Also accepts `when-to-use` and `when_to_use` |
| `disableModelInvocation` | When set to `true`, prevents the model from invoking this Skill automatically. Also accepts `disable-model-invocation` and `disable_model_invocation` |
| `arguments` | List of named parameters; can be written as a string array or a whitespace-separated string (e.g., `arguments: target mode`). Once declared, parameters can be read in the body with `$<name>` |

::: warning Note
In a directory-form `SKILL.md`, both `name` and `description` **must** be explicitly provided. Omitting either one will cause parsing to fail.
:::

## Skill Locations

The search path of skills is `.agents/skills/` in a project. In this project, it is a symlink to `skills/`, and you should write skills to `skills/`.

## Complete Example

```markdown
---
name: review-pr
description: Review a Pull Request according to team standards and produce a structured review report
type: prompt
whenToUse: When the user asks me to review a PR, inspect code changes, or evaluate commit quality
arguments:
  - pr_ref
---

Please review the PR the user specified: $pr_ref

1. Fetch and read the full diff for `$pr_ref`.
2. Check each of the following items:
   - Whether corresponding test cases are included
   - Whether public API documentation has been updated
   - Whether new dependencies have been introduced; if so, state the reason
   - Whether error handling covers edge cases
3. Refer to the checklist in the same directory: `references/checklist.md`
4. Produce a review report containing:
   - Overall conclusion (approve / request changes / comment)
   - Required changes (blocking)
   - Suggested improvements (non-blocking)
   - Noteworthy positives
```

Save this as `$KIMI_CODE_HOME/skills/review-pr/SKILL.md` (or `~/.kimi-code/skills/review-pr/SKILL.md` when `KIMI_CODE_HOME` is unset), place the checklist at `references/checklist.md` in the same directory, and after starting a new session you can invoke it with `/skill:review-pr #1234`, where `#1234` is expanded into `$pr_ref`.
