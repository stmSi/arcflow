import assert from 'node:assert/strict';

export async function testVectorFrames(browser, base) {
  const page = await browser.newPage({viewport: {width: 1440, height: 900}, acceptDownloads: true});
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('dialog', d => d.accept());
  await page.goto(base);
  await page.waitForFunction(() => Module.afCanvasFrames > 3);
  await page.keyboard.press('Escape');
  // An older blank document also verifies migration to the current format.
  const blank = 'arcflow_project 16\ncanvas 1280 720\nnext_id 1\nnext_animation_id 101\nnext_asset_id 1\nmetadata arcflow-blank 0\nlayout_canvas 1280 720 1280 720 1 0 0 0 0\nvfx_lod 65536 1 1 1000 5000 1\nclip\nid 100\nname Main\nduration 4\nloop 1\nloop_region 0 4\nend_clip\n';
  const open = async text => {
    const chooser = page.waitForEvent('filechooser');
    await page.keyboard.press('Control+o');
    await (await chooser).setFiles({name: 'frames.afproj', mimeType: 'text/plain', buffer: Buffer.from(text)});
    await page.waitForFunction(() => Module.FS.analyzePath('/documents/session.json').exists &&
      JSON.parse(Module.FS.readFile('/documents/session.json', {encoding: 'utf8'})).project.endsWith('/frames.afproj'));
    await settle();
  };
  const settle = async () => page.waitForFunction(n => Module.afCanvasFrames > n + 3,
    await page.evaluate(() => Module.afCanvasFrames));
  const save = async () => {
    await settle();
    const download = page.waitForEvent('download');
    await page.keyboard.press('Control+s');
    await download;
    return page.evaluate(() => Module.FS.readFile('/documents/frames.afproj', {encoding: 'utf8'}));
  };
  const blocks = (text, type, end) => [...text.matchAll(new RegExp(`^${type}\\n([\\s\\S]*?)^${end}$`, 'gm'))].map(m => m[1]);
  const nodes = text => blocks(text, 'node', 'end');
  const tracks = text => blocks(text, 'track', 'end_track');
  await open(blank);
  await page.keyboard.press('Shift+F6');
  await page.keyboard.press('r');
  await page.mouse.move(500, 250);
  await page.mouse.down();
  await page.mouse.move(650, 360, {steps: 6});
  await page.mouse.up();
  let project = await save();
  assert.equal(nodes(project).length, 2, 'A drawing creates its frame group and vector shape');
  assert.equal(tracks(project).length, 1);
  assert.match(tracks(project)[0], /^cel_frame 0$/m);
  assert.match(nodes(project)[1], /^parent 1$/m);
  await page.mouse.click(395, 708); // Duplicate drawing.
  project = await save();
  assert.equal(nodes(project).length, 4);
  assert.equal(tracks(project).length, 2);
  assert.match(tracks(project)[1], /^cel_frame 1$/m);
  await page.keyboard.press('v');
  await page.mouse.click(560, 300);
  await page.keyboard.press('ArrowRight');
  const moved = await save();
  assert.equal(nodes(moved)[1], nodes(project)[1], 'Editing the duplicate preserves the original drawing');
  assert.notEqual(nodes(moved)[3], nodes(project)[3], 'The duplicate is independently editable');
  await page.mouse.click(300, 708); // Blank drawing at frame 3.
  project = await save();
  assert.equal(nodes(project).length, 5);
  assert.equal(tracks(project).length, 3);
  const ghost = await page.screenshot({clip: {x: 490, y: 240, width: 180, height: 140}});
  await page.mouse.click(300, 736); // Onion skin off.
  await settle();
  const withoutGhost = await page.screenshot({clip: {x: 490, y: 240, width: 180, height: 140}});
  assert.notDeepEqual(ghost, withoutGhost, 'Onion skin shows the previous vector drawing');
  await page.mouse.click(560, 300);
  await page.keyboard.press('Delete');
  assert.equal(nodes(await save()).length, 5, 'Hidden drawings cannot be picked or deleted through an empty frame');
  await page.keyboard.press('4'); // Existing path tool, current frame.
  project = await save();
  assert.equal(nodes(project).length, 6);
  assert.match(nodes(project).at(-1), /^type path$/m);
  assert.match(nodes(project).at(-1), /^parent 5$/m);
  await page.mouse.click(343, 764); // Hold +.
  project = await save();
  assert.match(tracks(project).at(-1), /^cel_length 2$/m);
  await page.mouse.click(432, 764); // FPS +.
  project = await save();
  assert.match(project, /^frame_rate 25$/m);
  await page.keyboard.press('Control+z');
  assert.match(await save(), /^frame_rate 0$/m, 'Undo restores default 24 fps');
  await page.keyboard.press('Control+y');
  project = await save();
  assert.match(project, /^frame_rate 25$/m);
  await page.mouse.click(330, 792); // Fit playback/export length to the drawings.
  project = await save();
  assert.ok(Math.abs(Number(project.match(/^duration (.+)$/m)[1]) - 4 / 25) < 1e-6);
  await open(project);
  assert.equal(await save(), project, 'Drawing metadata and geometry round-trip in the browser');
  await page.keyboard.press(']');
  await page.mouse.click(395, 736); // Delete current held drawing.
  const deleted = await save();
  assert.equal(tracks(deleted).length, 2);
  assert.equal(nodes(deleted).length, 4);
  await page.keyboard.press('Control+z');
  assert.equal(tracks(await save()).length, 3, 'Undo restores drawing and its timing');
  await page.mouse.click(395, 708);
  const appended = tracks(await save());
  assert.equal(appended.length, 4);
  assert.match(appended.at(-1), /^cel_frame 4$/m, 'Duplicate skips occupied drawings and held cells');
  await page.keyboard.press('Control+z');
  assert.equal(tracks(await save()).length, 3);
  assert.deepEqual(errors, []);
  await page.close();
  console.log('PASS vector frames: drawing, duplication, onion skin, holds, fps, undo and reopen');
}
