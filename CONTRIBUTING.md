# Contributing to NoteWrapper

Thank you for your interest in contributing to NoteWrapper.

This project aims to remain minimal, terminal-focused, and easy to extend. Contributions should respect this design philosophy.

---

## Debugging and error handling

Please use the standardized debugging and error-handling functions:

* `debug(...)`
  Used for formatted debugging output (printf-style formatting).

* `altDebug(...)`
  A lighter version of `debug()` when full formatting is not required.

* `error(condition, "user" | "program", "error message", ...)`
  Standard error reporting function:

  * `"user"`: errors caused by user input or configuration
  * `"program"`: internal or unexpected program errors

---

## Commit naming convention

This is mostly for me, as my commit messages can sometimes be cryptic and hard to follow later.

Format:

```
<module> (#issue): description
```

Examples:

* `Editor (#1): add support for Microsoft Word`
* `README.md: fix typos`
* `Journal: improve entry parsing logic`

Keep commit messages short and descriptive.

---

## Adding editor support

To add support for a new editor, you must update both documentation and source code:

* Update `README.md`:

  * Add the editor to the **Editor support** table
  * Update the **Dependencies** section if necessary

* In `src/utils.c`:

  * Add the editor name to `supportedEditor[]`
  * Increment `numEditor`

* If the editor executable name differs from its logical name (for example `neovim` → `nvim`), update `doesEditorExist()` in `src/utils.c`

* Modify `openEditor()` in `src/utils.c`:

  * If the editor supports a Vivify plugin integration, follow the pattern used for **(neo)vim**
  * If it does not support integration, NoteWrapper must launch Vivify separately (see implementation used for **nano**)

---

## Before opening a pull request

Before submitting a pull request, ensure that:

* The project compiles successfully using:

  * `make`, or
  * `nix-build` (on NixOS)
* There are **no warnings or errors**
