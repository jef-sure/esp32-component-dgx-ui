/*
 * node test_class.js <dgx_ui_preview.js>: DgxUiPreview's own calls, as a page makes them: the version, the panel as the
 * module takes it and none after a refusal, the verdicts, the frame and its ImageData (a stand-in of the page's), the
 * page on the screen and its widgets, the events and notes as lines, a value, the keys, a page opened and closed, a
 * pageText that throws, fonts given as files one by one and by an index with the refusals of its lines, the module's
 * memory as it was after a thousand files given anew and let go, a character of the log split between two writes, and
 * the preview stopped by a fault. The files are DGX's fonts in the directory given, with dgx_fonts-files.txt.
 */
'use strict';
const path = require('path');

const fs           = require('fs');
const DgxUiPreview = require(path.resolve(process.argv[2]));
const fontFiles    = process.argv[3];
let failures       = 0;
const check        = (held, what) => {
    if (!held) console.error('  failed: ' + what), ++failures;
};

/* the page's ImageData, as a page has it */
global.ImageData = class {
    constructor(data, width, height) {
        if (data.length !== width * height * 4) throw new Error('ImageData: ' + data.length + ' bytes for ' + width + 'x' + height);
        this.data = data, this.width = width, this.height = height;
    }
};

const pages = {settings: '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "label", "text": "Settings"}, ' +
                         '{"kind": "button", "text": "Back", "on": "back", "want": [0, 16]}]}}'};
const home  = '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [' +
              '{"kind": "button", "text": "More", "on": "page", "ctx": "settings", "want": [0, 16]},' +
              '{"kind": "button", "text": "Lost", "on": "page", "ctx": "broken", "want": [0, 16]},' +
              '{"kind": "slider", "value": "volume", "min": 0, "max": 100, "want": [0, 10]}]}}';

(async () => {
    const logged  = [];
    const preview = await DgxUiPreview.create({
        pageText: name => {
            if (name === 'broken') throw new Error('no such page');
            return pages[name] === undefined ? null : pages[name];
        },
        log: line => logged.push(line),
    });
    let now = 1000;
    const click = i => {
        const widget = preview.widget(i);
        preview.pointer(widget.x + 2, widget.y + 2, true, now), preview.paint(now), now += 50;
        preview.pointer(widget.x + 2, widget.y + 2, false, now), preview.paint(now), now += 50;
    };

    const built = preview.version();
    check(/^\d+\.\d+\.\d+$/.test(built.dgx_ui) && /^\d+\.\d+\.\d+ of the registry/.test(built.dgx), 'the versions');
    check(/^clang /.test(built.toolchain) && /^sha256 [0-9a-f]{64}$/.test(built.sources), 'the toolchain and the hash');

    /* the panel as the module takes it */
    check(preview.screen(96.9, 64.9, 16, 1) === 0 && preview.width === 96 && preview.height === 64, 'a size taken as integers');
    check(preview.shown() === null && preview.count() === 0 && preview.widget(0) === null, 'no page');
    check(preview.names(null).code === 0 && preview.appearance(null).code === 0, 'no names file, no appearance');
    const taken = preview.page(home);
    check(taken.code === 0 && taken.offset === 0 && taken.name === '' && taken.said === '', 'the page taken');
    check(preview.paint(now) === true && preview.paint(now + 1) === false, 'painted once');
    const frame = preview.frame();
    check(frame instanceof Uint8ClampedArray && frame.length === 96 * 64 * 4 && frame[3] === 255, 'the frame');
    const image = preview.image();
    check(image instanceof ImageData && image.width === 96 && image.height === 64 && image.data[0] === frame[0] && image.data !== frame, 'the image');
    const changed = preview.changed();
    check(changed.x === 0 && changed.y === 0 && changed.w === 96 && changed.h === 64 && preview.changed().w === 0, 'the rectangle written');

    /* the widgets, the page on the screen */
    check(preview.shown() === '' && preview.count() === 4, 'the page given on the screen');
    const more = preview.widget(1);
    check(more.kind === 8 && more.text === 'More' && more.parent === 0 && more.shown === true && more.w === 96 && more.h === 16, 'a widget');
    check(preview.widgetAt(more.x + 1, more.y + 1) === 1 && preview.widgetAt(-1, -1) === -1, 'the widget under a finger');
    check(preview.value('volume', 70) === true && preview.value('none', 1) === false && preview.widget(3).value === 70, 'a value');
    preview.events();
    click(1);
    check(preview.events().join('|') === 'told 1 FOCUS|told 1 ACTIVATED', 'the events as lines');
    check(preview.shown() === 'settings' && preview.count() === 3 && preview.widget(1).text === 'Settings', 'the page opened, its widgets');
    check(preview.notes().some(line => line.startsWith('page settings: style')), 'its notes led by its name');
    preview.key(0), preview.key(2), preview.paint(now);
    check(preview.events().join('|') === 'told 2 FOCUS page settings|told 2 ACTIVATED page settings' && preview.shown() === '', 'back by the keys');
    preview.buttons(0, now);

    /* a pageText that throws: no text for the module, the error to the log */
    click(2);
    check(preview.events().includes('unopened broken no-text'), 'a page with no text');
    check(logged.includes('the text of the page "broken": no such page'), 'the error to the log: ' + logged.join('|'));

    /* the verdicts */
    const refused = preview.names('value v &v\nvalues w &w\n');
    check(refused.code === -1 && refused.said.startsWith('names:2: no table "values"'), 'a names file refused');
    preview.names(null);
    const missing = preview.appearance('{"dgx-ui": 1, "colors": {"paper": "#102030"}}');
    check(missing.code === 15 && missing.name === 'ink', 'an appearance refused: ' + JSON.stringify(missing));
    preview.appearance(null);

    /* an appearance on a names file of its own, by the rule with null, on the page's with none given */
    const framedLook = '{"dgx-ui": 1, "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#204060", "border": "#808080"}, ' +
                       '"looks": {"pane": {"draw": "frame", "fill": "fill", "params": {"border": "border", "width": 1}}}}';
    const framed = '{"dgx-ui": 1, "tree": {"kind": "frame", "look": "pane", "children": [{"kind": "slider", "value": "volume"}]}}';
    check(preview.names('look slider\nvalue volume &volume\n').code === 0, 'the page\'s names file');
    check(preview.appearance(framedLook).code === 8 && preview.page(framed).code === 8, 'on the page\'s, which has not its draw: refused');
    check(preview.appearance(framedLook, 'draw frame\n').code === 0 && preview.page(framed).code === 0, 'on its own: taken');
    check(preview.appearance(framedLook, null).code === 0 && preview.page(framed).code === 0, 'by the rule: taken');
    const ownRefused = preview.appearance(framedLook, 'what is this\n');
    check(ownRefused.code === -1 && ownRefused.said.includes("appearance's names"), 'its own refused, in words: ' + JSON.stringify(ownRefused));
    check(preview.appearance(framedLook).code === 8, 'none given: the page\'s again');
    preview.appearance(null), preview.page(null), preview.names(null);
    const broken = preview.page('{"dgx-ui": 1, "tree": {"kind": "box", "pad": 300}}');
    check(broken.code === 6 && broken.offset === 45 && preview.count() === 0 && preview.shown() === null,
          'a page refused: ' + JSON.stringify(broken));
    check(preview.page(null).code === 0 && preview.count() === 0 && preview.paint(now) === false, 'no page');
    check(preview.wake(now) === Infinity, 'nothing asks for time');

    /* fonts as files: one by one, then by an index */
    const terminus = fs.readFileSync(path.join(fontFiles, 'TerminusTTFMedium12.dgxf'));
    check(preview.names('font f TerminusTTFMedium12\n').code === 0, 'a names file of a font');
    check(preview.font('TerminusTTFMedium12', terminus).code === 0, 'a font file');
    const spoiled = Uint8Array.from(terminus.subarray(0, 40));
    const cut     = preview.font('TerminusTTFMedium12', spoiled);
    check(cut.code === -3 && cut.said.startsWith('TerminusTTFMedium12: ESP_ERR_'), 'a file cut: ' + JSON.stringify(cut));
    check(preview.font('n'.repeat(64), terminus).code === -3 && preview.font('TerminusTTFMedium12', 'not bytes').code === -3, 'refused');
    const nul = preview.font('TerminusTTFMedium12\u0000tail', terminus);
    check(nul.code === -3 && nul.said === 'a name of C holding a NUL', 'a name holding a NUL refused, not cut');
    check(preview.font('TerminusTTFMedium12', terminus.buffer.slice(terminus.byteOffset, terminus.byteOffset + terminus.length)).code === 0,
          'a file of an ArrayBuffer');
    check(preview.font('TerminusTTFMedium12', null).code === 0, 'let go');
    const index = fs.readFileSync(path.join(fontFiles, 'dgx_fonts-files.txt'), 'utf8');
    const read  = file => fs.promises.readFile(path.join(fontFiles, file));
    const all   = await preview.fonts(index, read);
    check(all.length === index.trim().split('\n').length && all.every(given => given.verdict.code === 0), 'every font of the index');
    const odd = 'one two three\nTerminusTTFMedium12 TerminusTTFMedium12.dgxf\nTerminusTTFMedium12 TerminusTTFMedium12.dgxf\n' +
                'a /etc/passwd\nb ../up.dgxf\nc missing.dgxf\nd empty.dgxf\n';
    const lines = await preview.fonts(odd, file => file === 'empty.dgxf' ? null : read(file));
    check(lines.length === 7 && lines[1].verdict.code === 0, 'an index of odd lines: ' + lines.length);
    check(lines[0].verdict.said.startsWith('not a line of a name and a file') && lines[2].verdict.said.includes('given twice'), 'its words, 1');
    check(lines[3].verdict.said.includes('not of the index') && lines[4].verdict.said.includes('not of the index'), 'its words, 2');
    check(lines[5].verdict.code === -3 && lines[5].verdict.said.startsWith('c: missing.dgxf: ENOENT'), 'its words, 3: ' + lines[5].verdict.said);
    check(lines[6].verdict.said === 'd: empty.dgxf: no bytes', 'its words, 4');
    check(await preview.fonts(index, file => fs.readFileSync(path.join(fontFiles, file))).then(given => given.every(one => one.verdict.code === 0)),
          'a reader of bytes, not of a Promise');

    /* the module's memory: a thousand files given anew under one name, a thousand let go and given again */
    preview.page(home), preview.paint(now);
    preview.font('TerminusTTFMedium12', terminus);
    const before = preview.memory.buffer.byteLength;
    for (let round = 0; round < 1000; ++round) preview.font('TerminusTTFMedium12', terminus);
    for (let round = 0; round < 1000; ++round) preview.font('TerminusTTFMedium12', null), preview.font('TerminusTTFMedium12', terminus);
    check(preview.memory.buffer.byteLength === before, `the memory grew: ${before} to ${preview.memory.buffer.byteLength}`);

    /* a screen refused leaves none */
    check(preview.screen(10, 10, 12, 1) === -2 && preview.width === 0 && preview.frame() === null && preview.image() === null, 'a screen refused');
    check(preview.paint(now) === false, 'nothing drawn with no screen');

    /* a character of the log split between two writes */
    const wasi = preview.imports().wasi_snapshot_preview1, at = preview.exports.preview_text(64);
    const memory = () => new DataView(preview.memory.buffer);
    const write  = (bytes) => {
        new Uint8Array(preview.memory.buffer).set(bytes, at);
        memory().setUint32(at + 16, at, true), memory().setUint32(at + 20, bytes.length, true);
        wasi.fd_write(1, at + 16, 1, at + 24);
    };
    write([0x41, 0xc3]), write([0xa9, 0x0a]);
    check(logged.includes('Aé'), 'a split character joined: ' + JSON.stringify(logged.slice(-1)));

    /* a fault stops the preview */
    let said = '';
    try {
        preview.call(() => preview.exports.preview_text(0xffffffff)); /* no memory for it: the module ends */
    } catch (error) {
        said = error.message;
    }
    check(said.startsWith('the preview stopped'), 'a fault: ' + said);
    try {
        preview.count(), said = '';
    } catch (error) {
        said = error.message;
    }
    check(said.startsWith('the preview stopped'), 'stopped for good');
    const again = await DgxUiPreview.create({log: () => {}});
    check(again.screen(8, 8, 16, 1) === 0, 'a new preview');

    if (failures) {
        console.error(`test_class.js: ${failures} check(s) failed`);
        process.exit(1);
    }
    console.log('test_class.js: ok');
})().catch(error => {
    console.error('test_class.js:', error);
    process.exit(1);
});
