export function createCanvasScreen(canvas) {
  const context = canvas.getContext("2d");
  context.imageSmoothingEnabled = false;

  return {
    render(imageData) {
      context.putImageData(imageData, 0, 0);
    }
  };
}

export function createPocketPogoRunner({
  GameboyCtor,
  romUrl,
  saveStore,
  screen,
  controlsRoot,
  status,
  bindControls,
  fetchRom = globalThis.fetch,
  now = Date.now
}) {
  let gameboy = null;
  let unbindControls = null;

  function setStatus(message) {
    status.textContent = message;
  }

  async function loadRom() {
    const response = await fetchRom(`${romUrl}?v=${now()}`);
    if (!response.ok) throw new Error(`ROM fetch failed: ${response.status}`);
    return response.arrayBuffer();
  }

  return {
    get instance() {
      return gameboy;
    },

    async boot() {
      setStatus("Loading ROM...");
      const rom = await loadRom();

      gameboy = new GameboyCtor();
      gameboy.loadGame(rom);
      saveStore.restore(gameboy);

      gameboy.onFrameFinished((imageData) => screen.render(imageData));
      saveStore.bind(gameboy);

      if (unbindControls) unbindControls();
      unbindControls = bindControls({ root: controlsRoot, input: gameboy.input });

      gameboy.run();
      setStatus("Running");
      return gameboy;
    }
  };
}

