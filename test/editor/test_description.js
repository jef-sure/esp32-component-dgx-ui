/*
 * node test_description.js <dgx_ui_preview.js> <directory of descriptions>: editor/description.js and editor/history.js
 * of the builder, held to the loader of the module. On every description of the directory (test_generate's) and on
 * them spoiled at a byte:
 *  - the builder's reader takes a text exactly when the loader's reader of JSON does (preview_json()), its bytes
 *    decoded as UTF-8 that refuses what is not well-formed, as the loader refuses it;
 *  - a text written by the builder keeps every string and number as it was written, escapes and forms; read and written
 *    again, it is the same byte for byte; its verdict is the original's;
 *  - each widget's span in the text written holds its object;
 *  - the widgets in the builder's pre-order are the module's, by index, kind and text;
 *  - operations on random pages the loader takes leave pages it takes, and give paths that are widgets;
 *  - and the history against a list kept plainly.
 */
'use strict';
const fs = require('fs');
const path = require('path');

const DgxUiPreview = require(path.resolve(process.argv[2]));
const description = require(path.join(__dirname, '../../editor/description.js'));
const { History } = require(path.join(__dirname, '../../editor/history.js'));
const directory = process.argv[3];
let failures = 0;
const check = (held, what) => {
    if (!held) console.error('  failed: ' + what), ++failures;
    return held;
};
let chance = 4848;
const random = (n) => ((chance = (chance * 1103515245 + 12345) >>> 0) >>> 8) % n;

(async () => {
    const preview = await DgxUiPreview.create({ log: () => {} });
    const json = (bytes) => preview.json(bytes);
    /* the page's bytes as they are: the texts held to the loader read as UTF-8, the same bytes */
    const verdict = (bytes) => preview.page(Buffer.from(bytes).toString('utf8'));
    const format = preview.format();
    const holding = format.keys.find((key) => key.key === 'children').kinds;
    preview.screen(96, 64, 16, 1), preview.names(null), preview.appearance(null);

    const names = fs.readdirSync(directory).filter((name) => name.endsWith('.json')).sort();
    const texts = names.map((name) => fs.readFileSync(path.join(directory, name)));
    check(texts.length > 100, 'the descriptions: ' + texts.length);

    /* the reader against the loader's reader of JSON, on the texts and on them spoiled at a byte */
    let read = 0, refused = 0, deep = 0;
    const holdReader = (bytes, what) => {
        const loader = json(bytes), ours = description.read(bytes);
        if (loader === 2) return ++deep;
        const said = `${what}: the builder's reader ${ours.ok ? 'took' : 'refused (' + ours.why + ')'}, the loader's ${loader ? 'took' : 'refused'}`;
        if (!check(ours.ok === (loader === 1), said))
            return;
        ours.ok ? ++read : ++refused;
    };
    const spoilers = [0x22, 0x7b, 0x7d, 0x5b, 0x5d, 0x2c, 0x3a, 0x30, 0x20, 0x5c, 0x00, 0x0a, 0x2d, 0x2e, 0x65, 0xc3, 0xff, 0xed];
    texts.forEach((bytes, k) => {
        holdReader(bytes, `text ${k}`);
        const step = k < 30 ? 1 : 7; // every byte of the first thirty, every seventh of the rest
        for (let at = 0; at < bytes.length; at += step) {
            const spoiled = Buffer.from(bytes);
            spoiled[at] = spoilers[random(spoilers.length)];
            holdReader(spoiled, `text ${k} spoiled at ${at}`);
        }
    });
    check(read > 1000 && refused > 1000, `both kinds seen: ${read} read, ${refused} refused`);

    /* written, read and written again: the same; the verdict the original's; the spans */
    let written = 0, taken = [];
    texts.forEach((bytes, k) => {
        const ours = description.read(bytes);
        if (!ours.ok) return;
        const first = description.write(ours.doc), again = description.read(Buffer.from(first.text));
        if (!check(again.ok, `text ${k}: its writing not read again`)) return;
        check(description.write(again.doc).text === first.text, `text ${k}: written twice, not the same`);
        const before = verdict(bytes), after = verdict(Buffer.from(first.text));
        check(before.code === after.code && (before.name === after.name || before.code === 3),
              `text ${k}: the verdict ${before.code} "${before.name}" became ${after.code} "${after.name}"`);
        /* every string and number as it was written, its escapes and its form: nothing of it edited */
        const raws = [], walk = [ours.doc];
        while (walk.length) {
            const node = walk.pop();
            if (node.t === 'object') for (const [key, value] of node.entries) raws.push(key.raw), walk.push(value);
            else if (node.t === 'array') walk.push(...node.items);
            else if (node.raw !== undefined) raws.push(node.raw);
        }
        const missing = raws.filter((raw) => !first.text.includes(raw));
        check(!missing.length, `text ${k}: written otherwise than it was: ${missing.slice(0, 3).join(' ')}`);
        const textBytes = Buffer.from(first.text);
        for (const one of description.widgets(ours.doc)) {
            const span = first.spans.find((s) => description.samePath(s.path, one.path));
            if (!check(span, `text ${k}: no span of ${JSON.stringify(one.path)}`)) continue;
            const inside = description.read(textBytes.subarray(span.start, span.end));
            const kind = description.shown(description.valueOf(one.widget, 'kind'));
            check(inside.ok && description.shown(description.valueOf(inside.doc, 'kind')) === kind,
                  `text ${k}: the span of ${JSON.stringify(one.path)}`);
        }
        ++written;
        if (before.code === 0) taken.push(ours.doc);
    });
    check(written > 100 && taken.length > 50, `written ${written}, taken ${taken.length}`);

    /* the builder's pre-order against the module's widgets */
    for (const doc of taken) {
        check(verdict(Buffer.from(description.write(doc).text)).code === 0, 'a page taken');
        const ours = description.widgets(doc);
        if (!check(ours.length === preview.count(), `${ours.length} widgets, the module's ${preview.count()}`)) continue;
        ours.forEach((one, i) => {
            const theirs = preview.widget(i);
            check(format.kinds[theirs.kind] === description.shown(description.valueOf(one.widget, 'kind')), `widget ${i}: its kind`);
            const text = description.shown(description.valueOf(one.widget, 'text'));
            if (typeof text === 'string') check(theirs.text === text, `widget ${i}: its text "${theirs.text}", "${text}"`);
            check(description.indexOf(doc, one.path) === i && description.samePath(description.pathOf(doc, i), one.path), `widget ${i}: its path`);
        });
    }

    /* operations on pages taken: pages taken, and paths of widgets */
    let operations = 0;
    for (const original of taken.slice(0, 60)) {
        let doc = description.read(Buffer.from(description.write(original).text)).doc;
        for (let round = 0; round < 24; ++round) {
            const all = description.widgets(doc), one = all[random(all.length)];
            const kind = description.shown(description.valueOf(one.widget, 'kind'));
            const choice = random(4);
            let selected;
            if (choice === 0 && holding.includes(kind)) {
                const added = format.kinds[random(format.kinds.length)];
                const tables = preview.tables();
                const ownLook = tables.look.includes(added) || ['box', 'frame', 'label'].includes(added);
                selected = description.addChild(doc, one.path, added, ownLook ? {} : { look: null });
            } else if (choice === 1 && one.path.length) {
                selected = description.remove(doc, one.path);
            } else if (choice === 2 && one.path.length) {
                selected = description.move(doc, one.path, random(2) ? 1 : -1) ?? one.path;
            } else if (choice === 3) {
                selected = description.wrap(doc, one.path);
            } else {
                continue;
            }
            const text = description.write(doc).text;
            const said = verdict(Buffer.from(text));
            check(said.code === 0, `an operation ${choice} left a page refused: ${said.code} at ${said.offset} "${said.name}"\n${text}`);
            check(selected && description.widgetAt(doc, selected), `an operation ${choice} selected no widget: ${JSON.stringify(selected)}`);
            ++operations;
            if (said.code) break;
        }
    }
    check(operations > 300, 'operations: ' + operations);

    /* a number not finite is refused by a throw, never written as null */
    {
        const doc = description.read(Buffer.from('{"dgx-ui": 1, "tree": {"kind": "box", "pad": 2}}')).doc;
        for (const bad of [NaN, Infinity, -Infinity]) {
            let said = '';
            try {
                description.setKey(doc, [], 'pad', bad);
            } catch (error) {
                said = error.message;
            }
            check(said.includes('not finite'), `${bad} not refused: "${said}"`);
            check(description.write(doc).text.includes('"pad": 2'), `${bad} changed the document`);
            let nested = '';
            try {
                description.setKey(doc, [], 'want', [0, bad]);
            } catch (error) {
                nested = error.message;
            }
            check(nested.includes('not finite') && description.write(doc).text.includes('"pad": 2') && !description.write(doc).text.includes('want'),
                  `${bad} within a list not refused, or the document changed`);
        }
    }

    /* positions: units and bytes, and the first byte that is not UTF-8, against TextEncoder and TextDecoder */
    let positions = 0;
    const pieces = ['a', 'é', '€', '😀', '\ud800', '\udc00', '"', '\n', 'ж', '\u0000'];
    for (let round = 0; round < 3000; ++round) {
        let text = '';
        for (let n = random(12); n--;) text += pieces[random(pieces.length)];
        const encoded = Buffer.from(new TextEncoder().encode(text));
        for (let unit = 0; unit <= text.length; ++unit) {
            const low = (text.charCodeAt(unit - 1) & 0xfc00) === 0xd800 && (text.charCodeAt(unit) & 0xfc00) === 0xdc00; // within a pair
            if (low) continue;
            const byte = description.byteOfUnit(text, unit);
            check(byte === new TextEncoder().encode(text.slice(0, unit)).length, `byteOfUnit(${JSON.stringify(text)}, ${unit}): ${byte}`);
            check(description.unitOfByte(text, byte) === unit, `unitOfByte(${JSON.stringify(text)}, ${byte}) is not ${unit}`);
            ++positions;
        }
        check(description.unitOfByte(text, encoded.length + 5) === text.length, 'a byte past the end: the end');
    }
    const fatal = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true });
    const decodes = (bytes) => { try { fatal.decode(bytes); return true; } catch { return false; } };
    for (let round = 0; round < 20000; ++round) {
        const bytes = new Uint8Array(random(10));
        for (let k = 0; k < bytes.length; ++k) bytes[k] = [random(256), 0x80 + random(64), 0xc2 + random(51), 0x41][random(4)];
        const bad = description.illFormedAt(bytes);
        if (bad < 0) {
            check(decodes(bytes), `illFormedAt says none in ${Buffer.from(bytes).toString('hex')}, TextDecoder refuses it`);
        } else {
            /* well-formed before it, and the first sequence TextDecoder replaces starts there */
            const lossy = new TextDecoder('utf-8', { ignoreBOM: true }).decode(bytes);
            check(decodes(bytes.subarray(0, bad)) && !decodes(bytes) &&
                  lossy.indexOf('\ufffd') === new TextDecoder('utf-8', { ignoreBOM: true }).decode(bytes.subarray(0, bad)).length,
                  `illFormedAt ${bad} in ${Buffer.from(bytes).toString('hex')}`);
            const said = description.read(bytes);
            check(!said.ok && said.byte === bad && said.why === 'not UTF-8',
                  `read() of ${Buffer.from(bytes).toString('hex')}: ${JSON.stringify(said)}`);
        }
        ++positions;
    }
    {
        const said = description.read(Buffer.from('{"dgx-ui": 1, "tree": {"text": "Café", ]}'));
        check(!said.ok && said.at === 39 && said.byte === 40, 'read(): a refusal past an é, at a unit and a byte: ' + JSON.stringify(said));
    }

    /* the history against a list kept plainly */
    for (let round = 0; round < 200; ++round) {
        const history = new History(20);
        const plain = { past: [], future: [] };
        let now = 0, state = 0;
        for (let step = 0; step < 60; ++step) {
            const act = random(4);
            if (act < 2) {
                const fold = random(3) === 0 ? null : `field${random(2)}`;
                const folded = fold !== null && plain.fold === fold && now - plain.at < 1000;
                if (!folded) {
                    plain.past.push(state);
                    if (plain.past.length > 20) plain.past.shift();
                    plain.future = [];
                }
                plain.fold = fold, plain.at = folded ? now : now;
                if (fold === null) plain.fold = null;
                history.record(state, fold, now);
                state = step + 1000;
            } else if (act === 2) {
                const back = history.undo(state);
                const plainBack = plain.past.length ? (plain.future.push(state), plain.past.pop()) : null;
                plain.fold = null;
                check(back === plainBack, `undo ${back}, the plain list's ${plainBack}`);
                if (back !== null) state = back;
            } else {
                const ahead = history.redo(state);
                const plainAhead = plain.future.length ? (plain.past.push(state), plain.future.pop()) : null;
                plain.fold = null;
                check(ahead === plainAhead, `redo ${ahead}, the plain list's ${plainAhead}`);
                if (ahead !== null) state = ahead;
            }
            now += random(3) * 600;
        }
    }

    if (failures) {
        console.error(`test_description.js: ${failures} check(s) failed`);
        process.exit(1);
    }
    console.log(`test_description.js: ${read} texts read and ${refused} refused as the loader's reader does (${deep} deeper than it goes), ` +
                `${written} written twice the same, ${taken.length} in the module's order, ${operations} operations taken, ${positions} positions`);
})().catch((error) => {
    console.error('test_description.js:', error);
    process.exit(1);
});
