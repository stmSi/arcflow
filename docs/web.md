# Web build

Requires [Emscripten 6.0.12](https://emscripten.org/docs/getting_started/downloads.html), CMake, and Ninja.
Activate `emsdk_env.sh`, then:

```sh
git submodule update --init --recursive
emcmake cmake --preset web
cmake --build --preset web
python3 -m http.server 8080 --directory build/web/site
```

Open `http://localhost:8080` for the editor or `/player.html` for playback.
Deploy the contents of `build/web/site/` to any static host. No server or special headers needed.

GitHub Pages: select **Settings → Pages → GitHub Actions**, then run the
**WebAssembly / GitHub Pages** workflow on `master`.

Projects and preferences persist in browser storage. Save/Export download files;
Open imports local files. The player accepts `.afp` uploads.
Opening and exporting show progress; downloads also provide a clickable link.
Storage is scoped per site; keep one editor tab open at a time.
GIF/MP4 export and native plugins require the desktop app. Rendering uses browser Canvas
and CPU billboard particles; GPU-specific VFX stay on desktop.

Browser tests: `npm ci --prefix web`, `npx --prefix web playwright install chromium`,
then `npm test --prefix web`.
