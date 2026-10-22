/* Presentation only: lesson prose lives in catalog.json. Works offline without fetch. */
'use strict';
const catalog = JSON.parse(document.querySelector('#catalog').textContent);
const main = document.querySelector('#lesson');
const articles = [...main.querySelectorAll('article')];
const links = [...document.querySelectorAll('[data-guide]')];
let stopDemo = () => {};
const read = key => { try { return JSON.parse(localStorage.getItem(key)); } catch { return null; } };
const save = (key, value) => { try { localStorage.setItem(key, JSON.stringify(value)); } catch { /* Private storage: keep working. */ } };
const el = (tag, text, parent) => { const node = document.createElement(tag); if (text) node.textContent = text; parent?.append(node); return node; };
const button = (text, parent, action) => { const b = el('button', text, parent); b.type = 'button'; b.addEventListener('click', action); return b; };
function slider(parent, name, min, max, value, step = 1) {
  const label = el('label', name, parent), input = el('input', '', label);
  Object.assign(input, {type: 'range', min, max, step, value});
  input.setAttribute('aria-label', name);
  return input;
}
function interactive(parent, kind) {
  const box = el('section', '', parent); box.className = 'demo';
  el('h3', 'Try it here', box);
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '0 0 480 160'); svg.setAttribute('role', 'img');
  box.append(svg);
  let timer;
  stopDemo = () => clearInterval(timer);
  if (kind === 'frames') {
    el('p', 'Three drawings, four exposures. Hold repeats the middle drawing; FPS changes playback speed.', box);
    const cells = el('div', '', box); cells.className = 'cells';
    const fps = slider(box, 'Frames per second', 1, 12, 4);
    const hold = slider(box, 'Middle drawing hold', 1, 4, 2);
    const label = el('label', '', box), onion = el('input', '', label); onion.type = 'checkbox'; label.append(' Onion skin');
    const status = el('p', '', box); status.setAttribute('role', 'status');
    let frame = 0, playing = false;
    const exposure = () => [0, ...Array(Number(hold.value)).fill(1), 2];
    function draw() {
      const sequence = exposure(); frame %= sequence.length;
      const index = sequence[frame], x = [60, 220, 380][index];
      svg.setAttribute('aria-label', `Drawing ${index + 1}, exposure ${frame + 1}`);
      svg.innerHTML = (onion.checked ? [index - 1, index + 1].filter(i => i >= 0 && i <= 2).map(i => `<circle cx="${[60,220,380][i]}" cy="80" r="25" fill="${i < index ? '#7ed6ff' : '#ffa7d3'}" opacity=".3"/>`).join('') : '') + `<circle cx="${x}" cy="80" r="25" fill="#9ccaff"/>`;
      cells.replaceChildren(); sequence.forEach((drawing, i) => { const b = button(`${i + 1}: ${'ABC'[drawing]}`, cells, () => { frame = i; draw(); }); b.setAttribute('aria-pressed', String(i === frame)); });
      status.textContent = `Exposure ${frame + 1} / ${sequence.length} · Drawing ${'ABC'[index]} · ${fps.value} FPS`;
    }
    function schedule() { clearInterval(timer); if (playing) timer = setInterval(() => { frame++; draw(); }, 1000 / Number(fps.value)); }
    const play = button('Play', box, () => { playing = !playing; play.textContent = playing ? 'Pause' : 'Play'; schedule(); });
    fps.oninput = () => { schedule(); draw(); }; hold.oninput = onion.onchange = draw;
    draw();
  } else if (kind === 'curves') {
    el('p', 'Scrub time and compare how easing changes the distance traveled between two keys.', box);
    const label = el('label', 'Easing ', box), select = el('select', '', label);
    for (const name of ['Linear', 'Ease both', 'Hold']) el('option', name, select);
    const time = slider(box, 'Time', 0, 1, .5, .01);
    const status = el('p', '', box);
    const value = t => select.value === 'Hold' ? (t === 1 ? 1 : 0) : select.value === 'Ease both' ? t * t * (3 - 2 * t) : t;
    function draw() {
      const t = Number(time.value), v = value(t);
      const points = Array.from({length:101}, (_,i) => `${20+i*1.3},${135-value(i/100)*110}`).join(' ');
      svg.setAttribute('aria-label', `${select.value}: ${(t*100).toFixed(0)}% time, ${(v*100).toFixed(0)}% distance`);
      svg.innerHTML = `<path d="M20 20V135H155" fill="none" stroke="#52647e"/><polyline points="${points}" fill="none" stroke="#9ccaff" stroke-width="3"/><circle cx="${20+t*130}" cy="${135-v*110}" r="6" fill="#fff"/><path d="M215 80H440" stroke="#52647e"/><circle cx="${215+v*225}" cy="80" r="18" fill="#a9e4c3"/>`;
      status.textContent = `${(t*100).toFixed(0)}% time → ${(v*100).toFixed(0)}% distance. This sketch illustrates easing; edit the actual curve in the editor.`;
    }
    time.oninput = select.onchange = draw; draw();
  } else {
    el('p', 'Change the colors and drag the direction control to see how a linear gradient is placed.', box);
    const colors = ['#8bbaff', '#ffb3ca'].map((value, i) => { const label = el('label', `Stop ${i+1} `, box), c = el('input','',label); c.type = 'color'; c.value = value; return c; });
    const direction = slider(box, 'Gradient direction', 0, 360, 30);
    function draw() {
      const a = Number(direction.value) * Math.PI / 180, x = Math.cos(a)*.45, y = Math.sin(a)*.45;
      svg.setAttribute('aria-label', `Linear gradient, ${direction.value} degrees`);
      svg.innerHTML = `<defs><linearGradient id="demoGradient" x1="${.5-x}" y1="${.5-y}" x2="${.5+x}" y2="${.5+y}"><stop stop-color="${colors[0].value}"/><stop offset="1" stop-color="${colors[1].value}"/></linearGradient></defs><rect x="20" y="15" width="440" height="130" rx="16" fill="url(#demoGradient)"/><path d="M${240-x*400} ${80-y*110}L${240+x*400} ${80+y*110}" stroke="#10141b" stroke-width="4"/><circle cx="${240-x*400}" cy="${80-y*110}" r="6" fill="white" stroke="#10141b"/><circle cx="${240+x*400}" cy="${80+y*110}" r="6" fill="white" stroke="#10141b"/>`;
    }
    direction.oninput = draw; colors.forEach(c => c.oninput = draw); draw();
  }
}
function show() {
  stopDemo();
  const slug = location.hash.slice(1);
  const guide = catalog.guides.find(g => g.slug === slug) || catalog.guides.find(g => g.slug === 'quick-start');
  const article = articles.find(a => a.id === guide.slug);
  articles.forEach(a => a.hidden = a !== article);
  links.forEach(a => { if(a.dataset.guide === guide.slug) a.setAttribute('aria-current', 'page'); else a.removeAttribute('aria-current'); });
  // Rebuild from source so history navigation never duplicates controls or resets content.
  article.replaceChildren();
  el('p', `${guide.category} · ${guide.minutes} min`, article).className = 'eyebrow';
  el('h2', guide.title, article); el('p', guide.summary, article).className = 'summary';
  const actions = el('div', '', article); actions.className = 'actions';
  const practice = el('a', 'Practice in editor', actions); practice.href = `../?tutorial=${encodeURIComponent(guide.slug)}`;
  if (location.protocol === 'file:') practice.href = `https://stmsi.github.io/arcflow/?tutorial=${encodeURIComponent(guide.slug)}`;
  const key = `arcflow.lesson.${guide.slug}.${article.dataset.revision}`;
  const stored = read(key);
  let step = Number.isInteger(stored?.step) && stored.step >= 0 && stored.step < guide.steps.length ? stored.step : 0;
  let completed = stored?.completed === true;
  const status = el('p', '', article); status.setAttribute('role','status');
  const progress = el('progress', '', article); progress.max = guide.steps.length; progress.setAttribute('aria-label', 'Lesson progress');
  const card = el('section', '', article); card.className = 'step';
  const controls = el('div','',article); controls.className = 'actions';
  const back = button('Previous', controls, () => { step--; draw(); });
  const next = button('Next step', controls, () => { if(step < guide.steps.length-1) step++; else completed = true; draw(); });
  button('Restart lesson', controls, () => {step = 0; completed = false; draw();});
  function draw() {
    const current = guide.steps[step];
    status.textContent = completed ? 'Lesson complete. Try it in your own project.' : `Step ${step+1} of ${guide.steps.length}`;
    progress.value = completed ? guide.steps.length : step;
    card.replaceChildren(); el('h3',current.text,card); el('p',current.detail || '',card);
    back.disabled = step === 0; next.disabled = completed && step === guide.steps.length-1;
    next.textContent = step === guide.steps.length-1 ? 'Complete lesson' : 'Next step';
    save(key, {step, completed});
  }
  draw();
  if (guide.demo) interactive(article, guide.demo);
}
document.querySelector('#search').addEventListener('input', event => {
  const query = event.target.value.trim().toLowerCase(); let count = 0;
  catalog.guides.forEach((g,i) => { const match = JSON.stringify([g.title,g.summary,g.category,g.steps]).toLowerCase().includes(query); links[i].hidden = !match; if(match) count++; });
  document.querySelector('#results').textContent = `${count} lesson${count===1?'':'s'} found`;
});
if (location.protocol === 'file:') {
  document.querySelector('#pdf').hidden = true;
  document.querySelector('header .actions a').href = 'https://stmsi.github.io/arcflow/';
}
window.addEventListener('hashchange', show);
window.addEventListener('pagehide', () => stopDemo());
show();
