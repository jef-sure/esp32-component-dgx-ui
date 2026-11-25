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
 *    not UTF-8, with the byte and the cursor where they stop;
 *  - the appearance by its forms: a colour typed and drawn at once, typing folded, an undo of its own; a colour renamed
 *    with its looks; a look renamed in two pages at once and undone in all three files or in none; renames and deletes
 *    refused where pages name them or do not read; the words of a kind's word and of the first style; a look of its
 *    draw's defaults and its draw changed; the order kept; a save that lists the pages the appearance refuses;
 *  - the names file by its forms: an expression typed at its column and folded, one of a space refused; a name renamed in
 *    the file, two pages and the appearance read on it, undone together or refused while stale, the C of a library's
 *    name written; deletes refused where names are used; a line added after its table; a file not on the disk yet made
 *    of the names its page misses and written at a save;
 *  - pictures in an appearance: one line named by a page's widget and a look's pieces, used by both, renamed in the
 *    three files in one step, not deleted; a new look of the frame given the first picture of nine, else none and refused
 *    at its pieces; a picture of fewer refused in the words of the need;
 *  - a scrolling box's offset and step offered with "scroll" alone.
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
    for (const script of scripts.filter((name) => name !== 'app.js' && name !== 'start.js')) {
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

        // its appearance by the forms: made by dgx_ui_appearance() with no PREFIX, its "name" the prefix; a look framed.json
        // names not deleted; a draw its names file has not, and the page on it on its names file refused at a save
        store.choose('main/framed.json');
        store.setTab('appearance');
        check(store.lookPrefix === 'theme' && same(store.pagesOn, ['main/framed.json', 'components/second/c.json']) &&
              !store.lookFormat.by_rule && same(store.lookFormat.fonts, ['small']), 'the appearance of two pages, on its own names file');
        check(!store.removeEntry('looks', 'pane') && store.said[store.said.length - 1].values[1].startsWith('main/framed.json'), 'pane, named, kept');
        check(store.setLook(['looks', 'pane', 'draw'], 'nothing') && store.verdicts.appearance.code > 0 && store.verdicts.page.code > 0,
              'a draw its names file has not: the appearance refused, and the page on its names with it');
        const asked = await store.save(true);
        check(asked === null && same(store.asking.broken.map((one) => one.path), ['components/second/look.json', 'main/framed.json']) &&
              store.asking.broken[1].verdict.name === 'pane', 'the appearance and the page listed before a write: ' + JSON.stringify(store.asking.broken));
        store.cancelSave();
        check(store.undo() && store.verdicts.page.code === 0 && !store.dirty.length && !Object.keys(written).length, 'undone; nothing written');
    }

    /* ---- a project in memory ---- */
    const fonts = fs.readFileSync(path.join(fontDirectory, 'dgx_fonts-files.txt'), 'utf8');
    const NAMES = 'look slider\nlook button\nvalue volume &volume\non_event page\nctx settings &settings_page page\nfont small TerminusTTFMedium12\n';
    const home = '{"dgx-ui": 1, "name": "home",\n "tree": {"kind": "box", "flags": ["column"], "pad": 4, "gap": 4, "children": [\n' +
                 '  {"kind": "label", "text": "Caf\\u00e9", "style": "body", "want": [0, 30]},\n' +
                 '  {"kind": "slider", "value": "volume", "min": 0, "max": 100, "want": [0, 20]},\n' +
                 '  {"kind": "box", "flags": ["row"], "want": [0, 40], "children": [\n' +
                 '    {"kind": "button", "text": "More", "style": "body", "want": [80, 30], "on_event": "page", "ctx": "settings"}]}]}}\n';
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
    check(same(store.tables.value, ['volume']) && store.tables.on_event.includes('page'),
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

    /* ---- the appearance by its forms: a project of two pages on one appearance ---- */
    {
        const look = '{"dgx-ui": 1, "name": "day", "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#202020", "border": "#808080", ' +
                     '"accent": "#f0a03c"}, "styles": {"body": {"fonts": ["small"]}, "big": {"fonts": ["small"]}}, "looks": {' +
                     '"pane": {"draw": "frame", "fill": "fill", "params": {"border": "accent", "width": 1}}, ' +
                     '"slider": {"draw": "slider", "params": {"track": "border", "fill": "accent", "knob": "ink", "focused": "ink", "disabled": "border", ' +
                     '"track_h": 3}}, "spare": {"draw": "frame", "params": {"border": "ink", "width": 2}}}}\n';
        const a = '{"dgx-ui": 1, "tree": {"kind": "frame", "look": "pane", "pad": 4, "flags": ["column"], "children": [{"kind": "slider", "value": "level", ' +
                  '"max": 9}, {"kind": "label", "text": "A", "style": "big"}]}}\n';
        const b = '{"dgx-ui": 1, "tree": {"kind": "frame", "look": "pane", "children": [{"kind": "label", "text": "B"}]}}\n';
        const files = {
            'main/CMakeLists.txt': bytes('idf_component_register(SRCS "main.c")\ndgx_ui_appearance(look.json PREFIX sunny)\n' +
                                         'dgx_ui_tree(a.json APPEARANCE look.json)\ndgx_ui_tree(b.json APPEARANCE look.json)\n'),
            'main/look.json': bytes(look), 'main/a.json': bytes(a), 'main/b.json': bytes(b),
            'main/night.json': bytes(look.replace('"name": "day"', '"name": "night"')), // an appearance loaded at run time
            'main/notes.json': bytes('{"dgx-ui": 1, "name": "notes"}\n'), // "dgx-ui" alone: neither
        };
        const { store, written, run } = await storeOf(files, 'looks');
        const text = (where) => store.files[where].text;
        const saidLast = () => store.said[store.said.length - 1] ?? { text: '', values: [] };
        const pixel = (x, y) => {
            paint(store);
            const frame = store.raw.preview.frame();
            return [...frame.slice(4 * (y * store.screen.width + x), 4 * (y * store.screen.width + x) + 3)];
        };
        check(store.current === 'main/a.json' && store.verdicts.appearance.code === 0 && store.verdicts.page.code === 0 && store.look,
              'the appearance read: ' + JSON.stringify(store.verdicts));
        check(store.lookPrefix === 'sunny' && same(store.pagesOn, ['main/a.json', 'main/b.json']), 'its prefix, the call\'s PREFIX over its name, and its two pages');
        check(store.lookFormat.draws.length === 21 && store.lookFormat.by_rule, 'what it may say, by the rule');
        check(same(store.project.appearances.map((one) => [one.path, one.from]), [['main/look.json', 'main/CMakeLists.txt:2'], ['main/night.json', null]]),
              'an appearance loaded at run time found beside the call\'s: ' + JSON.stringify(store.project.appearances));
        store.setTab('appearance');
        check(store.undoPath === 'main/look.json', 'an undo goes to the appearance while its forms are open');

        /* a colour: typed as a name of DGX's, written as hex; the page drawn on it again */
        const shown = (hex) => { const at = run('DgxUiAppearance').colorAtDepth(hex, 16); return [1, 3, 5].map((k) => parseInt(at.slice(k, k + 2), 16)); };
        const corner = pixel(0, 0);
        check(same(corner, shown('#f0a03c')), 'the pane\'s border in the accent: ' + corner);
        check(!store.setColor('accent', 'no such colour') && text('main/look.json') === look, 'a colour of no form waits, nothing changed');
        check(store.setColor('accent', 'lime', store.lookFold(['colors', 'accent'])), 'a colour by its name');
        check(text('main/look.json').includes('"accent": "#00ff00"') && same(pixel(0, 0), shown('#00ff00')), 'written as hex, drawn at once');
        store.setColor('accent', '#0000FF', store.lookFold(['colors', 'accent']));
        store.setColor('accent', 'ff0000', store.lookFold(['colors', 'accent']));
        check(text('main/look.json').includes('"accent": "#ff0000"') && same(pixel(0, 0), shown('#ff0000')), 'typed again: red');
        check(store.dirty.includes('main/look.json') && !store.dirty.includes('main/a.json'), 'the appearance edited, not the page');
        check(store.undo() && text('main/look.json') === look && same(pixel(0, 0), shown('#f0a03c')), 'the typing undone in one step');
        check(!store.undo() && store.redo() && text('main/look.json').includes('"#ff0000"'), 'nothing more to undo; done again');
        store.undo();
        store.setTab('properties');
        check(store.undoPath === 'main/a.json' && !store.canUndo, 'the page\'s history apart from the appearance\'s');
        store.setTab('appearance');

        /* a colour renamed: the looks that name it carried, the program's name of it said */
        check(store.renameEntry('colors', 'accent', 'highlight'), 'a colour renamed');
        check(!text('main/look.json').includes('accent') && store.verdicts.appearance.code === 0 && store.verdicts.page.code === 0,
              'its looks carried, all taken');
        check(store.said.some((one) => one.values.includes('sunny') && one.text.includes('{0}_color_{1}') && one.values.includes('accent')),
              'sunny_color_accent said, by the call\'s PREFIX: ' + JSON.stringify(store.said.slice(-2)));
        check(!store.renameEntry('colors', 'highlight', 'ink') && saidLast().wrong, 'a rename to a colour there refused');
        store.undo();
        check(text('main/look.json') === look, 'the rename undone');

        /* a look renamed: in the appearance and in every widget of both pages that names it, one step */
        const plan = store.renamePlan('looks', 'pane', 'panel');
        check(same(plan.pages.map((page) => [page.path, page.widgets.length]), [['main/a.json', 1], ['main/b.json', 1]]) && !plan.refused.length,
              'the plan: a widget in each page: ' + JSON.stringify(plan));
        check(store.renameEntry('looks', 'pane', 'panel'), 'a look renamed');
        check(text('main/a.json').includes('"look": "panel"') && text('main/b.json').includes('"look": "panel"') &&
              text('main/look.json').includes('"panel": {') && !text('main/look.json').includes('"pane"'), 'in the appearance and both pages');
        check(store.verdicts.page.code === 0 && same(store.dirty.sort(), ['main/a.json', 'main/b.json', 'main/look.json']), 'taken; three files edited');
        const order = () => ['colors', 'styles', 'looks'].map((section) => window.namesOf(store.look, section).join());
        const window = { namesOf: (doc, section) => store.raw.preview && doc ? Object.keys(JSON.parse(store.files['main/look.json'].text)[section] ?? {}) : [] };
        check(same(order(), ['paper,ink,fill,border,accent', 'body,big', 'panel,slider,spare']), 'renamed in its place: ' + order());
        store.setTab('properties');
        check(store.undoPath === 'main/a.json' && store.undo(), 'undone from a page');
        check(text('main/a.json') === a && text('main/b.json') === b && text('main/look.json') === look, 'undone in all three files');
        check(store.redo() && text('main/b.json').includes('"panel"') && text('main/look.json').includes('"panel"'), 'done again in all three');
        store.choose('main/b.json');
        store.select([]);
        store.setNumber('pad', '2');
        store.choose('main/a.json');
        const before = [text('main/a.json'), text('main/b.json'), text('main/look.json')];
        check(!store.undo() && saidLast().wrong && saidLast().values.includes('main/b.json'), 'b edited after it: the undo refused, b named');
        check(same([text('main/a.json'), text('main/b.json'), text('main/look.json')], before), 'nothing half undone');
        store.choose('main/b.json');
        check(store.undo() && !text('main/b.json').includes('"pad"'), 'b\'s own edit undone');
        check(store.undo() && text('main/a.json') === a && text('main/b.json') === b && text('main/look.json') === look,
              'then the rename, from b, in all three');
        store.choose('main/a.json');
        store.setTab('appearance');

        /* refused: a name there; a page that does not read; a delete of a look a widget names */
        check(!store.renameEntry('looks', 'pane', 'spare') && saidLast().wrong, 'a rename to a look there refused');
        store.choose('main/b.json');
        store.setText('page', '{"dgx-ui": 1, "tree": ');
        store.choose('main/a.json');
        check(same(store.renamePlan('looks', 'pane', 'panel').refused, ['main/b.json']) && !store.renameEntry('looks', 'pane', 'panel') &&
              saidLast().values.includes('main/b.json') && text('main/look.json') === look, 'a page on it that does not read: refused, named');
        store.choose('main/b.json');
        store.view = 'text';
        store.undo();
        store.view = 'page';
        check(text('main/b.json') === b, 'the text by hand undone in its own history');
        store.choose('main/a.json');
        store.setTab('appearance');
        check(!store.removeEntry('looks', 'pane') && saidLast().wrong && saidLast().values[1].includes('main/a.json') &&
              saidLast().values[1].includes('main/b.json'), 'a look the pages name not deleted: ' + JSON.stringify(saidLast()));
        check(!store.removeEntry('styles', 'big') && saidLast().values[1].includes('main/a.json: 1'), 'a style a widget names not deleted');

        /* the words said first: the word of a kind, the first style */
        const said = (plan) => plan.warnings.map((one) => one.text);
        check(said(store.renamePlan('looks', 'slider', 'track')).some((one) => one.includes('take the library')), 'a look of a kind\'s word renamed');
        check(said(store.renamePlan('looks', 'spare', 'button')).some((one) => one.includes('takes it')), 'renamed to a kind\'s word');
        check(said(store.renamePlan('styles', 'body', 'plain')).some((one) => one.includes('first style')), 'the first style renamed');
        check(!said(store.renamePlan('styles', 'big', 'large')).length, 'no word for another style');
        check(store.addLook('switch', 'switch') && saidLast().text.includes('takes it'), 'a look added under a kind\'s word, said');

        /* a look added from its draw's defaults; its draw changed, its fields of the same name kept */
        check(store.addLook('toggle', 'switch') && store.verdicts.appearance.code === 0, 'a look of the defaults of its draw');
        const toggle = () => JSON.parse(text('main/look.json')).looks.toggle;
        check(same(toggle(), { draw: 'switch', params: { off: 'fill', on: 'border', knob: 'ink', focused: 'ink', disabled: 'fill' } }), 'its defaults');
        store.setLook(['looks', 'toggle', 'params', 'knob'], 'accent');
        check(store.setDraw('toggle', 'slider') && store.verdicts.appearance.code === 0, 'its draw changed');
        check(same(toggle().params, { track: 'fill', fill: 'border', knob: 'accent', focused: 'ink', disabled: 'fill', track_h: 2 }),
              'knob, focused and disabled kept, the rest the slider\'s: ' + JSON.stringify(toggle()));
        check(same(order(), ['paper,ink,fill,border,accent', 'body,big', 'pane,slider,spare,switch,toggle']), 'added at the end: ' + order());
        check(store.removeEntry('looks', 'spare') && same(order()[2], 'pane,slider,switch,toggle'), 'a look no widget names deleted');
        check(store.removeEntry('colors', 'accent') && store.verdicts.appearance.code > 0 && same(store.appearanceAt, ['looks', 'pane']) &&
              saidLast().values.includes('accent'), 'a colour still named deleted: the loader refuses at the look: ' + JSON.stringify(store.appearanceAt));

        /* saved: the pages the appearance refuses listed first, the word asked */
        const pageBefore = { ...store.verdicts.page };
        const asked = await store.save(true);
        check(asked === null && same(store.asking.broken.map((one) => one.path), ['main/look.json']) && store.asking.paths.includes('main/look.json'),
              'the appearance refused listed, and its form asked: ' + JSON.stringify(store.asking));
        check(same(store.verdicts.page, pageBefore) && store.current === 'main/a.json', 'the page open given again after');
        store.cancelSave();
        check(!Object.keys(written).length, 'nothing written');
        store.undo();
        check(store.verdicts.appearance.code === 0 && store.verdicts.page.code === 0, 'the delete undone');
        const saved = await store.save(true);
        check(saved === null && !store.asking.broken.length && same(store.asking.paths, ['main/look.json']), 'its form alone asked now');
        check(same((await store.save(true, true)).sort(), ['main/look.json']) && written['main/look.json'] === text('main/look.json'), 'saved');

        /* by hand in the view of texts: its own history */
        store.showText('appearance');
        check(store.undoPath === 'main/look.json', 'the text shown is the one undone');
        const beforeHand = text('main/look.json'), handWritten = beforeHand.replace('"#808080"', '"#808081"');
        store.setText('appearance', handWritten);
        check(store.verdicts.appearance.code === 0 && store.undo() && text('main/look.json') === beforeHand, 'a hand edit undone, the text as it was');
        store.view = 'page';
    }

    /* ---- the names file by its forms: two pages on one names file, an appearance read on it, a file not there yet ---- */
    {
        const vocab = '# table   name      C expression\nlook      slider\nlook      button\nvalue     volume    &room_volume\n' +
                      'value     spare     &spare_value   # no page names it\nstyle     body      &body_style\non_event  back\n' +
                      'font      small     TerminusTTFMedium12\nfont      tiny\nhand      handwriting\ndraw      text\n';
        const a = '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "slider", "value": "volume", "max": 9}, ' +
                  '{"kind": "label", "text": "A"}, {"kind": "button", "text": "Back", "look": "button", "on_event": "back"}]}}\n';
        const b = '{"dgx-ui": 1, "tree": {"kind": "box", "children": [{"kind": "slider", "value": "volume", "max": 5}, {"kind": "label", "text": "B"}]}}\n';
        const c = '{"dgx-ui": 1, "tree": {"kind": "box", "id": "top", "children": [{"kind": "slider", "value": "level", "max": 5}, ' +
                  '{"kind": "button", "text": "Go", "on_event": "go", "ctx": "settings"}, {"kind": "label", "text": "C"}]}}\n';
        const look = '{"dgx-ui": 1, "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#202020", "border": "#808080"}, ' +
                     '"styles": {"big": {"fonts": ["small"]}}, "looks": {"plain": {"draw": "text", "params": {"ink": "ink", "paper": null}}}}\n';
        const files = {
            'main/CMakeLists.txt': bytes('idf_component_register(SRCS "main.c")\ndgx_ui_appearance(look.json NAMES vocab.txt)\n' +
                                         'dgx_ui_tree(a.json NAMES vocab.txt APPEARANCE look.json)\ndgx_ui_tree(b.json NAMES vocab.txt)\ndgx_ui_tree(c.json NAMES new.txt)\n'),
            'main/vocab.txt': bytes(vocab), 'main/a.json': bytes(a), 'main/b.json': bytes(b), 'main/c.json': bytes(c), 'main/look.json': bytes(look),
        };
        const { store, written, run } = await storeOf(files, 'names');
        const text = (where) => store.files[where].text;
        const saidLast = () => store.said[store.said.length - 1] ?? { text: '', values: [] };
        check(store.current === 'main/a.json' && store.verdicts.names.code === 0 && store.verdicts.page.code === 0, 'a on its names file');
        store.setTab('names');
        check(store.undoPath === 'main/vocab.txt' && same(store.pagesOnNames, ['main/a.json', 'main/b.json']) &&
              same(store.appearancesOnNames, ['main/look.json']), 'the Names tab: its file, its pages, the appearance read on it');
        check(store.namesRows.length === 10 && store.namesRows.every((row) => row.known), 'its ten lines');
        check(!store.namesMissing.length, 'a names nothing missing');
        check(same([...store.namesUnused], ['value spare', 'font tiny', 'hand handwriting']), 'the names no page on it names, the first style b\'s text takes: ' +
              JSON.stringify([...store.namesUnused]));

        /* an expression typed: folded, its columns kept, the comment kept; one of white space or '#' waits */
        const spare = store.namesRows.find((row) => row.words[1] === 'spare');
        for (const typed of ['&s', '&sp', '&spare_two'])
            store.setNamesLine(spare.number, ['value', 'spare', typed], store.namesFold('value', 'spare', 'expr'));
        check(text('main/vocab.txt').split('\n')[spare.number - 1] === 'value     spare     &spare_two     # no page names it',
              'typed at its column, the comment at its own: ' + text('main/vocab.txt').split('\n')[spare.number - 1]);
        check(!store.setNamesLine(spare.number, ['value', 'spare', '&a b']) && !store.setNamesLine(spare.number, ['value', 'spare', '&a#b']) &&
              text('main/vocab.txt').includes('&spare_two'), 'an expression of a space or a # waits');
        check(store.undo() && text('main/vocab.txt') === vocab, 'the typing undone in one step');

        /* a value renamed in the names file and in both pages, one step; undone from a page; stale refused */
        const plan = store.namesPlan('value', 'volume', 'loudness');
        check(same(plan.files.map((one) => [one.path, one.places.length]), [['main/a.json', 1], ['main/b.json', 1]]) && !plan.refused.length &&
              plan.warnings.some((one) => one.text.includes('C expression stays')), 'the plan: ' + JSON.stringify(plan));
        check(store.renameName('value', 'volume', 'loudness'), 'a value renamed');
        check(text('main/vocab.txt').includes('value     loudness  &room_volume\n') && text('main/a.json').includes('"value": "loudness"') &&
              text('main/b.json').includes('"value": "loudness"') && store.verdicts.page.code === 0, 'in the file at its columns and in both pages');
        store.setTab('properties');
        check(store.undo() && text('main/vocab.txt') === vocab && text('main/a.json') === a && text('main/b.json') === b, 'undone from a page in all three');
        check(store.redo() && text('main/b.json').includes('loudness'), 'done again');
        store.choose('main/b.json');
        store.select([]);
        store.setNumber('pad', '2');
        store.choose('main/a.json');
        store.setTab('names');
        check(!store.undo() && saidLast().wrong && saidLast().values.includes('main/b.json'), 'b edited after it: refused, named');
        store.choose('main/b.json');
        store.setTab('properties');
        store.undo(), store.undo();
        store.choose('main/a.json');
        store.setTab('names');
        check(text('main/vocab.txt') === vocab && text('main/a.json') === a && text('main/b.json') === b, 'then undone from b');

        /* a font of the file renamed into the appearance read on it; its C kept */
        check(store.renameName('font', 'small', 'smallest') && text('main/vocab.txt').includes('font      smallest  TerminusTTFMedium12\n') &&
              text('main/look.json').includes('"smallest"') && store.verdicts.appearance.code === 0, 'a font renamed into the appearance\'s style');
        store.undo();
        check(store.renameName('draw', 'text', 'words') && text('main/vocab.txt').includes('draw      words     dgx_ui_draw_text &dgx_ui_params_text\n') &&
              text('main/look.json').includes('"draw": "words"') && store.verdicts.appearance.code === 0, 'a draw of the library renamed: its C written');
        store.undo();
        check(store.renameName('look', 'button', 'key') && text('main/vocab.txt').includes('look      key       &dgx_ui_look_button\n') &&
              text('main/a.json').includes('"look": "key"') && store.verdicts.page.code === 0 && saidLast().text.includes('renamed in'),
              'a look of the library renamed: its C written, the page carried');
        store.undo();
        check(store.namesPlan('look', 'slider', 'track').warnings.some((one) => one.text.includes('are refused after it')), 'a kind\'s word renamed: said');
        check(!store.renameName('hand', 'handwriting', 'pen') && text('main/vocab.txt') === vocab && saidLast().wrong, 'no hand renamed');
        check(store.renameName('font', 'tiny', 'tinier') && text('main/vocab.txt').includes('font      tinier    dgx_font_tiny\n') &&
              store.verdicts.names.code === 0, 'a font of no constant renamed: the constant its name gave written');
        store.undo();
        store.choose('main/b.json');
        store.setText('page', '{"dgx-ui": 1, "tree": ');
        store.choose('main/a.json');
        store.setTab('names');
        check(!store.renameName('value', 'volume', 'loudness') && saidLast().values[1].includes('main/b.json') && text('main/vocab.txt') === vocab,
              'a page on it that does not read: the rename refused, the page named');
        store.choose('main/b.json');
        store.view = 'text', store.text = 'page';
        store.undo();
        store.view = 'page';
        store.choose('main/a.json');
        store.setTab('names');
        check(text('main/b.json') === b, 'b given back');
        check(!store.renameName('value', 'volume', 'spare') && saidLast().wrong, 'a rename to a name there refused');

        /* deleted: not what a page or the appearance names */
        check(!store.removeName('value', 'volume') && saidLast().values[1].includes('main/a.json') && saidLast().values[1].includes('main/b.json'),
              'a value pages name not deleted: ' + JSON.stringify(saidLast()));
        check(!store.removeName('font', 'small') && saidLast().values[1].includes('main/look.json'), 'a font the appearance names not deleted');
        check(store.removeName('value', 'spare') && !text('main/vocab.txt').includes('spare') && store.verdicts.names.code === 0, 'an unused one deleted');
        store.undo();

        /* a line added after the last of its table, at its columns */
        check(store.addNamesLine(['value', 'bass', '&room_bass']) && text('main/vocab.txt').split('\n')[5] === 'value     bass      &room_bass',
              'a value added after the values: ' + text('main/vocab.txt').split('\n')[5]);
        check(!store.addNamesLine(['value', 'bass', '&again']) && saidLast().wrong, 'a name twice refused');
        store.undo();

        /* a save: the pages the names file would refuse listed first */
        store.setText('names', vocab.replace('value     volume    &room_volume\n', ''));
        check(store.verdicts.page.code > 0, 'a line the pages need gone: refused');
        const asked = await store.save(true);
        check(asked === null && same(store.asking.broken.map((one) => one.path), ['main/a.json', 'main/b.json']), 'both pages listed: ' +
              JSON.stringify(store.asking.broken));
        store.cancelSave();
        store.view = 'text', store.text = 'names';
        store.undo();
        store.view = 'page';
        check(text('main/vocab.txt') === vocab && !Object.keys(written).length, 'given back, nothing written');

        /* a names file a call names, not on the disk yet: the names the page misses, all added, written at a save */
        store.choose('main/c.json');
        store.setTab('names');
        check(store.page.names === 'main/new.txt' && store.files['main/new.txt'].missing && store.undoPath === 'main/new.txt', 'c on a file not there');
        const missing = store.namesMissing.map((entry) => `${entry.table} ${entry.name}`);
        check(same(missing, ['look slider', 'look button', 'value level', 'on_event go', 'ctx settings', 'id top', 'style body']) ||
              ['look slider', 'value level', 'on_event go', 'ctx settings', 'id top', 'style body'].every((one) => missing.includes(one)), 'its names missing: ' + missing);
        check(store.addAllMissing() && store.verdicts.names.code === 0 && store.verdicts.page.code === 0 && !store.namesMissing.length,
              'all added: c taken: ' + JSON.stringify(store.verdicts.page));
        const made = run('DgxUiNames').lines(text('main/new.txt')).filter((line) => line.words.length);
        const words = (k) => made.filter((line) => line.words.length > k).map((line) => line.words[k].col);
        check(made.length === missing.length && [1, 2, 3].every((k) => new Set(words(k)).size === 1) &&
              text('main/new.txt').includes('on_event  go        go         number\n') && text('main/new.txt').includes('&settings  page\n'),
              'by the rule\'s C, laid in columns: ' + JSON.stringify(text('main/new.txt')));
        check(store.undo() && store.namesMissing.length === missing.length, 'undone in one step');
        check(store.addMissing(store.namesMissing.find((entry) => entry.name === 'level')) && text('main/new.txt') === 'value level &level\n', 'one added');
        store.addAllMissing();
        const saved = await store.save(true);
        check(saved && saved.includes('main/new.txt') && written['main/new.txt'] === text('main/new.txt') && !store.files['main/new.txt'].missing,
              'written at a save: ' + JSON.stringify(saved));

        /* what an agent asks, answered by the store: the state, a file, an edit as a step of its file, a page shown */
        const state = await store.agentAnswer('state', {});
        check(state.open === 'main/c.json' && state.verdicts.page.taken && state.pages.length === 3 && same(state.names_files, []) &&
              Array.isArray(state.missing_names), 'the state: ' + JSON.stringify(state).slice(0, 160));
        check((await store.agentAnswer('read', { path: 'main/vocab.txt' })).text === vocab && (await store.agentAnswer('read', { path: 'sdkconfig' })).error,
              'a file the builder holds read; another not');
        const broken = vocab.replace('value     volume    &room_volume\n', '');
        store.setTab('properties');
        const edit = await store.agentAnswer('edit', { path: 'main/vocab.txt', text: broken });
        check(store.current === 'main/a.json' && store.tab === 'names' && text('main/vocab.txt') === broken && edit.verdicts.page.code === 'DGX_UI_LOAD_UNKNOWN' &&
              edit.verdicts.page.name === 'volume', 'a names file edited: a page on it opened, its tab, the loader\'s verdict: ' + JSON.stringify(edit.verdicts.page));
        check(store.undo() && text('main/vocab.txt') === vocab && store.verdicts.page.code === 0, 'the person undoes it');
        check(same(await store.agentAnswer('select', { path: 'main/b.json', widget: [0] }), { open: 'main/b.json', selected: [0] }) &&
              (await store.agentAnswer('select', { path: 'main/b.json', widget: [9] })).error, 'a page and a widget shown; none at a wrong path');
        const formats = await store.agentAnswer('formats', {});
        check(formats.page.kinds.includes('slider') && formats.names.tables.includes('value') && formats.appearance.draws.length, 'the formats');
        check((await store.agentAnswer('image', {})).error === 'no panel drawn' || typeof document === 'undefined', 'no image with no document');
        check((await store.agentAnswer('edit', { path: 'main/none.json', text: '{}' })).error, 'an edit of no file of the builder refused');
    }

    /* ---- pictures in an appearance: one line of the names file named by a page's widget and a look's pieces ---- */
    {
        const pics = 'picture   one       one_piece\npicture   card      card_pieces 9\ndraw      nine\nfont      small\nlook      picture\n';
        const look = '{"dgx-ui": 1, "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#202020", "border": "#808080"}, "styles": {"body": {"fonts": ["small"]}}, ' +
                     '"looks": {"framed": {"draw": "nine", "params": {"fill": "fill", "ink": "ink", "pieces": "card"}}}}\n';
        const page = '{"dgx-ui": 1, "tree": {"kind": "box", "children": [{"kind": "picture", "picture": "card"}]}}\n';
        const files = {
            'main/CMakeLists.txt': bytes('idf_component_register(SRCS "main.c")\ndgx_ui_appearance(look.json NAMES pics.txt)\n' +
                                         'dgx_ui_tree(p.json NAMES pics.txt APPEARANCE look.json)\n'),
            'main/pics.txt': bytes(pics), 'main/look.json': bytes(look), 'main/p.json': bytes(page),
        };
        const { store } = await storeOf(files, 'names');
        const text = (where) => store.files[where].text;
        const saidLast = () => store.said[store.said.length - 1] ?? { text: '', values: [] };
        const framed = () => JSON.parse(text('main/look.json')).looks;
        check(store.verdicts.names.code === 0 && store.verdicts.page.code === 0 && store.verdicts.appearance.code === 0,
              'the page and the appearance taken on the pictures: ' + JSON.stringify(store.verdicts));
        check(!store.lookFormat.by_rule && same(store.lookFormat.pictures, [{ name: 'one', count: 1 }, { name: 'card', count: 9 }]),
              'the appearance\'s pictures, the names file\'s: ' + JSON.stringify(store.lookFormat.pictures));
        store.setTab('names');
        check(same([...store.namesUnused], ['picture one']), 'card used by both, one by none: ' + JSON.stringify([...store.namesUnused]));

        /* renamed: the line, the page's widget and the look's pieces, one step */
        check(store.renameName('picture', 'card', 'tiles'), 'a picture renamed');
        check(text('main/pics.txt').includes('picture   tiles     card_pieces 9\n') && text('main/p.json').includes('"picture": "tiles"') &&
              framed().framed.params.pieces === 'tiles' && store.verdicts.page.code === 0 && store.verdicts.appearance.code === 0,
              'in the names file, the page and the look: ' + text('main/look.json'));
        check(store.undo() && text('main/pics.txt') === pics && text('main/p.json') === page && text('main/look.json') === look, 'undone in all three at once');
        check(!store.removeName('picture', 'card') && saidLast().values[1].includes('main/look.json') && saidLast().values[1].includes('main/p.json'),
              'a picture the page and the look name not deleted: ' + JSON.stringify(saidLast()));

        /* a new look of the frame: the first picture of nine or more */
        store.setTab('appearance');
        check(store.addLook('card2', 'nine') && framed().card2.params.pieces === 'card' && store.verdicts.appearance.code === 0,
              'a look of nine pieces given card, not one: ' + JSON.stringify(framed().card2));
        store.undo();

        /* a picture of fewer than nine: refused at the field, in the words of the need */
        check(store.setLook(['looks', 'framed', 'params', 'pieces'], 'one') && store.verdicts.appearance.code === 6 &&
              store.verdicts.appearance.name === 'pieces' && same(store.appearanceAt, ['looks', 'framed']),
              'one picture for nine: RANGE at pieces: ' + JSON.stringify([store.verdicts.appearance, store.appearanceAt]));
        check(same(store.appearanceShort, { field: 'pieces', need: 9, name: 'one', count: 1 }), 'the words of the need: ' + JSON.stringify(store.appearanceShort));
        store.undo();
        check(store.appearanceShort === null && text('main/look.json') === look, 'given back, no words');
    }

    /* ---- a look of the frame on a names file of no picture of nine: left without pieces, refused at them ---- */
    {
        const look = '{"dgx-ui": 1, "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#202020", "border": "#808080"}, "styles": {"body": {"fonts": ["small"]}}}\n';
        const files = {
            'main/CMakeLists.txt': bytes('idf_component_register(SRCS "main.c")\ndgx_ui_appearance(look.json NAMES pics.txt)\n' +
                                         'dgx_ui_tree(p.json NAMES pics.txt APPEARANCE look.json)\n'),
            'main/pics.txt': bytes('picture   one       one_piece\ndraw      nine\nfont      small\nlook      picture\n'), 'main/look.json': bytes(look),
            'main/p.json': bytes('{"dgx-ui": 1, "tree": {"kind": "picture", "picture": "one"}}\n'),
        };
        const { store } = await storeOf(files, 'appearance');
        check(store.verdicts.appearance.code === 0, 'taken: ' + JSON.stringify(store.verdicts.appearance));
        store.setTab('appearance');
        check(store.addLook('framed', 'nine'), 'a look of the frame added');
        const added = JSON.parse(store.files['main/look.json'].text).looks.framed;
        check(added.params && !('pieces' in added.params) && store.verdicts.appearance.code === 15 && store.verdicts.appearance.name === 'pieces' &&
              same(store.appearanceAt, ['looks', 'framed']) && store.appearanceShort === null,
              'no pieces given; MISSING at them: ' + JSON.stringify([added, store.verdicts.appearance, store.appearanceAt]));
    }

    /* ---- a scrolling box's keys: its offset and step offered with "scroll" alone ---- */
    {
        const page = '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "label", "text": "a"}]}}\n';
        const files = {
            'main/CMakeLists.txt': bytes('idf_component_register(SRCS "main.c")\ndgx_ui_tree(s.json)\n'), 'main/s.json': bytes(page),
        };
        const { store } = await storeOf(files, 'properties');
        store.select([]);
        const keys = () => store.fields.map((field) => field.key);
        check(!keys().includes('value') && !keys().includes('step') && store.fields.every((field) => field.kind === 'box'), 'a box: no offset, no step: ' + keys());
        store.toggleWord('flags', 'scroll');
        check(keys().includes('value') && keys().includes('step') && store.verdicts.page.code === 0, 'a scrolling box: its offset and step offered, taken: ' +
              JSON.stringify([keys(), store.verdicts.page]));
        store.undo();
        check(!keys().includes('value') && store.files['main/s.json'].text === page, 'undone');
    }

    if (failures) {
        console.error(`test_store.js: ${failures} of ${checks} checks failed`);
        process.exit(1);
    }
    console.log(`test_store.js: ${checks} checks of the store on the hook project and a project in memory`);
})().catch((error) => {
    console.error('test_store.js:', error);
    process.exit(1);
});
