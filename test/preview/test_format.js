/*
 * node test_format.js <dgx_ui_preview.js>: preview_format(), the format as the module describes it from the loader's
 * tables, held to the loader of the same module. For every key in every kind it means something for, values at the
 * edges of its type are taken and values past them refused; in every kind it means nothing for, the key is refused as
 * a field of no meaning. A page that edits descriptions draws its controls by this description: the test shows that
 * it never offers what the loader refuses, nor hides what it takes.
 */
'use strict';
const path = require('path');

const DgxUiPreview = require(path.resolve(process.argv[2]));
const OK = 0, SYNTAX = 1, KIND = 4, FIELD = 5, RANGE = 6, NAME_BAD = 7, UNKNOWN = 8;
let failures = 0, held = 0;

(async () => {
    const preview = await DgxUiPreview.create({log: () => {}});
    const format  = preview.format();
    preview.screen(64, 48, 16, 1), preview.names(null), preview.appearance(null);
    const page = widget => preview.page(JSON.stringify({'dgx-ui': 1, tree: widget}));
    const expect = (widget, codes, what) => {
        const said = page(widget);
        if (!codes.includes(said.code)) {
            console.error(`  ${what}: ${JSON.stringify(widget)} gave ${said.code} at ${said.offset} "${said.name}", not ${codes}`);
            ++failures;
        } else {
            ++held;
        }
        return said;
    };
    /* a widget of a kind and a key, with what the key needs beside it: a text for a style or lines, a handler taking a
       number for a ctx of a number; every look null, so that no kind wants a look of the library's */
    const of = (kind, key, value) => {
        const widget = {kind, look: null};
        if (key === 'style' || key === 'lines') widget.text = 'x';
        if (key === 'ctx' && typeof value === 'number') widget.on = 'go';
        widget[key] = value;
        return widget;
    };
    const kinds = format.kinds;
    const byKey = Object.fromEntries(format.keys.map(entry => [entry.key, entry]));
    if (kinds.length !== 13 || format.keys.length !== 24 || format.version !== 1) console.error('  the format\'s shape'), ++failures;

    for (const entry of format.keys) {
        for (const kind of entry.kinds) {
            const at = `${entry.key} of ${kind}`;
            switch (entry.type) {
            case 'kind':
                for (const word of kinds) expect({kind: word, look: null}, [OK], at);
                expect({kind: 'nothing', look: null}, [KIND], at);
                break;
            case 'words':
                for (const word of entry.words) expect(of(kind, entry.key, [word]), [OK], at);
                expect(of(kind, entry.key, entry.words), [OK], at), expect(of(kind, entry.key, []), [OK], at);
                expect(of(kind, entry.key, ['nothing']), [UNKNOWN], at);
                break;
            case 'int':
                expect(of(kind, entry.key, entry.min), [OK], at), expect(of(kind, entry.key, entry.max), [OK], at);
                expect(of(kind, entry.key, entry.min - 1), [RANGE], at), expect(of(kind, entry.key, entry.max + 1), [RANGE], at);
                expect(of(kind, entry.key, 'x'), [SYNTAX], at);
                break;
            case 'pair':
                expect(of(kind, entry.key, [entry.min, entry.max]), [OK], at), expect(of(kind, entry.key, [entry.max, entry.min]), [OK], at);
                expect(of(kind, entry.key, [entry.min - 1, 0]), [RANGE], at), expect(of(kind, entry.key, [0, entry.max + 1]), [RANGE], at);
                expect(of(kind, entry.key, [1]), [SYNTAX], at);
                break;
            case 'name':
                expect(of(kind, entry.key, 'a_name'), [OK], at);
                expect(of(kind, entry.key, 'a name'), [NAME_BAD], at);
                expect(of(kind, entry.key, null), entry.null ? [OK] : [SYNTAX], at);
                if (entry.min !== undefined) {
                    expect(of(kind, entry.key, entry.min), [OK], at), expect(of(kind, entry.key, entry.max), [OK], at);
                    expect(of(kind, entry.key, entry.max + 1), [RANGE], at);
                    expect({kind, look: null, ctx: 1}, [FIELD], at + ', a number with no handler that takes one');
                } else {
                    expect(of(kind, entry.key, 1), [SYNTAX], at);
                }
                break;
            case 'text':
                expect(of(kind, entry.key, 'a text'), [OK], at), expect(of(kind, entry.key, 5), [SYNTAX], at);
                break;
            case 'id':
                expect(of(kind, entry.key, 'an_id'), [OK], at), expect(of(kind, entry.key, 'an id'), [NAME_BAD], at);
                break;
            case 'children':
                expect(of(kind, entry.key, []), [OK], at), expect(of(kind, entry.key, [{kind: 'box'}]), [OK], at);
                expect(of(kind, entry.key, {}), [SYNTAX], at);
                break;
            case 'editor':
                expect(of(kind, entry.key, {}), [OK], at), expect(of(kind, entry.key, {any: [1e3, 2.5]}), [OK], at);
                expect(of(kind, entry.key, []), [SYNTAX], at);
                break;
            default:
                console.error(`  ${entry.key}: a type the test does not know, ${entry.type}`), ++failures;
            }
        }
        /* in a kind it means nothing for: refused as a field of no meaning, by its name or by the one it needs beside it */
        const value = {kind: 'box', words: [], int: entry.min ?? 0, pair: [0, 0], name: 'a_name', text: 'x', id: 'an_id', children: [],
                       editor: {}}[entry.type];
        for (const kind of kinds.filter(kind => !entry.kinds.includes(kind))) {
            const said = expect(of(kind, entry.key, value), [FIELD], `${entry.key} of ${kind}, of no meaning`);
            if (said.code === FIELD && said.name !== entry.key && said.name !== 'text')
                console.error(`  ${entry.key} of ${kind}: refused by "${said.name}"`), ++failures;
        }
    }
    if (!byKey.children || byKey.children.kinds.join() !== 'box,frame,own') console.error('  the kinds that hold children'), ++failures;
    if (failures) {
        console.error(`test_format.js: ${failures} of ${held + failures} apart from the loader`);
        process.exit(1);
    }
    console.log(`test_format.js: ${held} values of the format's ${format.keys.length} keys as the loader takes them`);
})().catch(error => {
    console.error('test_format.js:', error);
    process.exit(1);
});
