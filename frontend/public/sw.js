// Claw4 parent PWA service worker.
//
// Caches ONLY the static app shell (precache). It never caches /api/*
// responses, auth data, child JSON or secrets — requests to the family
// backend are network-only. The host MVP targets 127.0.0.1 only.
const STATIC_CACHE = 'claw4-static-v1';
const PRECACHE = ['/', '/index.html', '/manifest.webmanifest', '/icon.svg'];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(STATIC_CACHE).then((cache) => cache.addAll(PRECACHE)).then(() => self.skipWaiting()),
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches
      .keys()
      .then((keys) =>
        Promise.all(keys.filter((k) => k !== STATIC_CACHE).map((k) => caches.delete(k))),
      )
      .then(() => self.clients.claim()),
  );
});

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  // Never serve or store API/backend traffic from the cache.
  if (url.pathname.startsWith('/api/')) {
    return;
  }
  // App shell: cache-first with network fallback for navigation/static.
  if (event.request.mode === 'navigate' || PRECACHE.includes(url.pathname)) {
    event.respondWith(
      caches.match(event.request).then(
        (cached) =>
          cached ||
          fetch(event.request).then((response) => {
            const copy = response.clone();
            caches.open(STATIC_CACHE).then((cache) => cache.put(event.request, copy));
            return response;
          }),
      ),
    );
  }
});
