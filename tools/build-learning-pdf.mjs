#!/usr/bin/env node
// Use the pinned browser tooling already used by web CI.
import {chromium} from '../web/node_modules/playwright/index.mjs';
import {resolve} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';
import {stat} from 'node:fs/promises';
const site = resolve(process.argv[2] || fileURLToPath(new URL('../build/web/site/learn', import.meta.url)));
const browser = await chromium.launch({headless: true, ...(process.env.ARCFLOW_CHROMIUM ? {executablePath:process.env.ARCFLOW_CHROMIUM} : {})});
try {
  const page = await browser.newPage();
  await page.goto(pathToFileURL(resolve(site, 'print.html')).href);
  await page.evaluate(() => document.fonts.ready);
  const path = resolve(site, 'Arcflow-Tutorials.pdf');
  await page.pdf({path, format:'A4', printBackground:true, preferCSSPageSize:true, tagged:true, outline:true,
    displayHeaderFooter:true, headerTemplate:'<span></span>',
    footerTemplate:'<div style="font:9px sans-serif;color:#52647e;width:100%;text-align:center">Arcflow · <span class="pageNumber"></span> / <span class="totalPages"></span></div>'});
  console.log(`Tutorial PDF: ${path} (${(await stat(path)).size} bytes)`);
} finally { await browser.close(); }
