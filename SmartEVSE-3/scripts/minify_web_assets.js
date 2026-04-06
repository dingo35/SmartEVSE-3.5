#!/usr/bin/env node

// This utility minifies the two packed web assets used by the firmware:
//   - data/index.html
//   - data/styling.css
//
// Why:
// - It saves a lot of bytes in the embedded filesystem image.
// - Smaller assets mean less flash/storage use and less transfer overhead.
//
// Safety:
// - The minification is whitespace/comment-only for HTML/CSS structure.
// - Runtime functionality is unchanged: same DOM/script logic and same CSS rules.
// - Script/style/pre/textarea blocks are protected in HTML minification so their
//   content is not altered.

const fs = require('fs');
const path = require('path');

function minifyCss(input) {
  // Compact CSS by removing comments and unnecessary whitespace.
  // This keeps selectors/properties/values intact.
  return input
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/\s+/g, ' ')
    .replace(/\s*([{}:;,>+~])\s*/g, '$1')
    .replace(/;}/g, '}')
    .trim() + '\n';
}

function minifyHtml(input) {
  // Protect blocks where whitespace/content must remain untouched.
  const blocks = [];
  const protectedHtml = input
    .replace(/<(script|style|pre|textarea)\b[\s\S]*?<\/\1>/gi, (match) => {
      const idx = blocks.push(match) - 1;
      return `___HTML_BLOCK_${idx}___`;
    })
    .replace(/<!--[\s\S]*?-->/g, '')
    .replace(/\s{2,}/g, ' ')
    .replace(/>\s+</g, '><')
    .trim()
    .replace(/___HTML_BLOCK_(\d+)___/g, (_, idx) => blocks[Number(idx)]);

  return protectedHtml + '\n';
}

function run() {
  // Resolve asset paths from current repository root.
  const root = process.cwd();
  const htmlPath = path.join(root, 'data', 'index.html');
  const cssPath = path.join(root, 'data', 'styling.css');

  const htmlBefore = fs.readFileSync(htmlPath, 'utf8');
  const cssBefore = fs.readFileSync(cssPath, 'utf8');

  const htmlAfter = minifyHtml(htmlBefore);
  const cssAfter = minifyCss(cssBefore);

  // Write minified content back to disk in place.
  fs.writeFileSync(htmlPath, htmlAfter);
  fs.writeFileSync(cssPath, cssAfter);

  const htmlDelta = Buffer.byteLength(htmlAfter) - Buffer.byteLength(htmlBefore);
  const cssDelta = Buffer.byteLength(cssAfter) - Buffer.byteLength(cssBefore);

  // Print before/after sizes so the space savings are visible immediately.
  console.log(`Minified data/index.html: ${Buffer.byteLength(htmlBefore)} -> ${Buffer.byteLength(htmlAfter)} bytes (${htmlDelta >= 0 ? '+' : ''}${htmlDelta})`);
  console.log(`Minified data/styling.css: ${Buffer.byteLength(cssBefore)} -> ${Buffer.byteLength(cssAfter)} bytes (${cssDelta >= 0 ? '+' : ''}${cssDelta})`);
}

run();
