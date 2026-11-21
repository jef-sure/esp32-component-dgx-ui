/*
 * node scene.js <dgx_ui_preview.js> <scene file> <out file>: a scene played on the module built to WebAssembly, as
 * scene.c plays it on the preview built for the computer, written in the same form byte for byte (scene.c says the
 * form). Texts go in and come out as bytes, not as JavaScript's strings, so that a text not well-formed UTF-8 is held
 * equal too.
 */
'use strict';
const fs   = require('fs');
const path = require('path');

const [moduleFile, sceneFile, outFile] = process.argv.slice(2);
const DgxUiPreview = require(path.resolve(moduleFile));
const directory    = path.dirname(sceneFile);
const pages        = new Map();
const out          = [];
const say          = text => out.push(Buffer.from(text, 'latin1'));

(async () => {
    const preview = await DgxUiPreview.create({log: () => {}});
    const exports = preview.exports;
    const memory  = () => new Uint8Array(preview.memory.buffer);
    /* the pages' texts as bytes */
    preview.pageTextOf = (name, at, size) => {
        const text = pages.get(rawString(name).toString('latin1'));
        if (!text) return 0;
        if (at && size >= text.length) memory().set(text, at);
        return text.length;
    };
    const rawString = (at, most = Infinity) => {
        if (!at) return Buffer.alloc(0);
        const bytes = memory();
        let end     = at;
        while (end - at < most && bytes[end]) ++end;
        return Buffer.from(bytes.subarray(at, end));
    };
    const withBytes = (bytes, run) => {
        if (bytes === null) return run(0, 0);
        const at = exports.preview_text(bytes.length + 1);
        memory().set(bytes, at), memory()[at + bytes.length] = 0;
        return run(at, bytes.length);
    };
    const verdict = (what, at) => {
        const view = new DataView(preview.memory.buffer);
        out.push(Buffer.concat([Buffer.from(`${what} ${view.getInt32(at, true)} ${view.getInt32(at + 4, true)} [`, 'latin1'), rawString(at + 8, 24),
                                Buffer.from('] [', 'latin1'), rawString(at + 32, 512), Buffer.from(']\n', 'latin1')]));
    };
    const lines = (lead, at) => {
        const text = rawString(at);
        let start  = 0;
        while (start < text.length) {
            let end = text.indexOf(10, start);
            if (end < 0) end = text.length;
            out.push(Buffer.concat([Buffer.from(lead + ' ', 'latin1'), text.subarray(start, end), Buffer.from('\n', 'latin1')]));
            start = end + 1;
        }
    };
    let width = 0, height = 0;
    for (const line of fs.readFileSync(sceneFile, 'latin1').split('\n')) {
        const words = line.trim().split(/\s+/);
        if (!words[0] || words[0].startsWith('#')) continue;
        const number = k => parseInt(words[k], 10) | 0;
        switch (words[0]) {
        case 'screen': {
            const said = exports.preview_screen(number(1), number(2), number(3), number(4));
            if (said === 0) width = number(1), height = number(2);
            say(`screen ${said}\n`);
            break;
        }
        case 'names':
        case 'appearance':
        case 'page': {
            const bytes = words[1] === '-' ? null : fs.readFileSync(path.join(directory, words[1]));
            const call  = {names: exports.preview_names, appearance: exports.preview_appearance, page: exports.preview_page}[words[0]];
            verdict(words[0], withBytes(bytes, (at, length) => call(at, length)));
            break;
        }
        case 'pagefile':
            pages.set(words[1], fs.readFileSync(path.join(directory, words[2])));
            break;
        case 'paint':
            say(`paint ${exports.preview_paint(number(1) >>> 0)}\n`);
            say(`wake ${exports.preview_wake(number(1) >>> 0) >>> 0}\n`);
            break;
        case 'pointer':
            exports.preview_pointer(number(1), number(2), number(3), number(4) >>> 0);
            break;
        case 'key':
            exports.preview_key(number(1));
            break;
        case 'buttons':
            exports.preview_buttons(number(1) >>> 0, number(2) >>> 0);
            break;
        case 'value':
            say(`value ${withBytes(Buffer.from(words[1], 'latin1'), at => exports.preview_value(at, number(2)))}\n`);
            break;
        case 'dump': {
            const shown = exports.preview_shown();
            out.push(Buffer.concat([Buffer.from(`shown ${shown ? '' : 'none '}[`, 'latin1'), rawString(shown), Buffer.from(']\n', 'latin1')]));
            const count = exports.preview_count();
            say(`count ${count}\n`);
            for (let i = 0; i < count; ++i) {
                const at = exports.preview_widget(i), view = new DataView(preview.memory.buffer), field = k => view.getInt32(at + 4 * k, true);
                const head  = `widget ${i} ${field(0)} ${field(1)} ${field(2)} ${field(3)} ${field(4)} ${field(5)} ${field(6)} ` +
                              `${view.getUint32(at + 28, true)} ${field(8)} [`;
                out.push(Buffer.concat([Buffer.from(head, 'latin1'), rawString(view.getUint32(at + 36, true)), Buffer.from(']\n', 'latin1')]));
            }
            for (let k = 0; k < 4; ++k) {
                const x = Math.trunc((k + 1) * width / 5), y = Math.trunc((k + 1) * height / 5);
                say(`at ${exports.preview_widget_at(x, y)}\n`);
            }
            lines('event', exports.preview_events());
            lines('note', exports.preview_notes());
            const changed = exports.preview_changed(), view = new DataView(preview.memory.buffer), side = k => view.getInt32(changed + 4 * k, true);
            say(`changed ${side(0)} ${side(1)} ${side(2)} ${side(3)}\n`);
            const frame = exports.preview_frame();
            say(`frame ${frame ? width * height * 4 : 0}\n`);
            if (frame) out.push(Buffer.from(memory().subarray(frame, frame + width * height * 4)));
            say('\n');
            break;
        }
        default:
            throw new Error('not understood: ' + line);
        }
    }
    fs.writeFileSync(outFile, Buffer.concat(out));
})().catch(error => {
    console.error('scene.js:', error.message);
    process.exit(1);
});
