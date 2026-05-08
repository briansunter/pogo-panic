import { CONTROL_BINDINGS, KEYBOARD_BINDINGS } from "./constants.js";

function setInputState(input, bindings, control, pressed) {
  const prop = bindings[control];
  if (!prop) return false;
  input[prop] = pressed;
  return true;
}

function safelyCapturePointer(element, event) {
  if (event.pointerId == null || typeof element.setPointerCapture !== "function") return;
  try {
    element.setPointerCapture(event.pointerId);
  } catch {
    // Some synthetic/mobile test events are not eligible for pointer capture.
  }
}

function safelyReleasePointer(element, event) {
  if (event.pointerId == null || typeof element.releasePointerCapture !== "function") return;
  if (typeof element.hasPointerCapture === "function" && !element.hasPointerCapture(event.pointerId)) return;
  try {
    element.releasePointerCapture(event.pointerId);
  } catch {
    // Matching pointerup can arrive after capture was already cancelled.
  }
}

export function bindVirtualButtons({ root = document, input, bindings = CONTROL_BINDINGS, onInputStart = () => {} }) {
  const unbind = [];

  root.querySelectorAll("[data-key]").forEach((button) => {
    if (typeof button.closest === "function" && button.closest("[data-dpad]")) return;

    const control = button.dataset.key;
    if (!bindings[control]) return;

    const press = (event) => {
      event.preventDefault();
      safelyCapturePointer(button, event);
      setInputState(input, bindings, control, true);
      onInputStart();
      button.classList.add("active");
    };
    const release = (event) => {
      event.preventDefault();
      safelyReleasePointer(button, event);
      setInputState(input, bindings, control, false);
      button.classList.remove("active");
    };

    button.addEventListener("pointerdown", press);
    button.addEventListener("pointerup", release);
    button.addEventListener("pointercancel", release);
    button.addEventListener("pointerleave", release);

    unbind.push(() => {
      button.removeEventListener("pointerdown", press);
      button.removeEventListener("pointerup", release);
      button.removeEventListener("pointercancel", release);
      button.removeEventListener("pointerleave", release);
    });
  });

  return () => {
    unbind.forEach((release) => release());
  };
}

export function bindDirectionalPads({ root = document, input, bindings = CONTROL_BINDINGS, onInputStart = () => {} }) {
  const unbind = [];
  const directions = ["up", "down", "left", "right"];

  root.querySelectorAll("[data-dpad]").forEach((pad) => {
    let activeControl = null;
    let activePointerId = null;

    const buttons = new Map(directions.map((control) => [control, pad.querySelector(`[data-key="${control}"]`)]));
    const setActiveControl = (control) => {
      directions.forEach((direction) => {
        setInputState(input, bindings, direction, direction === control);
        buttons.get(direction)?.classList.toggle("active", direction === control);
      });
      activeControl = control;
    };
    const clearActiveControl = () => {
      directions.forEach((direction) => {
        setInputState(input, bindings, direction, false);
        buttons.get(direction)?.classList.remove("active");
      });
      activeControl = null;
    };
    const controlFromPointer = (event) => {
      const rect = pad.getBoundingClientRect();
      const x = event.clientX - rect.left - rect.width / 2;
      const y = event.clientY - rect.top - rect.height / 2;
      if (Math.abs(x) > Math.abs(y)) return x < 0 ? "left" : "right";
      if (Math.abs(y) > 0) return y < 0 ? "up" : "down";
      return activeControl;
    };
    const isActivePointer = (event) => activePointerId != null && (event.pointerId == null || event.pointerId === activePointerId);
    const press = (event) => {
      const control = controlFromPointer(event);
      if (!control) return;
      activePointerId = event.pointerId ?? 0;
      event.preventDefault();
      safelyCapturePointer(pad, event);
      setActiveControl(control);
      onInputStart();
    };
    const move = (event) => {
      if (!isActivePointer(event)) return;
      const control = controlFromPointer(event);
      if (!control) return;
      event.preventDefault();
      setActiveControl(control);
    };
    const release = (event) => {
      if (!isActivePointer(event)) return;
      event.preventDefault();
      safelyReleasePointer(pad, event);
      activePointerId = null;
      clearActiveControl();
    };

    pad.addEventListener("pointerdown", press);
    pad.addEventListener("pointermove", move);
    pad.addEventListener("pointerup", release);
    pad.addEventListener("pointercancel", release);
    pad.addEventListener("pointerleave", release);

    unbind.push(() => {
      activePointerId = null;
      clearActiveControl();
      pad.removeEventListener("pointerdown", press);
      pad.removeEventListener("pointermove", move);
      pad.removeEventListener("pointerup", release);
      pad.removeEventListener("pointercancel", release);
      pad.removeEventListener("pointerleave", release);
    });
  });

  return () => {
    unbind.forEach((release) => release());
  };
}

export function bindKeyboardControls({
  root = document,
  input,
  bindings = CONTROL_BINDINGS,
  keyboardBindings = KEYBOARD_BINDINGS,
  onInputStart = () => {}
}) {
  const activeCodes = new Set();

  const press = (event) => {
    const keyId = event.code || event.key;
    const control = keyboardBindings[keyId];
    if (!control) return;
    event.preventDefault();
    activeCodes.add(keyId);
    setInputState(input, bindings, control, true);
    onInputStart();
  };

  const release = (event) => {
    const keyId = event.code || event.key;
    const control = keyboardBindings[keyId];
    if (!control) return;
    event.preventDefault();
    activeCodes.delete(keyId);
    setInputState(input, bindings, control, false);
  };

  root.addEventListener("keydown", press);
  root.addEventListener("keyup", release);

  return () => {
    activeCodes.forEach((code) => {
      const control = keyboardBindings[code];
      setInputState(input, bindings, control, false);
    });
    activeCodes.clear();
    root.removeEventListener("keydown", press);
    root.removeEventListener("keyup", release);
  };
}

export function bindInputControls(options) {
  const unbindDpads = bindDirectionalPads(options);
  const unbindVirtual = bindVirtualButtons(options);
  const unbindKeyboard = bindKeyboardControls(options);

  return () => {
    unbindDpads();
    unbindVirtual();
    unbindKeyboard();
  };
}
