import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile, stat} from 'node:fs/promises';
import {resolve, extname, sep} from 'node:path';
import {chromium} from '../web/node_modules/playwright/index.mjs';

const root = resolve(process.argv[2] || 'build/web/site');
const prefix = '/arcflow/';
const types = {'.html': 'text/html', '.js': 'application/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream', '.svg': 'image/svg+xml'};
const server = createServer(async (request, response) => {
  const path = new URL(request.url, 'http://localhost').pathname;
  if (path === '/favicon.ico') { response.writeHead(204).end(); return; }
  if (!path.startsWith(prefix)) { response.writeHead(404).end(); return; }
  const file = resolve(root, decodeURIComponent(path.slice(prefix.length)) || 'index.html');
  if (!file.startsWith(root + sep)) { response.writeHead(403).end(); return; }
  try {
    response.writeHead(200, {'Content-Type': types[extname(file)] || 'application/octet-stream'});
    response.end(await readFile(file));
  } catch { response.writeHead(404).end(); }
});
await new Promise(r => server.listen(0, '127.0.0.1', r));
const base = `http://127.0.0.1:${server.address().port}${prefix}`;
const options = {headless: true};
if (process.env.ARCFLOW_CHROMIUM) options.executablePath = process.env.ARCFLOW_CHROMIUM;
const browser = await chromium.launch(options);
try {
  for (const [smoke, frames] of [['studio', 4], ['template', 2], ['inspector', 8], ['vector-tools', 8], ['image', 4], ['gradient-drag', 4], ['editing', 4], ['particle-inputs', 4], ['particle-view', 24]]) {
    const page = await browser.newPage({viewport: {width: 1440, height: 900}});
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto(`${base}?smoke=${smoke}&frames=${frames}&capture=1`);
    await page.waitForFunction(() => Module.exitCode !== undefined, null, {timeout: 60000});
    assert.equal(await page.evaluate(() => Module.exitCode), 0, `${smoke}: application failed`);
    const capture = await page.evaluate(() => Array.from(Module.FS.readFile('/documents/capture.png').slice(0, 8)));
    assert.deepEqual(capture, [137, 80, 78, 71, 13, 10, 26, 10]);
    assert.deepEqual(errors, [], `${smoke}: browser errors`);
    console.log(`PASS editor ${smoke}`);
    await page.close();
  }
  const context = await browser.newContext({viewport: {width: 1440, height: 900}, acceptDownloads: true});
  const page = await context.newPage();
  const settleEditor = async () => page.waitForFunction(n => Module.afCanvasFrames > n + 2,
    await page.evaluate(() => Module.afCanvasFrames));
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('dialog', d => d.type() === 'prompt' ? d.accept('browser-project.afproj') : d.accept());
  await page.goto(base);
  await page.waitForFunction(() => Module.afCanvasFrames > 3);
  await page.keyboard.press('Escape');
  await page.keyboard.press('Control+n');
  await page.keyboard.press('r');
  await page.mouse.move(500, 250);
  await page.mouse.down();
  await page.mouse.move(650, 360, {steps: 8});
  await page.mouse.up();
  await settleEditor();
  await page.keyboard.press('Shift+g');
  await settleEditor();
  await page.mouse.move(520, 270);
  await page.mouse.down();
  await page.mouse.move(630, 330, {steps: 8});
  await page.mouse.up();
  await settleEditor();
  const download = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  const saved = await download;
  assert.equal(saved.suggestedFilename(), 'browser-project.afproj');
  const projectPath = '/documents/' + saved.suggestedFilename();
  let project = await page.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), projectPath);
  assert.equal((project.match(/^node$/gm) || []).length, 4, 'New document gains the drawn rectangle');
  assert.doesNotMatch(project, /^type group$/m, 'Starting objects are directly in Layers');
  const fillType = source => {
    const fields = source.split('\n').filter(line => line.startsWith('style ')).at(-1).split(/\s+/);
    return Number(fields[17 + Number(fields[13])]); // Fill follows the style header and dash list.
  };
  assert.equal(fillType(project), 1, 'Gradient tool applies a linear fill');
  // Undo and redo use actual browser key events and preserve the drawn gradient on disk.
  await page.keyboard.press('Control+z');
  let gradientSave = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  await gradientSave;
  const undone = await page.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), projectPath);
  assert.equal(fillType(undone), 0, 'Undo restores the solid fill');
  await page.keyboard.press('Control+y');
  gradientSave = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  await gradientSave;
  const redone = await page.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), projectPath);
  const geometry = source => source.replace(/^metadata .*\n|^change .*\n/gm, '');
  assert.equal(geometry(redone), geometry(project), 'Redo restores the same objects and paint');
  project = redone;
  console.log('PASS gradient tool drawing, undo, redo and project persistence');
  await page.evaluate(() => new Promise((done, fail) => Module.FS.syncfs(false, e => e ? fail(e) : done())));
  await page.reload();
  await page.waitForFunction(() => Module.afCanvasFrames > 3);
  assert.equal(await page.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), projectPath), project);
  assert.ok(await page.evaluate(path => Module.arguments.includes(path), projectPath));
  const resave = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  await resave;
  console.log('PASS drawing, project download and persistent storage');
  const chooser = page.waitForEvent('filechooser');
  await page.keyboard.press('Control+o');
  await (await chooser).setFiles({name: 'imported.afproj', mimeType: 'text/plain', buffer: Buffer.from(project)});
  await page.waitForFunction(() => Module.FS.analyzePath('/documents/session.json').exists &&
    JSON.parse(Module.FS.readFile('/documents/session.json', {encoding: 'utf8'})).project.endsWith('/imported.afproj'));
  const importedDownload = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  assert.equal((await importedDownload).suggestedFilename(), 'imported.afproj');
  console.log('PASS opening and saving a local project');
  await page.keyboard.press('Control+Shift+e');
  const exportDownload = page.waitForEvent('download');
  await page.keyboard.press('Enter');
  assert.match((await exportDownload).suggestedFilename(), /\.svg$/);
  const svg = await page.evaluate(() => Module.FS.readFile('/documents/browser-project.afproj.svg', {encoding: 'utf8'}));
  assert.match(svg, /<svg/);
  console.log('PASS SVG export and browser download');
  await page.setViewportSize({width: 1120, height: 680});
  await page.waitForFunction(() => Module.canvas.width === 1120 && Module.canvas.height === 680);
  assert.deepEqual(errors, []);
  console.log('PASS resize');
  await page.setViewportSize({width: 1440, height: 900});
  await page.keyboard.press('Control+n');
  const imageBytes = await readFile(new URL('../assets/image_42d2f96b0d5effe1.png', import.meta.url));
  let imageChooser = page.waitForEvent('filechooser');
  await page.keyboard.press('Control+i');
  await (await imageChooser).setFiles({name: 'uploaded.png', mimeType: 'image/png', buffer: imageBytes});
  await page.waitForFunction(() => Module.FS.analyzePath('/documents/assets').exists);
  const webp = await page.evaluate(async bytes => {
    const bitmap = await createImageBitmap(new Blob([new Uint8Array(bytes)], {type: 'image/png'}));
    const canvas = new OffscreenCanvas(bitmap.width, bitmap.height);
    canvas.getContext('2d').drawImage(bitmap, 0, 0);
    bitmap.close();
    return Array.from(new Uint8Array(await (await canvas.convertToBlob({type: 'image/webp'})).arrayBuffer()));
  }, Array.from(imageBytes));
  imageChooser = page.waitForEvent('filechooser');
  await page.keyboard.press('Control+i');
  await (await imageChooser).setFiles({name: 'uploaded.webp', mimeType: 'image/webp', buffer: Buffer.from(webp)});
  await page.waitForFunction(() => Module.FS.analyzePath('/documents/uploaded.png').exists && Module.canvas === document.activeElement);
  const imageSave = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  const imageFile = '/documents/' + (await imageSave).suggestedFilename();
  const imageProject = await page.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), imageFile);
  assert.equal((imageProject.match(/^image_binding /gm) || []).length, 2, 'New unsaved project imports PNG and WebP');
  assert.deepEqual(errors, []);
  assert.equal((await page.request.get(base + 'favicon.svg')).status(), 200);
  console.log('PASS PNG/WebP imports into a new project and favicon');
  await context.close();
  const appearance = await browser.newPage({viewport: {width: 1440, height: 900}, acceptDownloads: true});
  const themeErrors = [];
  appearance.on('pageerror', e => themeErrors.push(e.message));
  appearance.on('dialog', d => d.accept('theme-project.afproj'));
  const settleTheme = async () => appearance.waitForFunction(n => Module.afCanvasFrames > n + 2,
    await appearance.evaluate(() => Module.afCanvasFrames));
  const themePreference = async key => appearance.evaluate(key => {
    const source = Module.FS.readFile('/documents/editor_preferences.afprefs', {encoding: 'utf8'});
    return Number(source.split('\n').find(line => line.startsWith(key + ' ')).split(' ')[1]);
  }, key);
  const saveThemeProject = async () => {
    const downloaded = appearance.waitForEvent('download');
    await appearance.keyboard.press('Control+s');
    const path = '/documents/' + (await downloaded).suggestedFilename();
    return appearance.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), path);
  };
  await appearance.goto(base);
  await appearance.waitForFunction(() => Module.afCanvasFrames > 3);
  await appearance.keyboard.press('Escape');
  await appearance.keyboard.press('Control+n');
  await settleTheme();
  const beforeTheme = await saveThemeProject();
  await appearance.keyboard.press('Control+,');
  await settleTheme();
  // Every preview card selects and saves its distinct preset, including both light themes.
  for (const [i, theme] of [1, 3, 4, 2, 6, 7, 8, 9].entries()) {
    await appearance.mouse.click(480 + (i % 4) * 160, 278 + Math.floor(i / 4) * 84);
    await settleTheme();
    assert.equal(await themePreference('theme'), theme);
  }
  await appearance.mouse.move(680, 462);
  await appearance.mouse.down();
  await appearance.mouse.move(700, 462, {steps: 5});
  await appearance.keyboard.press('Escape');
  await appearance.mouse.up();
  await settleTheme();
  assert.equal(await themePreference('theme'), 9, 'Cancel restores the selected preset');
  assert.equal(await themePreference('theme_accent'), 0xA38DFF, 'Cancel preserves stored custom colors');
  await appearance.mouse.click(640, 462);
  await settleTheme();
  await appearance.keyboard.press('Control+a');
  await appearance.keyboard.type('59D7C3');
  await appearance.keyboard.press('Enter');
  await settleTheme();
  assert.equal(await themePreference('theme'), 5);
  assert.equal(await themePreference('theme_accent'), 0x59D7C3);
  await appearance.mouse.click(1008, 462);
  await settleTheme();
  assert.equal(await themePreference('theme_accent'), 0x59D7C4);
  await appearance.mouse.click(960, 462);
  await settleTheme();
  await appearance.mouse.move(680, 462);
  await appearance.mouse.down();
  await appearance.mouse.move(700, 462, {steps: 5});
  await appearance.mouse.up();
  await settleTheme();
  assert.equal(await themePreference('theme_accent'), 0x59D7C3 + 20);
  // An invalid edit stays pending; Escape discards it without changing preferences.
  await appearance.mouse.click(680, 462);
  await settleTheme();
  await appearance.keyboard.type('12');
  await appearance.keyboard.press('Enter');
  await settleTheme();
  assert.equal(await themePreference('theme_accent'), 0x59D7C3 + 20);
  await appearance.keyboard.press('Escape');
  await appearance.mouse.click(480, 278);
  await settleTheme();
  assert.equal(await themePreference('theme'), 1);
  await appearance.mouse.click(976, 428);
  await appearance.mouse.click(973, 629);
  await settleTheme();
  assert.equal(await themePreference('theme'), 5);
  assert.equal(await themePreference('theme_accent'), 0x59D7C3 + 20);
  assert.equal(await themePreference('theme_depth'), 100);
  await appearance.keyboard.press('Escape');
  await settleTheme();
  const afterTheme = await saveThemeProject();
  assert.equal(afterTheme.replace(/^metadata .*\n|^change .*\n/gm, ''),
    beforeTheme.replace(/^metadata .*\n|^change .*\n/gm, ''), 'Theme changes preserve artwork');
  await appearance.evaluate(() => new Promise((done, fail) => Module.FS.syncfs(false, e => e ? fail(e) : done())));
  await appearance.reload();
  await appearance.waitForFunction(() => Module.afCanvasFrames > 3);
  assert.equal(await themePreference('theme'), 5);
  assert.equal(await themePreference('theme_accent'), 0x59D7C3 + 20);
  assert.equal(await themePreference('theme_depth'), 100);
  assert.equal(await appearance.evaluate(() => document.documentElement.style.getPropertyValue('--ui-accent')), '#59d7d7');
  await appearance.keyboard.press('Control+,');
  await settleTheme();
  await appearance.mouse.click(482, 713);
  await settleTheme();
  assert.equal(await themePreference('theme'), 1);
  assert.equal(await themePreference('theme_depth'), 0);
  assert.deepEqual(themeErrors, []);
  console.log('PASS eight themes, color typing/stepping/dragging, reset and persisted preferences');
  await appearance.close();
  const particles = await browser.newPage({viewport: {width: 1440, height: 900}, acceptDownloads: true});
  particles.on('dialog', d => d.accept('particles.afproj'));
  await particles.goto(`${base}?smoke=particle-inputs&frames=100000`);
  await particles.waitForFunction(() => Module.afCanvasFrames > 3);
  await particles.mouse.move(1320, 500);
  await particles.waitForFunction(n => Module.afCanvasFrames > n + 2, await particles.evaluate(() => Module.afCanvasFrames));
  await particles.mouse.wheel(0, 1000);
  await particles.waitForFunction(n => Module.afCanvasFrames > n + 2, await particles.evaluate(() => Module.afCanvasFrames));
  // Content ends at 942; scrolling to its limit puts the spawn row at y=639.
  await particles.mouse.click(1360, 640);
  await particles.waitForFunction(n => Module.afCanvasFrames > n + 2, await particles.evaluate(() => Module.afCanvasFrames));
  await particles.keyboard.press('Control+a');
  await particles.keyboard.type('80', {delay: 20});
  await particles.keyboard.press('Enter');
  await particles.waitForFunction(n => Module.afCanvasFrames > n + 2, await particles.evaluate(() => Module.afCanvasFrames));
  await particles.mouse.click(1410, 640);
  await particles.waitForFunction(n => Module.afCanvasFrames > n + 2, await particles.evaluate(() => Module.afCanvasFrames));
  const particleSave = particles.waitForEvent('download');
  await particles.keyboard.press('Control+s');
  const particleFile = '/documents/' + (await particleSave).suggestedFilename();
  const particleProject = await particles.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), particleFile);
  const emitter = particleProject.split('\n').find(line => line.startsWith('particle ')).split(/\s+/);
  assert.equal(Number(emitter[8]), 88);
  console.log('PASS particle typing, step buttons and inspector scrolling');
  await particles.close();
  const player = await browser.newPage({viewport: {width: 1280, height: 720}});
  await player.goto(`${base}player.html`);
  await player.waitForFunction(() => Module.afCanvasFrames > 3);
  const before = await player.locator('canvas').screenshot();
  await player.waitForFunction(n => Module.afCanvasFrames > n + 15, await player.evaluate(() => Module.afCanvasFrames));
  assert.notDeepEqual(await player.locator('canvas').screenshot(), before, 'Player animation must advance');
  await player.locator('#package-file').setInputFiles(resolve(root, '../assets/arcflow_demo.afp'));
  await player.waitForURL(/package=/);
  await player.waitForFunction(() => Module.afCanvasFrames > 3);
  console.log('PASS player animation and package upload');
  await player.close();
  for (const name of ['index.html', 'index.js', 'index.wasm', 'index.data', 'player.html', 'player.js', 'player.wasm', 'player.data'])
    assert.ok((await stat(resolve(root, name))).size > 0);
} finally {
  await browser.close();
  server.close();
}
