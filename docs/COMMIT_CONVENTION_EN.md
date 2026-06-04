# Git Commit Message Guidelines (Angular Convention)

**English | [中文](COMMIT_CONVENTION.md)**

This project follows the [Angular Commit Message Convention](https://github.com/angular/angular/blob/main/CONTRIBUTING.md#commit), aiming to improve readability and maintainability through a unified commit message format.

Benefits of standardized commit messages:

- **Auto-generate CHANGELOG**: Tools can automatically categorize changes by type
- **Quick change tracking**: Filter module-specific history via scope
- **Semantic versioning**: Determine version bump level (major/minor/patch) based on type
- **Better collaboration**: Team members can understand commit intent without reading code

---

## Commit Message Format

Each commit message consists of three parts: **Header**, **Body**, and **Footer**, separated by blank lines.

```
(Required) <type>(<scope>): <subject>
(blank line)
(Optional) <body>
(blank line)
(Optional) <footer>
```

**Example**

```
feat(spi): add DMA transfer mode support

1. Add DMA transfer channel configuration in SPI driver
2. Support high-speed bulk data transfer, reducing CPU usage
3. Original polling mode remains unchanged, DMA mode enabled via config

Closes #45
```

> **Note**: Header is the only required part. Body and Footer are optional based on commit complexity.
> For simple changes (e.g., fixing a typo), Header alone is sufficient.

---

## Header Details (Required)

The Header is the first line and most important part. Format:

```
<type>(<scope>): <subject>
```

| Field     | Required | Description                              |
| --------- | -------- | ---------------------------------------- |
| `type`    | Yes      | Type of commit, see table below          |
| `scope`   | No       | Module or area affected by this commit   |
| `subject` | Yes      | Brief description of what was done       |

### Type

| Type       | Description                                                          | Version Impact |
| ---------- | -------------------------------------------------------------------- | -------------- |
| `feat`     | New feature or capability                                            | minor          |
| `fix`      | Bug fix                                                              | patch          |
| `docs`     | Documentation only (README, comments, API docs)                      | none           |
| `style`    | Code formatting, no logic change (whitespace, indentation, etc.)     | none           |
| `refactor` | Code restructuring, neither new feature nor bug fix                  | none           |
| `perf`     | Performance improvement                                              | patch          |
| `test`     | Adding or fixing tests                                               | none           |
| `build`    | Build system or external dependency changes (Makefile, linker, etc.) | none           |
| `ci`       | CI/CD configuration changes (GitHub Actions, Jenkins, etc.)          | none           |
| `chore`    | Miscellaneous changes not affecting source or tests                  | none           |
| `revert`   | Revert a previous commit                                            | varies         |

> **`fix` vs `refactor`**: If the change resolves a known issue or abnormal behavior, use `fix`;
> if it only improves code structure without changing behavior, use `refactor`.
>
> **`feat` vs `perf`**: If it adds user-perceivable capability, use `feat`;
> if functionality is unchanged but runs faster, use `perf`.
>
> **`style` vs `refactor`**: `style` is purely formatting (whitespace, indentation) where the AST
> remains unchanged; `refactor` involves structural logic changes (extracting functions, renaming).

### Scope

Scope identifies the module or subsystem affected, enabling quick filtering in git log.
Use lowercase English words, keep them short. Common scopes in this project:

| Scope      | Usage                                  |
| ---------- | -------------------------------------- |
| `gpio`     | GPIO peripheral driver                 |
| `i2c`      | I2C peripheral driver                  |
| `spi`      | SPI peripheral driver                  |
| `uart`     | UART peripheral driver                 |
| `adc`      | ADC peripheral driver                  |
| `pwm`      | PWM peripheral driver                  |
| `base`     | Base package / framework / system init |
| `lvgl`     | LVGL GUI framework                     |
| `examples` | Example code                           |
| `docs`     | Documentation                          |
| `build`    | Build system (Makefile, linker, etc.)  |
| `config`   | Configuration (pin config, params)     |
| `tools`    | Tools and scripts                      |

> If a commit spans multiple modules, omit scope or pick the most affected one.
> When scope is too broad, omitting it is better than using an inaccurate one.

### Subject Rules

1. **Length**: Max 72 characters (entire Header line)
2. **Language**: Chinese or English, be consistent within the project
3. **No period**: Do not end with a period
4. **Imperative mood**: Start with base verb form (`add`, `fix`, `remove`), not past tense
5. **Describe what**: Not why or how (save those for Body)

**Good examples**:

```
feat(spi): add DMA transfer mode support
fix(uart): fix baud rate divider overflow at high rates
refactor(config): extract pin mapping table into config array
```

**Bad examples**:

```
feat(spi): added DMA transfer mode support.    ← no period, no past tense
fix: fixed a bug                               ← too vague
update code                                    ← missing type, unclear
```

---

## Body Details (Optional)

Body supplements the Header's subject, explaining **why the change was made** and **what changed**.

### Writing Rules

1. Must be separated from Header by a **blank line**
2. Max 72 characters per line, wrap manually if needed
3. Use **numbered lists** (1, 2, 3, ...) for change points
4. Focus on **motivation (Why)** and **before/after comparison (What changed)**
5. Include at least 2 items

### When to Write Body

- The reason isn't obvious from the code diff
- Background context is needed
- Performance data changed (include before/after metrics)
- Alternative approaches were considered (explain why this one was chosen)

### When to Skip Body

- Change is trivial and subject is self-explanatory
- Pure formatting or documentation changes

---

## Footer Details (Optional)

Footer records two types of information: **Breaking Changes** and **Related Issues**.

### Breaking Changes

If the commit introduces backward-incompatible changes (API signature changes, config format changes, feature removal), it **must** be declared in the Footer:

```
BREAKING CHANGE: <detailed description>
```

Also recommended to add `!` after type in the Header:

```
feat(base)!: <subject>
```

> `BREAKING CHANGE` must be uppercase, followed by a colon and space.

### Related Issues

```
Closes #123
Closes #123, #456, #789
```

> Other supported keywords: `Fixes`, `Resolves`.

---

## Complete Examples

### feat — New Feature

```
feat(spi): add DMA transfer mode support

1. Add DMA transfer channel configuration in SPI driver
2. Support high-speed bulk data transfer, reducing CPU usage
3. Original polling mode remains unchanged, DMA mode enabled via config

Closes #45
```

### fix — Bug Fix

```
fix(uart): fix baud rate divider overflow above 115200

1. Original divider stored in uint16_t
2. Multiplication overflow at baud rates above 115200, causing inaccurate rates
3. Changed to uint32_t for intermediate calculation, accurate up to 921600
```

### refactor — Restructuring

```
refactor(config): extract hardcoded pin mapping into config array

1. Pin mappings were scattered across multiple switch-case branches
2. Now unified into pin_map_table array
3. Adding new models only requires one new array entry
```

### perf — Performance

```
perf(lvgl): optimize partial screen refresh to reduce SPI transfers

1. Previous implementation transferred full screen (240x320) every refresh
2. Now only transfers dirty region pixels
3. Typical UI scenario: frame rate improved from 12fps to 24fps, SPI bus usage reduced ~60%
```

### BREAKING CHANGE

```
feat(base)!: change system init to phased callback mechanism

1. Original single-entry AppMain() split into three phase callbacks
2. AppEarlyInit(): peripheral clock and pin init (interrupts disabled)
3. AppInit(): driver and middleware init (interrupts enabled)
4. AppReady(): application logic start (all subsystems ready)

BREAKING CHANGE: AppMain() removed. All application code must migrate to
the new three-phase callback interface. See docs/migration-v2.md.
```

---

## Special Cases

### Single commit spans multiple types

Principle: **one commit does one thing**. If unavoidable, use the **primary change** type.

### Fixing commit messages

Use `git commit --amend` for the latest commit. Use `git rebase -i` for earlier commits (unpushed only).

> **Warning**: Never amend or rebase commits already pushed to remote.

### Merge Commits

Merge commits may retain Git's auto-generated message without following this convention.

---

## Best Practices

1. **Atomic commits**: One commit, one thing
2. **Write Header before coding**: Helps maintain focus
3. **Concise Header, detailed Body**: Put explanations in Body
4. **Always declare breaking changes**: Use BREAKING CHANGE in Footer
5. **Commit often**: Small incremental commits are easier to trace
6. **Use scope**: Makes `git log --oneline` output clear at a glance
