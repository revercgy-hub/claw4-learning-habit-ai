import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

// Host-MVP: the dev server and preview bind to 127.0.0.1 only; the API base
// URL comes from the environment (VITE_API_BASE_URL), never hard-coded.
export default defineConfig({
  plugins: [react()],
  server: {
    host: '127.0.0.1',
    port: 5173,
    proxy: {
      // Local dev convenience: /api -> backend on 127.0.0.1:8000. The built
      // app talks to VITE_API_BASE_URL directly.
      '/api': {
        target: process.env.VITE_API_BASE_URL ?? 'http://127.0.0.1:8000',
        changeOrigin: false,
      },
    },
  },
  preview: {
    host: '127.0.0.1',
    port: 4173,
  },
  build: {
    // Never let vite wipe the output directory: destructive bulk deletes are
    // blocked by the host security policy and would break repeated builds.
    // (Hashed assets are overwritten; stale hashed files are harmless for the
    // host-MVP evidence runs.)
    emptyOutDir: false,
  },
  test: {
    environment: 'jsdom',
    globals: true,
    setupFiles: ['./src/test/setup.ts'],
    css: false,
  },
} as any);
