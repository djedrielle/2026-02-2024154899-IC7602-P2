import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    /* En desarrollo la UI corre en 5173 y las funciones del API en 3000 (vercel dev).
     * Este proxy hace que /api/... se pida al propio servidor de Vite, que lo reenvia
     * al 3000. El navegador ve un solo origen, asi que no hay CORS, y el codigo del
     * frontend usa las mismas rutas relativas que usara en produccion. */
    proxy: {
      '/api': {
        target: 'http://localhost:3000',
        changeOrigin: true,
      },
    },
  },
})
