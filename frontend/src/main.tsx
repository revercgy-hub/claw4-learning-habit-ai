import React from 'react';
import ReactDOM from 'react-dom/client';
import { App } from './App';
import './style.css';

// PWA service worker registration: production only, caches the static shell
// (never API/auth data — see public/sw.js).
if (import.meta.env.PROD && 'serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('/sw.js').catch(() => {
      // offline shell is a progressive enhancement; ignore failures
    });
  });
}

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
