export function createCanvasScreen(canvas) {
  const context = canvas.getContext("2d");
  const pocketFrame = context.createImageData(canvas.width, canvas.height);
  const pocketPalette = [
    [248, 248, 240],
    [184, 184, 176],
    [104, 104, 96],
    [24, 24, 22]
  ];
  context.imageSmoothingEnabled = false;

  return {
    render(imageData) {
      const source = imageData.data;
      const target = pocketFrame.data;
      for (let i = 0; i < source.length; i += 4) {
        const luma = (source[i] * 77 + source[i + 1] * 150 + source[i + 2] * 29) >> 8;
        const shade = luma > 218 ? 0 : luma > 146 ? 1 : luma > 72 ? 2 : 3;
        const color = pocketPalette[shade];
        target[i] = color[0];
        target[i + 1] = color[1];
        target[i + 2] = color[2];
        target[i + 3] = source[i + 3];
      }
      context.putImageData(pocketFrame, 0, 0);
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
  let soundEnabled = false;

  function setStatus(message) {
    status.textContent = message;
  }

  function enableSound() {
    if (soundEnabled) return;
    if (typeof gameboy?.apu?.enableSound !== "function") return;
    gameboy.apu.enableSound();
    soundEnabled = true;
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
      unbindControls = bindControls({ root: controlsRoot, input: gameboy.input, onInputStart: enableSound });

      gameboy.run();
      setStatus("Running");
      return gameboy;
    }
  };
}
