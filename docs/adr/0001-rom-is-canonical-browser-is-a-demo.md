# ROM is the canonical artifact; the browser runner is a demo

Pogo Panic is a real Game Boy game. The ROM in `src-rom/` (built with GBDK-2020 to `dist/pogo-panic.gb`) is the canonical artifact and must run unmodified on DMG hardware, flashcarts, and standalone emulators. The browser runner in `src-web/` exists so people can try the game without downloading a ROM or installing an emulator — it loads the same `.gb` file through `gameboy-emulator` and is treated as a demo, not a peer build.

This framing decides several downstream questions:

- **All gameplay rules live in C.** The JS side is strictly the runner (emulator host, input mapping, SRAM persistence) plus dev tooling (level debug viewer, reachability tests). It never re-implements gameplay.
- **Mirrored data flows one way: C → JSON → JS.** Levels, tile metadata, and tile art are dumped from the ROM source by host-compiled C tools (see ADR-0005); JS consumes the JSON. JS does not author tile maps or level data.
- **No browser-only features.** A change that would only work in the web shell isn't acceptable — it has to work on real hardware too.

## Considered alternatives

- **ROM-only distribution.** Loses the zero-friction trial path. Rejected because most discovery happens through links, not flashcart purchases.
- **Web-only with no ROM target.** Loses the constraint discipline that gives the game its identity (8-bit palette, 160×144 screen, 4-channel sound, real bounce physics on a fixed tick). Without "must run on DMG", scope creep is inevitable.
- **Author gameplay in JS and transpile to Game Boy.** Undermines the "real Game Boy game" positioning and has no mature toolchain for the kind of code we write.
