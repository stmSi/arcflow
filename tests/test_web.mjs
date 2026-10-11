import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile, stat} from 'node:fs/promises';
import {resolve, extname, sep} from 'node:path';
import {chromium} from '../web/node_modules/playwright/index.mjs';

const root = resolve(process.argv[2] || 'build/web/site');
const prefix = '/arcflow/';
const types = {'.html': 'text/html', '.js': 'application/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream'};
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
  for (const [smoke, frames] of [['studio', 4], ['template', 2], ['inspector', 8], ['vector-tools', 8], ['image', 4], ['gradient', 4], ['particle-inputs', 4], ['particle-view', 24]]) {
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
  const download = page.waitForEvent('download');
  await page.keyboard.press('Control+s');
  const saved = await download;
  assert.equal(saved.suggestedFilename(), 'browser-project.afproj');
  const projectPath = '/documents/' + saved.suggestedFilename();
  const project = await page.evaluate(path => Module.FS.readFile(path, {encoding: 'utf8'}), projectPath);
  assert.equal((project.match(/^node$/gm) || []).length, 5, 'New document gains the drawn rectangle');
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
  await context.close();
  const particles = await browser.newPage({viewport: {width: 1440, height: 900}, acceptDownloads: true});
  particles.on('dialog', d => d.accept('particles.afproj'));
  await particles.goto(`${base}?smoke=particle-inputs&frames=100000`);
  await particles.waitForFunction(() => Module.afCanvasFrames > 3);
  await particles.mouse.move(1320, 500);
  await particles.mouse.wheel(0, 1000);
  await particles.waitForFunction(n => Module.afCanvasFrames > n + 2, await particles.evaluate(() => Module.afCanvasFrames));
  // Content ends at 942; scrolling to its limit puts the spawn row at y=639.
  await particles.mouse.click(1360, 640);
  await particles.waitForFunction(n => Module.afCanvasFrames > n + 2, await particles.evaluate(() => Module.afCanvasFrames));
  await particles.keyboard.press('Control+a');
  await particles.keyboard.type('80', {delay: 20});
  await particles.keyboard.press('Enter');
  await particles.mouse.click(1410, 640);
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
