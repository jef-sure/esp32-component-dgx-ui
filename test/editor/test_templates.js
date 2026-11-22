/*
 * node test_templates.js: the builder's page before a browser sees it.
 *  - every template of editor/app.js compiled by the Vue of editor/vendor/: a mistake in one leaves the page blank,
 *    and no other check sees it;
 *  - every text the page shows has a translation in each language of editor/lang/: the texts given to t() in app.js,
 *    the words of the loader's codes and of the keys there, what the store says and the reader of descriptions refuses
 *    with, as store.js and description.js write them.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const EDITOR = path.join(__dirname, '../../editor');
let failures = 0;

/* what the compiler of Vue asks of the DOM: to decode the entities of a text and of an attribute */
const decoder = () => ({
    html: '',
    set innerHTML(value) { this.html = value; },
    get textContent() { return this.html; },
    get children() { const html = this.html; return [{ getAttribute: () => html.replace(/^<div foo="|">$/g, '') }]; },
});
const sandbox = { console, window: {}, document: { createElement: decoder, createElementNS: decoder }, navigator: { userAgent: '' }, setTimeout,
                  Promise };
vm.createContext(sandbox);
vm.runInContext(`${fs.readFileSync(path.join(EDITOR, 'vendor/vue.global.prod.js'), 'utf8')};this.Vue = Vue;`, sandbox);
const app = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
const templates = [...app.matchAll(/template: (`(?:[^`\\]|\\.)*`)/g)].map((match) => vm.runInNewContext(match[1]));
for (const template of templates) {
    try {
        sandbox.Vue.compile(template, { onError: (error) => { throw error; }, onWarn: (warning) => { throw warning; } });
    } catch (error) {
        console.error(`  a template of the page does not compile: ${error.message}\n${template.slice(0, 300)}`);
        ++failures;
    }
}
if (templates.length < 6) console.error(`  ${templates.length} templates found in app.js`), ++failures;

/* the texts */
const texts = new Set();
const unquote = (text) => text.replace(/\\(.)/g, '$1');
/* the texts of the templates as Vue reads them, the escapes of the source's template literals undone; then the rest */
const script = app.replace(/template: `(?:[^`\\]|\\.)*`/g, '');
for (const source of [...templates, script]) for (const call of source.matchAll(/\bt\(\s*'((?:[^'\\]|\\.)+)'/g)) texts.add(unquote(call[1]));
const long = app.match(/const LONG_TEXTS = (\{[^]*?\n    \});/);
if (!long) console.error('  app.js: LONG_TEXTS not found'), ++failures;
else for (const text of Object.values(vm.runInNewContext(`(${long[1]})`))) texts.add(text);
for (const block of [/const CODE_WORDS = \{([^}]*)\}/, /const KEY_NAMES = \[([^\]]*)\]/]) {
    const found = app.match(block);
    if (!found) console.error(`  app.js: ${block} not found`), ++failures;
    else for (const value of found[1].matchAll(/:\s*'((?:[^'\\]|\\.)+)'|\[?\s*'((?:[^'\\]|\\.)+)'/g)) texts.add(unquote(value[1] ?? value[2]));
}
const store = fs.readFileSync(path.join(EDITOR, 'store.js'), 'utf8');
for (const call of store.matchAll(/\b(?:say|said)\(\s*'((?:[^'\\]|\\.)+)'/g)) texts.add(unquote(call[1]));
for (const call of store.matchAll(/\{ text: '((?:[^'\\]|\\.)+)'/g)) texts.add(unquote(call[1]));
for (const call of store.matchAll(/\bwhy: '((?:[^'\\]|\\.)+)'/g)) texts.add(unquote(call[1]));
for (const call of store.matchAll(/wrong = \['((?:[^'\\]|\\.)+)'/g)) texts.add(unquote(call[1]));
const description = fs.readFileSync(path.join(EDITOR, 'description.js'), 'utf8');
for (const call of description.matchAll(/\b(?:fail\(|why: )'((?:[^'\\]|\\.)+)'/g)) texts.add(unquote(call[1]));
/* a text of values alone reads the same in every language */
for (const text of [...texts]) if (/^[{}0-9 ]*$/.test(text)) texts.delete(text);
if (texts.size < 80) console.error(`  only ${texts.size} texts found: the patterns above no longer match the sources`), ++failures;

global.DGX_UI_LANGUAGES = require(path.join(EDITOR, 'lang.js')).DGX_UI_LANGUAGES;
const languages = fs.readdirSync(path.join(EDITOR, 'lang')).filter((name) => name.endsWith('.js'));
for (const name of languages) require(path.join(EDITOR, 'lang', name));
let missing = 0, unused = 0;
for (const [code, language] of Object.entries(global.DGX_UI_LANGUAGES)) {
    if (code === 'en') continue;
    for (const text of texts) {
        if (!(text in language.texts)) console.error(`  ${code}: no translation: ${text}`), ++missing;
        else if ((text.match(/\{\d\}/g) ?? []).sort().join() !== (language.texts[text].match(/\{\d\}/g) ?? []).sort().join())
            console.error(`  ${code}: the values of "${text}" are not those of its translation`), ++missing;
    }
    for (const text of Object.keys(language.texts)) {
        if (!texts.has(text)) console.error(`  ${code}: a translation of no text of the page: ${text}`), ++unused;
    }
}
failures += missing + unused;
if (!languages.length) console.error('  no language in editor/lang/'), ++failures;

if (failures) {
    console.error(`test_templates.js: ${failures} check(s) failed`);
    process.exit(1);
}
console.log(`test_templates.js: ${templates.length} templates compiled; ${texts.size} texts, each translated in ${languages.length} language(s)`);
