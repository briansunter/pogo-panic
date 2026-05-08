# Pocket Pogo Panic

A tiny Game Boy arcade platformer built with GBDK-2020. The player is always bouncing on a pogo stick; steer left/right, stomp with A to break cracked blocks and rocks, use B to tilt/short-hop, and grab the golden battery before reaching the exit.

## Build

```sh
npm install
npm run setup:gbdk
npm run build:rom
```

The ROM is written to `dist/pocket-pogo-panic.gb` and copied to `public/roms/pocket-pogo-panic.gb` for the browser runner.

The production browser build is written to `site-dist/`, separate from the ROM artifact directory.

## Play In Browser

```sh
npm run dev
```

Open `http://127.0.0.1:5173/`.

Controls:

- Arrow Left / Arrow Right: steer
- A: stomp / break cracked blocks and rocks
- B: tilt / short hop
- Enter: start / pause

## Test

```sh
npm test
npm run build
```
