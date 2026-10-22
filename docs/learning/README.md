# Tutorials

[Learn online](https://stmsi.github.io/arcflow/learn/) ·
[Download PDF](https://stmsi.github.io/arcflow/learn/Arcflow-Tutorials.pdf)

In the editor: **Help → Tutorials / PDF → Practice in editor**.
Walkthroughs keep your current project open. The website adds searchable lessons,
local progress, and interactive frame, easing, and gradient examples.

## Update

Edit **catalog.json**. Its titles, steps, and details generate the editor guides,
website, and printable book. Keep existing `id` and `slug` values stable; append new
guides. Each guide has 1–8 steps (text: 200 characters, detail: 250). Optional `demo`: `frames`, `curves`, or `gradient`.
Only presentation code belongs in `learn.js` / `learn.css`.

```sh
python3 tools/build-learning.py                 # validate
cmake --build --preset debug                   # editor + build/debug/learn
python3 tests/test_learning.py
```

Open `build/debug/learn/index.html` offline. Open `print.html` and use Print → Save
as PDF, or generate the downloadable PDF (Node and Chromium required):

```sh
npm ci --prefix web
npx --prefix web playwright install chromium
npm run docs:pdf --prefix web -- ../build/debug/learn
```

The PDF command defaults to `build/web/site/learn`. Pages CI regenerates and tests
the PDF before publishing. Tagged releases include it as a separate download.
Native packages include the offline HTML guide. Generated files stay in `build/`.

When behavior or shortcuts change, update the matching lesson in the same commit.
Lesson hashes invalidate stale progress automatically; the book shows its editor
revision and catalog hash. CI validates the schema and checks generated content,
navigation, progress, interactive examples, and PDF delivery.
