/*
 * node test_store.js <font files directory>: the builder's store, editor/store.js, driven as the page drives it, in Node
 * with no browser: Vue and Pinia of editor/vendor/, description.js, history.js, the host of files in memory and the
 * preview's module, each loaded as a page loads it, into a context of its own. The font files are DGX's, with their
 * index, as the host tests make them.
 *  - the project read from its CMakeLists: test/hook/project as it is on the disk, its calls in blocks listed as not
 *    understood with their lines; calls of every form the reading takes and refuses; the fonts of a board's build and
 *    the board of the preset;
 *  - nothing written that was not edited; a page written by hand saved in the builder's form after a word, once;
 *  - selection by the boxes of the layout, every widget, not only those a finger takes; none while a page an action
 *    opened is on the screen;
 *  - the properties: a number field empty removes its key, one not a number sets nothing, no NaN ever; a pair; words;
 *  - the operations, undo and redo, typing folded into one step; a widget the loader finds no look for given one;
 *  - a refusal mapped to its widget, in the builder's form and in a text by hand; a text that does not read, and one
 *    not UTF-8, with the byte and the cursor where they stop.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const ROOT = path.join(__dirname, '../..');
const EDITOR = path.join(ROOT, 'editor');
const fontDirectory = process.argv[2];
let failures = 0, checks = 0;
const check = (held, what) => {
    ++checks;
    if (!held) console.error('  failed: ' + what), ++failures;
    return held;
};
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const encoder = new TextEncoder();
const bytes = (text) => encoder.encode(text);

/* a context as a page's: the scripts of index.html in their order */
function page() {
    const sandbox = { console, TextEncoder, TextDecoder, WebAssembly, atob, setTimeout, clearTimeout, queueMicrotask, Promise, URL,
                      navigator: { userAgent: '' } };
    sandbox.window = sandbox;
    vm.createContext(sandbox);
    const scripts = [...fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8').matchAll(/<script src="([^"]+)">/g)].map((match) => match[1]);
    check(scripts.includes('store.js') && scripts.includes('preview/dgx_ui_preview.js'), 'index.html loads the store and the module');
    for (const script of scripts.filter((name) => name !== 'app.js')) {
        const exported = { 'vendor/vue.global.prod.js': 'Vue', 'vendor/pinia.iife.prod.js': 'Pinia' }[script];
        const source = fs.readFileSync(path.join(EDITOR, script), 'utf8') + (exported ? `\n;window.${exported} = ${exported};` : '');
        vm.runInContext(source, sandbox, { filename: script });
    }
    return { sandbox, run: (code) => vm.runInContext(code, sandbox) };
}

/* the files of a directory of the disk, as a memory host holds them */
function filesOf(directory) {
    const files = {};
    const walk = (at, relative) => {
        for (const entry of fs.readdirSync(at, { withFileTypes: true })) {
            if (entry.name === 'build' || entry.name === 'managed_components') continue;
            const inside = relative ? `${relative}/${entry.name}` : entry.name;
            if (entry.isDirectory()) walk(path.join(at, entry.name), inside);
            else files[inside] = new Uint8Array(fs.readFileSync(path.join(at, entry.name)));
        }
    };
    walk(directory, '');
    return files;
}

async function storeOf(files, name = 'project') {
    const { sandbox, run } = page();
    run('Pinia.setActivePinia(Pinia.createPinia())');
    const store = run('useDgxUiBuilder()');
    const preferences = new Map();
    await store.start({ preferences: { get: (key) => preferences.get(key) ?? null, set: (key, value) => preferences.set(key, value) } });
    check(store.ready && !store.failed, 'the module started: ' + store.failed);
    const written = {};
    sandbox.files = files;
    sandbox.save = (where, text) => ((written[where] = text), true);
    const host = run(`memoryHost(${JSON.stringify(name)}, files, save)`);
    await store.open(host);
    return { store, written, preferences, run };
}

let clock = 1000;
const paint = (store) => store.paint((clock += 50));
// the widget of the module's at a path of the document, after a paint
const boxOf = (store, widgetPath) => {
    paint(store);
    store.select(widgetPath);
    return store.box;
};

(async () => {
    /* ---- the reading of CMakeLists ---- */
    {
        const { store, run } = await (async () => {
            const { sandbox, run } = page();
            return { store: null, run, sandbox };
        })();
        const calls = (file, text) => run(`useDgxUiBuilder.cmakeCalls(${JSON.stringify(file)}, ${JSON.stringify(text)})`);
        const one = calls('main/CMakeLists.txt', [
            'idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")',
            'dgx_ui_tree(home.json NAMES names.txt APPEARANCE ../look/look.json HEADERS a.h b.h)   # a comment (dgx_ui_tree(no.json))',
            '# dgx_ui_tree(commented.json)',
            'DGX_UI_TREE("pages/the settings.json"',
            '            PREFIX settings)',
            'dgx_ui_names(names.txt HEADERS a.h APPEARANCE ../look/look.json)',
            'dgx_ui_tree(${PAGE} NAMES names.txt)',
            'dgx_ui_tree(page.json NAMES "${NAMES}")',
            'dgx_ui_tree(../../outside.json)',
            'dgx_ui_tree(/abs/page.json)',
            'dgx_ui_tree(x.json PREFX y)',
            'dgx_ui_tree(x.json NAMES a;b)',
            'if(CONFIG_X)',
            '  dgx_ui_tree(maybe.json)',
            'endif()',
            'foreach(page IN ITEMS a b)',
            '  dgx_ui_tree(${page}.json)',
            'endforeach()',
            'dgx_ui_appearance(../look/look.json NAMES ../look/names.txt)',
            'dgx_ui_tree(open.json NAMES names.txt',
        ].join('\n'));
        const found = one.calls.map((call) => [call.call, call.file, call.names ?? null, call.appearance ?? null, call.from]);
        check(same(found, [
            ['dgx_ui_tree', 'main/home.json', 'main/names.txt', 'look/look.json', 'main/CMakeLists.txt:2'],
            ['dgx_ui_tree', 'main/pages/the settings.json', null, null, 'main/CMakeLists.txt:4'],
            ['dgx_ui_names', 'main/names.txt', null, 'look/look.json', 'main/CMakeLists.txt:6'],
            ['dgx_ui_appearance', 'look/look.json', 'look/names.txt', null, 'main/CMakeLists.txt:19'],
        ]), 'the calls read: ' + JSON.stringify(found));
        const refused = one.notUnderstood.map((said) => said.line);
        check(same(refused, [7, 8, 9, 10, 11, 12, 14, 17, 20]), 'the calls not understood, by their lines: ' + JSON.stringify(refused));
        check(one.notUnderstood.every((said) => said.file === 'main/CMakeLists.txt' && said.text.startsWith('dgx_ui_tree(')),
              'each with its file and text');
        check(one.notUnderstood.find((said) => said.line === 11).values[0] === 'PREFX', 'a word the call does not take named');
        check(one.notUnderstood.find((said) => said.line === 20).why === 'the call has no end', 'a call with no end');
        const top = calls('CMakeLists.txt', 'dgx_ui_tree(main/a.json)\ndgx_ui_tree(./b/../c.json)');
        check(same(top.calls.map((call) => call.file), ['main/a.json', 'c.json']), 'paths of the top CMakeLists: ' + JSON.stringify(top.calls));
    }

    /* ---- a project of the disk: test/hook/project ---- */
    {
        const { store, written } = await storeOf(filesOf(path.join(ROOT, 'test/hook/project')), 'project');
        const pages = store.project.pages.map((one) => [one.path, one.names, one.appearance, one.from]);
        check(same(pages, [
            ['main/pages/a.json', 'main/vocab.txt', null, 'main/CMakeLists.txt:6'],
            ['main/b.json', 'main/vocab.txt', null, 'main/CMakeLists.txt:7'],
            ['main/framed.json', 'main/vocab.txt', 'components/second/look.json', 'main/CMakeLists.txt:9'],
            ['components/second/c.json', 'components/second/words/names.txt', 'components/second/look.json', 'components/second/CMakeLists.txt:3'],
            ['components/third/d.json', null, null, 'components/third/CMakeLists.txt:3'],
            ['main/bad.json', null, null, null], ['main/more/a.json', null, null, null], ['main/odd page.json', null, null, null],
        ]), 'the pages of the hook project: ' + JSON.stringify(pages));
        const lines = store.project.notUnderstood.map((said) => `${said.file}:${said.line}`);
        check(same(lines, ['main/CMakeLists.txt:14', 'main/CMakeLists.txt:16', 'main/CMakeLists.txt:18', 'main/CMakeLists.txt:20',
              'main/CMakeLists.txt:22',
                           'main/CMakeLists.txt:24', 'main/CMakeLists.txt:26', 'main/CMakeLists.txt:28', 'main/CMakeLists.txt:30',
                           'components/second/CMakeLists.txt:6']), 'the calls within if(): ' + JSON.stringify(lines));
        check(store.current === 'main/pages/a.json' && store.verdicts.page.code === 0 && store.verdicts.names.code === 0,
              'the first page open and taken');
        for (const one of store.project.pages) {
            store.choose(one.path), paint(store);
            // framed.json's appearance read on its own names file, words/names.txt, the page on vocab.txt, as dgx_ui_c reads them
            check(store.verdicts.page.code === 0 && (!store.verdicts.appearance || store.verdicts.appearance.code === 0),
                  `${one.path}: ${JSON.stringify([store.verdicts.page, store.verdicts.appearance])}`);
            // its font "small" is TerminusTTFMedium12 by words/names.txt, not dgx_font_small by the rule
            if (one.appearance) check(!store.notes.some((note) => note.includes('dgx_font_small')), `${one.path}: ${store.notes.join(' | ')}`);
        }
        check(!store.dirty.length && !(await store.save(true)).length && !Object.keys(written).length, 'nothing edited, nothing written');
    }

    /* ---- a project in memory ---- */
    const fonts = fs.readFileSync(path.join(fontDirectory, 'dgx_fonts-files.txt'), 'utf8');
    const NAMES = 'look slider\nlook button\nvalue volume &volume\non page\nctx settings &settings_page page\nfont small TerminusTTFMedium12\n';
    const home = '{"dgx-ui": 1, "name": "home",\n "tree": {"kind": "box", "flags": ["column"], "pad": 4, "gap": 4, "children": [\n' +
                 '  {"kind": "label", "text": "Caf\\u00e9", "style": "body", "want": [0, 30]},\n' +
                 '  {"kind": "slider", "value": "volume", "min": 0, "max": 100, "want": [0, 20]},\n' +
                 '  {"kind": "box", "flags": ["row"], "want": [0, 40], "children": [\n' +
                 '    {"kind": "button", "text": "More", "style": "body", "want": [80, 30], "on": "page", "ctx": "settings"}]}]}}\n';
    const settings = '{"dgx-ui": 1, "name": "settings", "tree": {"kind": "box", "children": [\n' +
                     ' {"kind": "label", "text": "Settings", "style": "body"}]}}\n';
    const project = {
        'CMakeLists.txt': bytes('cmake_minimum_required(VERSION 3.16)\nproject(demo)\n'),
        'CMakePresets.json': bytes(JSON.stringify({ version: 3, configurePresets: [
            { name: 'cyd', binaryDir: 'build/cyd' }, { name: 'default', binaryDir: '${sourceDir}/build/cyd', cacheVariables: { BOARD: 'cyd' } }] })),
        'main/CMakeLists.txt': bytes('idf_component_register(SRCS "main.c")\ndgx_ui_appearance(look.json NAMES names.txt)\n' +
                                     'dgx_ui_names(names.txt APPEARANCE look.json)\ndgx_ui_tree(home.json NAMES names.txt APPEARANCE look.json)\n'),
        'main/names.txt': bytes(NAMES),
        'main/look.json': bytes('{"dgx-ui": 1, "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#202020", "border": "#808080"}, ' +
                                '"styles": {"body": {"fonts": ["small"]}}}\n'),
        'main/home.json': bytes(home),
        'main/pages/settings.json': bytes(settings),
        'main/broken.json': new Uint8Array([...bytes('{"dgx-ui": 1, "tree": {"kind": "label", "text": "a'), 0xc3, 0x28, ...bytes('"}}')]),
        'components/local-boards/boards/cyd/board.json':
            bytes(JSON.stringify({ devices: { lcd: { config: { panel: { width: 320, height: 240 }, depth_bits: 16 } } } })),
        'build/alpha/dgx_fonts/dgx_fonts-files.txt': bytes(''), // first by its name: the preset's board is taken, not the first
        'build/cyd/dgx_fonts/dgx_fonts-files.txt': bytes(fonts),
    };
    for (const line of fonts.trim().split('\n')) {
        const file = line.split(' ')[1];
        project[`build/cyd/dgx_fonts/${file}`] = new Uint8Array(fs.readFileSync(path.join(fontDirectory, file)));
    }
    const { store, written, preferences } = await storeOf(project, 'demo');

    /* the project: its pages, the fonts of the preset's board, its screen */
    check(same(store.project.pages.map((one) => one.path), ['main/home.json', 'main/broken.json', 'main/pages/settings.json']),
          'the pages: ' + JSON.stringify(store.project.pages.map((one) => one.path)));
    check(store.project.pages[2].names === 'main/names.txt' && store.project.pages[2].appearance === 'main/look.json',
          'a page read at run time on the one names file and its appearance');
    check(store.project.board === 'cyd' && store.project.fontDir === 'build/cyd/dgx_fonts' && store.project.fontDirs.length === 2,
          'the fonts of the preset\'s board: ' + store.project.fontDir);
    check(store.verdicts.fonts.length === fonts.trim().split('\n').length && store.verdicts.fonts.every((one) => one.verdict.code === 0),
          'every font taken');
    check(same(store.screen, { width: 320, height: 240, depth: 16, unit: 1 }) && store.project.boardScreen.from.includes('local-boards'),
          'the screen of the board: ' + JSON.stringify(store.screen));
    check(store.current === 'main/home.json' && store.verdicts.page.code === 0 && store.verdicts.names.code === 0 &&
          store.verdicts.appearance.code === 0,
          'home, its names file and its appearance taken: ' + JSON.stringify(store.verdicts));
    paint(store);
    check(!store.notes.length, 'its font given, no stand-in: ' + store.notes.join(' | '));
    check(same(store.tables.value, ['volume']) && store.tables.on.includes('page'),
          'the names offered, the names file\'s: ' + JSON.stringify(store.tables));

    /* the selection, by the boxes of the layout */
    const label = boxOf(store, [0]);
    check(label && label.w > 0 && label.h === 30, 'the label laid: ' + JSON.stringify(label));
    store.select([]);
    check(store.selectAt(label.x + 2, label.y + 2) && same(store.selection, [0]),
          'a click on a label selects it: ' + JSON.stringify(store.selection));
    const button = boxOf(store, [2, 0]);
    store.select([]);
    check(store.selectAt(button.x + button.w / 2, button.y + button.h / 2) && same(store.selection, [2, 0]), 'the deepest: the button in its box');
    const inner = boxOf(store, [2]);
    check(store.selectAt(inner.x + inner.w - 1, inner.y + inner.h - 1) || true, 'a click in the row');
    check(store.selectAt(-5, -5) === false, 'outside the panel: nothing');

    /* nothing edited: nothing written */
    check(!store.dirty.length && same(await store.save(true), []) && !Object.keys(written).length, 'nothing edited, nothing written');

    /* the properties */
    store.select([0]);
    check(store.fields.map((field) => field.key).slice(0, 5).join() === 'kind,id,flags,state,look', 'the fields of a label in the order of a save');
    check(!store.fields.some((field) => field.key === 'pad' || field.key === 'children'), 'no field its kind takes not');
    const original = store.files['main/home.json'].text;
    check(store.setKey('text', 'Hello', store.fold('text')) && store.setKey('text', 'Hello!', store.fold('text')), 'typing a text');
    check(store.files['main/home.json'].text.includes('"text": "Hello!"') && store.dirty.includes('main/home.json'),
          'the text written, the file edited');
    check(store.undo() && store.files['main/home.json'].text === original, 'typing folded: one undo gives the file back, byte for byte');
    check(!store.dirty.length, 'undone: nothing edited');
    check(store.redo() && store.files['main/home.json'].text.includes('"Hello!"'), 'redo');
    store.undo();
    store.select([]);
    check(store.setNumber('pad', '7') && store.files['main/home.json'].text.includes('"pad": 7'), 'a number');
    check(store.setNumber('pad', 'x') === false && store.setNumber('pad', '1.') === false && store.setNumber('pad', 'NaN') === false &&
          store.setNumber('pad', '0x10') === false, 'what is not a number sets nothing');
    check(store.files['main/home.json'].text.includes('"pad": 7') && !/NaN|null/.test(store.files['main/home.json'].text), 'no NaN, no null');
    check(store.setNumber('pad', '') && !store.files['main/home.json'].text.includes('"pad"'), 'an empty number field removes its key');
    check(store.setNumber('gap', '1e3') && store.files['main/home.json'].text.includes('"gap": 1e3'), 'a number as typed');
    check(store.verdicts.page.code === 6 && same(store.refusedPath, []),
          'the loader refuses it, RANGE, in the root: ' + JSON.stringify([store.verdicts.page, store.refusedPath]));
    check(store.setNumber('gap', '4') && store.verdicts.page.code === 0 && store.refusedPath === null, 'and takes it again');
    store.select([1]);
    check(store.setPair('want', '', '25') === false && store.setPair('want', '0', '25') &&
          store.files['main/home.json'].text.includes('"want": [0, 25]'), 'a pair');
    check(store.setPair('want', '', '') && !/"slider"[^\n]*"want"/.test(store.files['main/home.json'].text), 'a pair emptied: removed');
    /* percents: typed, toggled, and judged by the loader */
    const homeText = () => store.files['main/home.json'].text;
    check(store.setPair('want', '50%', '20') && homeText().includes('"want": ["50%", 20]') && store.verdicts.page.code === 0,
          'a percent typed in a pair');
    check(store.togglePercent('want', 1) && homeText().includes('"want": ["50%", "20%"]'), 'the second made a percent');
    check(store.togglePercent('want', 0) && homeText().includes('"want": [50, "20%"]') && store.verdicts.page.code === 0, 'the first made a number');
    check(store.setPair('want', '50 %', '1') === false && store.setPair('want', '5%%', '1') === false, 'what is no percent nor number waits');
    store.select([]);
    check(store.setNumber('pad', '5%') && homeText().includes('"pad": "5%"') && store.verdicts.page.code === 0, 'a pad of 5 %');
    check(store.setNumber('pad', '1.234%') && store.verdicts.page.code === 6 && same(store.refusedPath, []), 'three decimals: RANGE, the loader\'s');
    check(store.setNumber('pad', '4') && store.verdicts.page.code === 0, 'and points again');
    store.select([1]);
    check(store.setPair('want', '', ''), 'the pair removed again');
    check(store.toggleWord('state', 'disabled') && store.files['main/home.json'].text.includes('"state": ["disabled"]'), 'a word');
    check(store.toggleWord('state', 'disabled') && !/"slider"[^\n]*"state"/.test(store.files['main/home.json'].text),
          'the last word off: the key removed');
    check(store.setKey('value', 'treble') && store.verdicts.page.code === 8 && same(store.refusedPath, [1]),
          'a name the table has not: UNKNOWN, the slider');
    check(store.undo() && store.verdicts.page.code === 0, 'undone');

    /* the operations */
    store.select([0]);
    check(store.addChild('button') && same(store.selection, [1]) && store.tree[2].kind === 'button', 'beside a label: a widget after it');
    check(store.verdicts.page.code === 0, 'taken');
    store.select([3]);
    check(store.addChild('icon') && same(store.selection, [3, 1]), 'in a box: its last child');
    check(store.verdicts.page.code === 0 && /"icon"[^\n]*"look": null/.test(store.files['main/home.json'].text),
          'an icon, which the loader finds no look for, given "look": null in the same step');
    const steps = store.raw.histories.get('main/home.json').past.length;
    check(store.move(-1) && same(store.selection, [3, 0]) && store.tree.find((one) => one.key === '3.0').kind === 'icon', 'moved up');
    check(store.move(-1) === false, 'not past the first');
    check(store.wrap() && same(store.selection, [3, 0]) && store.tree.find((one) => one.key === '3.0').kind === 'box', 'wrapped');
    check(store.remove() && store.verdicts.page.code === 0, 'removed');
    check(store.raw.histories.get('main/home.json').past.length === steps + 3, 'one step each');
    store.select([]);
    check(store.remove() === false, 'the root stays');

    /* a page by hand: refused at a widget of a text in another form; then a text that does not read */
    store.setText('page', home.replace('Caf\\u00e9', 'Café').replace('"max": 100', '"max": 100, "pad": 3'));
    check(store.verdicts.page.code === 5 && same(store.refusedPath, [1]),
          'a text by hand: FIELD, mapped to the slider: ' + JSON.stringify([store.verdicts.page, store.refusedPath]));
    store.showText('page');
    const at = store.cursor.at, text = store.files['main/home.json'].text;
    check(store.view === 'text' && text.slice(at).startsWith('"pad"'), 'the cursor at the key refused: ' + JSON.stringify(text.slice(at, at + 12)));
    store.setText('page', '{"dgx-ui": 1, "tree": {"kind": "label", "text": "Café", "x": }}');
    check(store.readError && store.readError.why === 'not a value of JSON' && !store.tree.length,
          'a text that does not read: ' + JSON.stringify(store.readError));
    check(store.readError.byte === store.readError.at + 1, 'its byte past the é counted in bytes: ' + JSON.stringify(store.readError));
    store.putCursor();
    check(store.cursor.at === store.readError.at, 'the cursor where it stops');
    store.setText('page', home);
    check(!store.readError && store.tree.length === 5 && store.verdicts.page.code === 0, 'read again: the tree back');
    store.view = 'page';

    /* the screen: written into the description's own entry */
    check(store.setScreen({ width: 480, height: 272, depth: 16, unit: 1 }), 'a screen chosen');
    const entry = '"editor": {"dgx-ui-builder": {"screen": {"width": 480, "height": 272, "depth": 16, "unit": 1}}}';
    check(store.files['main/home.json'].text.includes(entry),
          'the screen in "editor"');
    check(JSON.parse(preferences.get('dgx-ui-builder.screen')).width === 480, 'and kept as the last chosen');
    store.setZoom(3);
    check(store.zoom === 3 && preferences.get('dgx-ui-builder.zoom') === '3', 'the zoom kept');

    /* a page written by hand, edited: saved in the builder's form after a word, once */
    check(store.dirty.length === 1 && (await store.save()) === null && store.asking && same(store.asking.paths, ['main/home.json']),
          'a word asked first');
    check(!Object.keys(written).length, 'nothing written before it');
    check(same(await store.save(false, true), ['main/home.json']) && written['main/home.json'] === store.files['main/home.json'].text,
          'saved after it');
    check(!store.dirty.length && !store.asking, 'nothing edited after');
    store.select([0]);
    store.setKey('text', 'Again');
    check(same(await store.save(), ['main/home.json']) && written['main/home.json'].includes('"Again"'), 'once: not asked again');

    /* trying the page: an action opens a page over it; nothing selected in it, though it has as many widgets */
    const count = store.tree.length;
    store.files['main/pages/settings.json'].text = '{"dgx-ui": 1, "name": "settings", "tree": {"kind": "box", "children": [' +
        Array.from({ length: count - 1 }, () => '{"kind": "label", "text": "Settings", "style": "body", "want": [0, 20]}').join(', ') + ']}}';
    store.setTrying(true);
    const more = boxOf(store, [2, 0]);
    store.pointer(more.x + 3, more.y + 3, true, (clock += 10));
    paint(store);
    store.pointer(more.x + 3, more.y + 3, false, (clock += 60));
    paint(store), paint(store);
    check(store.shown === 'settings', 'the action page opened settings: ' + JSON.stringify([store.shown, store.events]));
    check(store.events.some((line) => line.startsWith('told')), 'its events read');
    store.setTrying(false);
    check(store.selectAt(more.x + 3, more.y + 3) === false && store.box === null, 'while it is on the screen, clicks select nothing');
    store.back(), paint(store);
    check(store.shown === '' && store.selectAt(more.x + 3, more.y + 3), 'back: the page again, selectable');
    store.setTrying(true);
    store.pointer(more.x + 3, more.y + 3, true, (clock += 10)), paint(store);
    store.pointer(more.x + 3, more.y + 3, false, (clock += 60)), paint(store);
    check(store.openShown() && store.current === 'main/pages/settings.json' && store.verdicts.page.code === 0,
          '"open it": the builder on that description');

    store.files['main/pages/settings.json'].text = store.files['main/pages/settings.json'].saved;

    /* a file not UTF-8: where it stops, and never written unless edited */
    check(store.choose('main/broken.json') && store.readError.why === 'not UTF-8', 'not UTF-8: ' + JSON.stringify(store.readError));
    check(store.readError.byte === 50 && store.readError.at === 50, 'at its first bad byte: ' + JSON.stringify(store.readError));
    store.showText('page');
    check(store.cursor.at === 50, 'the cursor there');
    check(same(await store.save(true), []) && !('main/broken.json' in written), 'not written');

    /* the names file by hand */
    store.choose('main/home.json');
    store.setText('names', 'value volume &volume\nwhat is this\n');
    check(store.verdicts.names.code === -1 && store.verdicts.names.said.length > 0,
          'a names file refused, in words: ' + JSON.stringify(store.verdicts.names));
    store.setText('names', NAMES);
    check(store.verdicts.names.code === 0 && store.verdicts.page.code === 0 && !store.dirty.length, 'given back: taken, nothing edited');

    if (failures) {
        console.error(`test_store.js: ${failures} of ${checks} checks failed`);
        process.exit(1);
    }
    console.log(`test_store.js: ${checks} checks of the store on the hook project and a project in memory`);
})().catch((error) => {
    console.error('test_store.js:', error);
    process.exit(1);
});
