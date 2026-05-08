function bytesToBinary(bytes) {
  let binary = "";
  for (let i = 0; i < bytes.length; i += 1) binary += String.fromCharCode(bytes[i]);
  return binary;
}

function binaryToBytes(binary) {
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

export function bufferToBase64(buffer) {
  const bytes = new Uint8Array(buffer);
  if (typeof globalThis.btoa === "function") return globalThis.btoa(bytesToBinary(bytes));
  return globalThis.Buffer.from(bytes).toString("base64");
}

export function base64ToBuffer(value) {
  if (typeof globalThis.atob === "function") return binaryToBytes(globalThis.atob(value)).buffer;
  return Uint8Array.from(globalThis.Buffer.from(value, "base64")).buffer;
}

export function createSaveStore({ storage, key, logger = console }) {
  return {
    restore(instance) {
      const saved = storage.getItem(key);
      if (!saved) return false;
      try {
        instance.setCartridgeSaveRam(base64ToBuffer(saved));
        return true;
      } catch (error) {
        logger.warn("Could not load saved SRAM", error);
        storage.removeItem(key);
        return false;
      }
    },

    persist(instance) {
      const sram = instance.getCartridgeSaveRam();
      if (!sram) return false;
      storage.setItem(key, bufferToBase64(sram));
      return true;
    },

    bind(instance) {
      instance.setOnWriteToCartridgeRam(() => {
        this.persist(instance);
      });
    }
  };
}

