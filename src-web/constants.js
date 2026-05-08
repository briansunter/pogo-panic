const baseUrl = import.meta.env?.BASE_URL ?? "/";

export const ROM_URL = `${baseUrl}roms/pocket-pogo-panic.gb`;
export const SAVE_KEY = "pocket-pogo-panic.sram.v1";

export const CONTROL_BINDINGS = {
  up: "isPressingUp",
  down: "isPressingDown",
  left: "isPressingLeft",
  right: "isPressingRight",
  a: "isPressingA",
  b: "isPressingB",
  select: "isPressingSelect",
  start: "isPressingStart"
};

export const KEYBOARD_BINDINGS = {
  ArrowUp: "up",
  ArrowDown: "down",
  ArrowLeft: "left",
  ArrowRight: "right",
  KeyZ: "b",
  z: "b",
  Z: "b",
  KeyX: "a",
  x: "a",
  X: "a",
  Space: "select",
  Enter: "start",
  NumpadEnter: "start"
};
