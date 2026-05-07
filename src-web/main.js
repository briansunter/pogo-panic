import { Gameboy } from "gameboy-emulator";
import "./style.css";

const ROM_URL = "/roms/pocket-pogo-panic.gb";
const SAVE_KEY = "pocket-pogo-panic.sram.v1";

const canvas = document.querySelector("#screen");
const statusEl = document.querySelector("#status");
const reloadButton = document.querySelector("#reload");
const context = canvas.getContext("2d");
context.imageSmoothingEnabled = false;

let gameboy = null;

function setStatus(message) {
  statusEl.textContent = message;
}

function bufferToBase64(buffer) {
  const bytes = new Uint8Array(buffer);
  let binary = "";
  for (let i = 0; i < bytes.length; i += 1) binary += String.fromCharCode(bytes[i]);
  return btoa(binary);
}

function base64ToBuffer(value) {
  const binary = atob(value);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) bytes[i] = binary.charCodeAt(i);
  return bytes.buffer;
}

function loadSavedSram(instance) {
  const saved = localStorage.getItem(SAVE_KEY);
  if (!saved) return;
  try {
    instance.setCartridgeSaveRam(base64ToBuffer(saved));
  } catch (error) {
    console.warn("Could not load saved SRAM", error);
    localStorage.removeItem(SAVE_KEY);
  }
}

function bindVirtualButtons(instance) {
  const mapping = {
    left: "isPressingLeft",
    right: "isPressingRight",
    a: "isPressingA",
    b: "isPressingB",
    start: "isPressingStart"
  };

  document.querySelectorAll("[data-key]").forEach((button) => {
    const prop = mapping[button.dataset.key];
    if (!prop) return;

    const press = (event) => {
      event.preventDefault();
      instance.input[prop] = true;
      button.classList.add("active");
    };
    const release = (event) => {
      event.preventDefault();
      instance.input[prop] = false;
      button.classList.remove("active");
    };

    button.addEventListener("pointerdown", press);
    button.addEventListener("pointerup", release);
    button.addEventListener("pointercancel", release);
    button.addEventListener("pointerleave", release);
  });
}

async function boot() {
  setStatus("Loading ROM...");
  const response = await fetch(`${ROM_URL}?v=${Date.now()}`);
  if (!response.ok) throw new Error(`ROM fetch failed: ${response.status}`);

  const rom = await response.arrayBuffer();
  gameboy = new Gameboy();
  gameboy.loadGame(rom);
  loadSavedSram(gameboy);

  gameboy.onFrameFinished((imageData) => {
    context.putImageData(imageData, 0, 0);
  });

  gameboy.setOnWriteToCartridgeRam(() => {
    localStorage.setItem(SAVE_KEY, bufferToBase64(gameboy.getCartridgeSaveRam()));
  });

  bindVirtualButtons(gameboy);
  gameboy.run();
  setStatus("Running");
}

reloadButton.addEventListener("click", () => window.location.reload());

boot().catch((error) => {
  console.error(error);
  setStatus(error.message);
});
