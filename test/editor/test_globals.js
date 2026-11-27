/*
 * The builder's scripts on a page of another's that mounts it, as boards' page does: none of them leaves a name in the
 * page's global scope but of the builder's prefixes, dgxUi*, DgxUi* and DGX_UI_*, so a name of the page's own is never
 * declared twice, which would stop a script with a SyntaxError. Held: every script of index.html (not vendor/, the
 * page's own, nor start.js) by its declarations at the top of its text; and all of them run as classic scripts in one
 * context, in index.html's order, after a script of the page declaring boards' own names, with no error.
 *
 * node test_globals.js
 */
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.join(__dirname, '..', '..', 'editor');
const PREFIXES = /^(dgxUi|DgxUi|DGX_UI_)/;
// the names boards' page declares at the top level of its own scripts
const PAGE_NAMES = ['get', 'select', 'check', 'host', 'app', 'translate', 'LANGUAGES', 'PREFERENCES', 'FILES'];

let failures = 0, checks = 0;
function check(held, what) {
    ++checks;
    if (!held) {
        ++failures;
        console.error(`  failed: ${what}`);
    }
}

const scripts = [...fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8').matchAll(/<script src="([^"]+)">/g)].map((match) => match[1]);
const ours = scripts.filter((name) => !name.startsWith('vendor/') && name !== 'start.js');
check(ours.length >= 12 && ours.includes('description.js') && ours.includes('preview/dgx_ui_preview.js'), `the scripts of index.html: ${ours}`);
// the stylesheet and the scripts each by an element a page can take, nothing written inside the page
const html = fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8');
check(/<link rel="stylesheet" href="builder.css">/.test(html), 'the stylesheet by <link rel="stylesheet">');
check(!/<script(?![^>]*\bsrc=)[^>]*>/.test(html), 'no script written inside index.html');

/* the declarations at the top of each script's text: at the start of a line, as the scripts are written */
for (const script of ours) {
    const text = fs.readFileSync(path.join(EDITOR, script), 'utf8');
    for (const found of text.matchAll(/^(?:const|let|var|class|function|async function)\s+([A-Za-z_$][\w$]*)/gm))
        check(PREFIXES.test(found[1]), `${script}: "${found[1]}" declared at the top, not of the builder's prefixes`);
}

/* run as a page runs them: one context, boards' names first, then the vendor's and ours in index.html's order */
const sandbox = { console, TextEncoder, TextDecoder, WebAssembly, atob, btoa, setTimeout, clearTimeout, queueMicrotask, Promise, URL,
                  navigator: { userAgent: '', language: 'en' }, fetch: () => Promise.reject(new Error('no server')) };
sandbox.window = sandbox;
vm.createContext(sandbox);
vm.runInContext(`'use strict';\n${PAGE_NAMES.map((name) => `const ${name} = 'boards';`).join('\n')}\n`, sandbox, { filename: 'boards.js' });
const before = new Set(Object.getOwnPropertyNames(sandbox));
for (const script of scripts.filter((name) => name !== 'start.js')) {
    const exported = { 'vendor/vue.global.prod.js': 'Vue', 'vendor/pinia.iife.prod.js': 'Pinia' }[script];
    const source = fs.readFileSync(path.join(EDITOR, script), 'utf8') + (exported ? `\n;window.${exported} = ${exported};` : '');
    try {
        vm.runInContext(source, sandbox, { filename: script });
    } catch (error) {
        check(false, `${script} after the page's names: ${error.name}: ${error.message}`);
    }
}
check(PAGE_NAMES.every((name) => vm.runInContext(name, sandbox) === 'boards'), 'the page\'s names are still its own');
const added = Object.getOwnPropertyNames(sandbox).filter((name) => !before.has(name) && name !== 'Vue' && name !== 'Pinia');
check(added.every((name) => PREFIXES.test(name)), `properties of the global object not of the prefixes: ${added.filter((name) => !PREFIXES.test(name))}`);
check(['dgxUiBuilderMount', 'dgxUiServerHost', 'dgxUiBrowserHost', 'DgxUiPreview', 'DgxUiDescription', 'DgxUiHistory'].every((name) =>
          vm.runInContext(`typeof ${name}`, sandbox) !== 'undefined'), 'what a page calls is there');

console.log(failures ? `test_globals.js: ${failures} of ${checks} checks failed` : `test_globals.js: ${checks} checks held`);
process.exit(failures ? 1 : 0);
