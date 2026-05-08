import { Gameboy } from "gameboy-emulator";
import { ROM_URL, SAVE_KEY } from "./constants.js";
import { createCanvasScreen, createPocketPogoRunner } from "./emulator-runner.js";
import { bindInputControls } from "./input.js";
import { createSaveStore } from "./save-store.js";
import "./style.css";

const canvas = document.querySelector("#screen");
const status = document.querySelector("#status");
const reloadButton = document.querySelector("#reload");

const runner = createPocketPogoRunner({
  GameboyCtor: Gameboy,
  romUrl: ROM_URL,
  saveStore: createSaveStore({ storage: localStorage, key: SAVE_KEY }),
  screen: createCanvasScreen(canvas),
  controlsRoot: document,
  status,
  bindControls: bindInputControls
});

reloadButton.addEventListener("click", () => window.location.reload());

runner.boot().catch((error) => {
  console.error(error);
  status.textContent = error.message;
});
