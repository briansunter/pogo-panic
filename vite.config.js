import { defineConfig } from "vite";

export default defineConfig({
  build: {
    outDir: "site-dist"
  },
  server: {
    headers: {
      "Cross-Origin-Opener-Policy": "same-origin",
      "Cross-Origin-Embedder-Policy": "require-corp"
    }
  }
});
