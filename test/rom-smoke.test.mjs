import assert from "node:assert/strict";
import fs from "node:fs";
import { test } from "node:test";

const romPath = new URL("../dist/pocket-pogo-panic.gb", import.meta.url);
const webRomPath = new URL("../public/roms/pocket-pogo-panic.gb", import.meta.url);
const sourcePath = new URL("../src-rom/main.c", import.meta.url);
const viteConfigPath = new URL("../vite.config.js", import.meta.url);

test("ROM is built with the expected Game Boy header", () => {
  assert.ok(fs.existsSync(romPath), "dist/pocket-pogo-panic.gb should exist");
  const rom = fs.readFileSync(romPath);
  assert.ok(rom.length >= 0x8000, "ROM should be at least one 32K cartridge image");

  const title = rom.subarray(0x0134, 0x0144).toString("ascii").replace(/\0+$/g, "");
  assert.match(title, /POGO_PANIC/);
  assert.equal(rom[0x0147], 0x03, "cartridge should be MBC1+SRAM+BATTERY");
  assert.notEqual(rom[0x0149], 0x00, "cartridge should expose SRAM for saves");
});

test("browser ROM copy matches the built ROM", () => {
  const rom = fs.readFileSync(romPath);
  const webRom = fs.readFileSync(webRomPath);
  assert.deepEqual(webRom, rom);
});

test("source advertises the complete adventure and mode set", () => {
  const source = fs.readFileSync(sourcePath, "utf8");
  assert.match(source, /#define NUM_ADVENTURE_LEVELS 80/);
  assert.match(source, /MODE_ADVENTURE/);
  assert.match(source, /MODE_PANIC/);
  assert.match(source, /MODE_TRIAL/);
  assert.match(source, /T_SPRING/);
  assert.match(source, /T_CRACK/);
  assert.match(source, /T_SWITCH/);
  assert.match(source, /T_FAN_R/);
  assert.match(source, /T_CONV_R/);
  assert.match(source, /T_BUBBLE/);
});

test("web build output does not overwrite ROM artifacts", () => {
  const viteConfig = fs.readFileSync(viteConfigPath, "utf8");
  assert.match(viteConfig, /outDir:\s*"site-dist"/);
});
