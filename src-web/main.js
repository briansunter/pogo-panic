import { Gameboy } from "gameboy-emulator";
import { AUDIO_MUTED_KEY, ROM_URL, SAVE_KEY } from "./constants.js";
import { createAudioPreference, createCanvasScreen, createPocketPogoRunner } from "./emulator-runner.js";
import { bindInputControls } from "./input.js";
import { renderLevelDebug } from "./level-debug.js";
import { createSaveStore } from "./save-store.js";
import "./style.css";

const shell = document.querySelector(".shell");
const canvas = document.querySelector("#screen");
const status = document.querySelector("#status");
const soundButton = document.querySelector("#sound");
const reloadButton = document.querySelector("#reload");
const debugButton = document.querySelector("#debug-levels");

function openDebugLevels() {
  const url = new URL(window.location.href);
  url.searchParams.set("debug", "levels");
  window.location.href = url.toString();
}

if (new URLSearchParams(window.location.search).get("debug") === "levels") {
  renderLevelDebug({
    root: shell,
    onBack() {
      window.location.href = window.location.pathname;
    }
  });
} else {
  const runner = createPocketPogoRunner({
    GameboyCtor: Gameboy,
    romUrl: ROM_URL,
    saveStore: createSaveStore({ storage: localStorage, key: SAVE_KEY }),
    screen: createCanvasScreen(canvas),
    controlsRoot: document,
    status,
    soundToggle: soundButton,
    audioPreference: createAudioPreference({ storage: localStorage, key: AUDIO_MUTED_KEY }),
    bindControls: bindInputControls
  });

  reloadButton.addEventListener("click", () => window.location.reload());
  debugButton.addEventListener("click", openDebugLevels);

  runner.boot().catch((error) => {
    console.error(error);
    status.textContent = error.message;
  });
}
