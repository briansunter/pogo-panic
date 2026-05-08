# Repository Guidelines

## Project Structure & Module Organization

Pocket Pogo Panic is split between a Game Boy ROM and a browser runner.

- `src-rom/main.c`: GBDK-2020 ROM source, including gameplay, level generation, save handling, menus, music, and rendering.
- `src-web/`: Vite browser shell, emulator boot code, controls, SRAM persistence, styling, and the level debug viewer.
- `test/`: Node test files. `rom-smoke.test.mjs` checks ROM artifacts and headers; `web-modules.test.mjs` checks web modules and generated level invariants.
- `public/roms/`: browser-served ROM copy created by `make`.
- `dist/`: built ROM artifact.
- `site-dist/`: production web build output.
- `.tools/gbdk/`: local GBDK install created by setup script.

## Build, Test, and Development Commands

- `npm install`: install Vite and emulator dependencies.
- `npm run setup:gbdk`: download/install GBDK into `.tools/gbdk`.
- `npm run build:rom` or `make`: compile `src-rom/main.c` to `dist/pocket-pogo-panic.gb` and copy it to `public/roms/`.
- `npm run build:web`: build the Vite app into `site-dist/`.
- `npm run build`: build both ROM and web app.
- `npm test`: rebuild the ROM, then run all Node tests.
- `npm run dev`: start the local browser runner at `http://127.0.0.1:5173/` or the next free port.

## Coding Style & Naming Conventions

Use existing style. C code uses 4-space indentation, `snake_case` functions/variables, uppercase macros, and compact static helpers. JavaScript modules use ES modules, 2-space indentation, named exports, and `camelCase`. Keep generated level logic in `src-rom/main.c` and `src-web/level-debug.js` aligned when changing room layouts or tile semantics. There is no configured formatter or linter; run `git diff --check` before committing.

## Testing Guidelines

Tests use Node’s built-in `node:test` and `node:assert/strict`. Name test files `*.test.mjs`. Add or update tests when changing save behavior, emulator boot order, input mapping, ROM artifact expectations, or generated level invariants. Run `npm test` before submitting changes.

## Commit & Pull Request Guidelines

The current history uses short, informal messages. Prefer concise imperative commits such as `add stomp rock routes` or `fix panic room exit safety`. PRs should describe gameplay/user-facing changes, mention test results, and include screenshots when changing the browser UI or level debug view. If ROM gameplay changes, include the rebuilt `dist/` and `public/roms/` artifacts when appropriate.

## Agent-Specific Instructions

Do not overwrite unrelated local changes. Treat generated ROM artifacts as intentional when produced by `npm run build:rom`. Avoid editing `.tools/`, `node_modules/`, or `site-dist/` unless the task explicitly requires it.

## Agent skills

### Issue tracker

Issues live as local markdown files under `.scratch/<feature>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default canonical strings (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
