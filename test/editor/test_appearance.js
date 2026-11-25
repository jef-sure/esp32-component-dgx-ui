/*
 * node test_appearance.js <dgx_ui_preview.js>: the appearance as the builder edits it (editor/appearance.js), held to
 * the loader of the preview's module.
 *  - what the module says an appearance may say (preview_appearance_format()): every draw of the library with the
 *    values of its look taken as they are; each field left out refused where it must be said, at its bounds taken and
 *    past them refused, each word taken; and the same fields as the table of docs/en/api.md says them;
 *  - every appearance of the tree (the hook project's, the documents'), written in the builder's form: the loader's
 *    verdict the same, the same page drawn on it pixel for pixel, the form written twice the same;
 *  - the operations of the forms: each leaves a text the loader takes; a colour renamed with every look that names
 *    it; a look given another draw keeps its fields of the same name and kind; the order of the entries never moved;
 *  - a colour typed as hex or as a name of DGX's, and as a panel of 16 and 18 bits shows it, the module's own pixel.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const ROOT = path.join(__dirname, '../..');
const DgxUiPreview = require(path.resolve(process.argv[2]));
const appearance = require(path.join(ROOT, 'editor/appearance.js'));
const description = require(path.join(ROOT, 'editor/description.js'));
const { History } = require(path.join(ROOT, 'editor/history.js'));
const OK = 0, KEY = 3, RANGE = 6, UNKNOWN = 8, MISSING = 15;
let failures = 0, checks = 0;
const check = (held, what) => {
    ++checks;
    if (!held) console.error('  failed: ' + what), ++failures;
    return held;
};
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const COLORS = { paper: '#102030', ink: '#e0f0ff', fill: '#204060', border: '#808080', accent: '#f0a03c' };
const textOf = (doc) => appearance.write(doc).text;
const docOf = (text) => description.read(text).doc;

(async () => {
    const preview = await DgxUiPreview.create({ log: () => {} });
    preview.screen(160, 120, 16, 1), preview.names(null);
    const format = preview.appearanceFormat();
    const judged = (doc) => preview.appearance(typeof doc === 'string' ? doc : textOf(doc));
    const base = () => ({ 'dgx-ui': 1, colors: { ...COLORS }, styles: { body: { fonts: ['small'] } } });
    const withLook = (look) => ({ ...base(), looks: { one: look } });

    /* what the module says, held to its loader */
    check(format.draws.length === 21 && format.by_rule, 'the library\'s twenty-one draws by the rule: ' + format.draws.length);
    for (const draw of format.draws) {
        // a field of pictures has no default of the library's: a picture by the rule, "card", of 65535 there
        const defaults = Object.fromEntries(Object.entries(draw.defaults ?? {}).map(([name, value]) =>
            [name, value === null && draw.fields.find((field) => field.name === name)?.type === 'picture' ? 'card' : value]));
        const look = draw.fields.length ? { draw: draw.draw, params: defaults } : { draw: draw.draw };
        const said = judged(JSON.stringify(withLook(look)));
        check(said.code === OK, `${draw.draw}: its library look's values taken: ${said.code} "${said.name}"`);
        for (const field of draw.fields) {
            const without = { ...look.params };
            delete without[field.name];
            const left = judged(JSON.stringify(withLook({ draw: draw.draw, params: without })));
            const must = field.type === 'color' || field.type === 'color_or_under' || field.type === 'picture';
            check(must ? left.code === MISSING && left.name === field.name : left.code === OK,
                  `${draw.draw}.${field.name} left out: ${left.code} "${left.name}"`);
            const at = (value) => judged(JSON.stringify(withLook({ draw: draw.draw, params: { ...look.params, [field.name]: value } }))).code;
            if (field.type === 'number') {
                check(at(field.min) === OK && at(field.max) === OK, `${draw.draw}.${field.name} at its bounds ${field.min} … ${field.max}`);
                check(at(field.min - 1) === RANGE && at(field.max + 1) === RANGE, `${draw.draw}.${field.name} past its bounds`);
            } else if (field.type === 'word') {
                check(field.words.every((word) => at(word) === OK) && at('nothing') !== OK, `${draw.draw}.${field.name}: its words alone`);
            } else if (field.type === 'color_or_under') {
                check(at(null) === OK && at('accent') === OK && at('nothing') === UNKNOWN, `${draw.draw}.${field.name}: null, a colour`);
            } else if (field.type === 'color') {
                check(at('accent') === OK && at(null) !== OK && at('nothing') === UNKNOWN, `${draw.draw}.${field.name}: a colour, never null`);
            } else if (field.type === 'bool' || field.type === 'bit') {
                check(at(true) === OK && at(false) === OK, `${draw.draw}.${field.name}: true and false`);
            } else if (field.type === 'bands') {
                check(at([[10, 'ink'], [20, 'accent']]) === OK && at([[10, 'nothing']]) === UNKNOWN, `${draw.draw}.${field.name}: bands`);
            } else if (field.type === 'picture') {
                check(at('another') === OK && at(null) !== OK && at(3) !== OK && field.min >= 1, `${draw.draw}.${field.name}: a picture by the rule, never null`);
            }
        }
        const unknown = judged(JSON.stringify(withLook({ ...look, params: { ...look.params, nothing: 1 } })));
        check(unknown.code === KEY || (!draw.fields.length && unknown.code !== OK), `${draw.draw}: a field it has not refused: ${unknown.code}`);
    }
    /* on a names file: a picture of the file's, of as many as the field needs; one of fewer, RANGE at the field; one not
       there, UNKNOWN; and the format lists the file's pictures with their counts */
    const nineLook = (pieces) => JSON.stringify({ ...base(), looks: { card: { draw: 'nine', params: { fill: 'fill', ink: 'ink', pieces } } } });
    check(preview.appearance(null, 'draw nine\nfont small TerminusTTFMedium12\npicture card card_pieces 9\npicture one card_pieces 8\n').code === 0,
          'an appearance on its own names file of pictures');
    const onNames = (pieces) => { const said = { ...preview.appearance(nineLook(pieces), 'draw nine\nfont small TerminusTTFMedium12\npicture card card_pieces 9\npicture one card_pieces 8\n') }; return said; };
    check(onNames('card').code === OK, 'a picture of nine there taken');
    const short = onNames('one');
    check(short.code === RANGE && short.name === 'pieces', 'a picture of eight where nine are needed: RANGE at pieces: ' + JSON.stringify(short));
    const missing = onNames('nothing');
    check(missing.code === UNKNOWN && missing.name === 'nothing', 'a picture not there: UNKNOWN: ' + JSON.stringify(missing));
    const listed = preview.appearanceFormat();
    check(same(listed.pictures, [{ name: 'card', count: 9 }, { name: 'one', count: 8 }]) &&
          listed.draws.find((one) => one.draw === 'nine').fields.find((field) => field.name === 'pieces').min === 9, 'the format: the pictures and the need');
    preview.appearance(null, null);
    const styleSaid = (style) => judged(JSON.stringify({ ...base(), styles: { body: style } })).code;
    for (const field of format.style) {
        if (field.type === 'word') check(field.words.every((word) => styleSaid({ fonts: ['small'], [field.name]: word }) === OK), `style.${field.name}`);
        if (field.type === 'flags') check(styleSaid({ fonts: ['small'], [field.name]: field.words }) === OK, `style.${field.name}`);
        if (field.type === 'number')
            check(styleSaid({ fonts: ['small'], [field.name]: field.max }) === OK && styleSaid({ fonts: ['small'], [field.name]: field.max + 1 }) === RANGE,
                  `style.${field.name} at and past ${field.max}`);
    }
    check(same(format.style.map((field) => field.name), ['fonts', 'align', 'flags', 'scroll_ms']), 'the style\'s fields');
    check(same(format.variant.map((field) => field.name), ['font', 'hand', 'height', 'narrow', 'joined', 'pen', 'soft', 'points']),
          'the variant\'s fields: ' + format.variant.map((field) => field.name));
    check(styleSaid({ fonts: [{ font: 'hand', hand: format.hands[0], height: 20, narrow: 100, joined: true, pen: 255, soft: true }] }) === OK,
          'a handwritten variant of every field');

    /* the same fields as the table of the documents: `name`; ° a colour that may be null; a number's bounds */
    const whole = fs.readFileSync(path.join(ROOT, 'docs/en/api.md'), 'utf8'), from = whole.indexOf('| Draw | Its parameters |');
    const api = from < 0 ? '' : whole.slice(from, whole.indexOf('\n\n', from));
    let rows = 0;
    for (const draw of format.draws) {
        const row = api.match(new RegExp(`^\\| \`"${draw.draw}"\` \\| (.*) \\|$`, 'm'));
        if (!check(row, `${draw.draw}: a row of the table of draws`)) continue;
        ++rows;
        const said = row[1] === 'none' ? [] : row[1].split('; ').map((one) => {
            const found = one.match(/^`(\w+)`(°?)(?: (-?\d+)…(-?\d+))?/);
            return found ? { name: found[1], under: !!found[2], min: found[3], max: found[4] } : { name: one };
        });
        check(same(said.map((one) => one.name), draw.fields.map((field) => field.name)), `${draw.draw}: the table's fields ${row[1]}`);
        for (const field of draw.fields) {
            const one = said.find((each) => each.name === field.name);
            if (!one) continue;
            check(one.under === (field.type === 'color_or_under'), `${draw.draw}.${field.name}: ° as it may be null`);
            if (field.type === 'number') check(+one.min === field.min && +one.max === field.max, `${draw.draw}.${field.name}: its bounds`);
        }
    }

    /* every appearance of the tree, written in the builder's form */
    const texts = [['test/hook/project/components/second/look.json', fs.readFileSync(path.join(ROOT, 'test/hook/project/components/second/look.json'), 'utf8')]];
    for (const file of ['docs/en/api.md', 'docs/ru/api.md', 'docs/en/cookbook.md', 'docs/ru/cookbook.md']) {
        for (const block of fs.readFileSync(path.join(ROOT, file), 'utf8').matchAll(/```json\n([^]*?)```/g)) {
            const said = description.read(block[1]);
            if (said.ok && description.valueOf(said.doc, 'colors')) texts.push([file, block[1]]);
        }
    }
    check(texts.length >= 4, 'the appearances of the tree found: ' + texts.length);
    let drawnAlike = 0;
    const drawn = () => {
        preview.paint(1000);
        return Buffer.from(preview.frame()).toString('base64');
    };
    for (const [file, text] of texts) {
        const doc = docOf(text), { text: written, spans: entrySpans } = appearance.write(doc);
        check(textOf(docOf(written)) === written, `${file}: written twice the same`);
        const wide = written.split('\n').filter((one) => one.length > description.ONE_LINE);
        check(!wide.length, `${file}: no line past ${description.ONE_LINE} columns: ${wide[0]}`);
        const bytes = Buffer.from(written);
        check(entrySpans.every((span) => bytes.subarray(span.start).toString().startsWith(JSON.stringify(span.path[1]) + ':') &&
                                         same(appearance.entryAtByte(entrySpans, span.start), span.path) &&
                                         same(appearance.entryAtByte(entrySpans, span.end - 1), span.path)),
              `${file}: each entry's span from its name to its end`);
        const before = { ...judged(text) }, after = { ...judged(written) };
        check(before.code === after.code && before.name === after.name, `${file}: the same verdict, ${before.code} and ${after.code}`);
        // a page of a widget of each look, of the first kind its draw is for, a text in each style
        const looks = appearance.namesOf(doc, 'looks'), styles = appearance.namesOf(doc, 'styles');
        const children = looks.map((name) => {
            const draw = description.shown(appearance.nodeAt(doc, ['looks', name, 'draw']));
            const kind = format.draws.find((one) => one.draw === draw)?.kinds[0];
            return kind === 'label' || kind === 'button' ? { kind, look: name, text: name } : kind ? { kind, look: name } : null;
        }).filter(Boolean).concat(styles.map((name) => ({ kind: 'label', text: name, style: name })));
        const page = JSON.stringify({ 'dgx-ui': 1, tree: { kind: 'box', flags: ['column'], children } });
        judged(text);
        const pageBefore = preview.page(page).code, pixelsBefore = drawn();
        judged(written);
        const pageAfter = preview.page(page).code, pixelsAfter = drawn();
        check(pageBefore === pageAfter && pixelsBefore === pixelsAfter, `${file}: the page drawn on it the same (${pageBefore})`);
        drawnAlike += pageBefore === OK;
    }
    check(drawnAlike >= 2, 'pages drawn on the appearances: ' + drawnAlike);

    /* the operations of the forms */
    const hook = texts[0][1];
    let doc = docOf(hook);
    const order = (one) => appearance.SECTIONS.map((section) => appearance.namesOf(one, section).join(','));
    const orderBefore = order(doc);
    appearance.setAt(doc, ['colors', 'accent'], '#00ff00');
    check(judged(doc).code === OK && textOf(doc).includes('"accent": "#00ff00"'), 'a colour set');
    const renamed = appearance.renameColor(doc, 'accent', 'highlight', format);
    check(renamed && renamed.changed === 2 && !renamed.own.length, 'a colour renamed: its two users: ' + JSON.stringify(renamed));
    check(judged(doc).code === OK && !textOf(doc).includes('accent'), 'every look that named it carried');
    check(same(appearance.namesOf(doc, 'colors'), ['paper', 'ink', 'fill', 'border', 'highlight']), 'renamed in its place');
    check(appearance.renameColor(doc, 'highlight', 'ink', format) === null, 'a rename to a name there refused');
    appearance.setAt(doc, ['looks', 'meter'], { draw: 'level', params: { ...format.draws.find((one) => one.draw === 'level').defaults } });
    appearance.insertAt(doc, ['looks', 'meter', 'params', 'bands'], 0, [50, 'highlight']);
    appearance.insertAt(doc, ['looks', 'meter', 'params', 'bands'], 1, [80, 'ink']);
    check(judged(doc).code === OK, 'a look added, two bands');
    check(appearance.colorUsers(doc, 'highlight', format).users.some((user) => same(user.path, ['looks', 'meter', 'params', 'bands', 0, 1])),
          'a band names its colour');
    appearance.moveAt(doc, ['looks', 'meter', 'params', 'bands'], 0, 1);
    check(judged(doc).code === OK && same(description.shown(appearance.nodeAt(doc, ['looks', 'meter', 'params', 'bands'])), [[80, 'ink'], [50, 'highlight']]),
          'a band moved');
    appearance.setAt(doc, ['styles', 'body', 'fonts', 1], { font: 'hand', hand: 'handwriting', height: 18 });
    appearance.moveAt(doc, ['styles', 'body', 'fonts'], 1, -1);
    check(judged(doc).code === OK && description.shown(appearance.nodeAt(doc, ['styles', 'body', 'fonts', 1])) === 'small', 'a variant added and moved first');
    appearance.removeAt(doc, ['colors', 'highlight']);
    const gone = judged(doc);
    check(gone.code === UNKNOWN, 'a colour removed while looks name it: the loader refuses: ' + gone.code);
    check(same(order(docOf(hook)).slice(1), orderBefore.slice(1)) && order(doc)[2] === 'pane,slider,meter', 'no entry moved: ' + order(doc));

    /* a draw changed: its fields of the same name and kind kept, the others its defaults */
    doc = docOf(hook);
    const switchDraw = format.draws.find((one) => one.draw === 'switch'), sliderDraw = format.draws.find((one) => one.draw === 'slider');
    const params = appearance.nodeAt(doc, ['looks', 'slider', 'params']);
    const kept = sliderDraw.fields.filter((field) => switchDraw.fields.some((one) => one.name === field.name && one.type === field.type));
    check(kept.map((field) => field.name).join() === 'track,fill,knob,focused,disabled', 'slider and switch share track, fill, knob, focused, disabled');
    const next = { draw: 'switch', params: {} };
    for (const field of switchDraw.fields) next.params[field.name] = kept.some((one) => one.name === field.name)
        ? description.shown(description.valueOf(params, field.name)) : switchDraw.defaults[field.name];
    appearance.setAt(doc, ['looks', 'slider'], next);
    check(judged(doc).code === OK && next.params.disabled === 'border' && next.params.track === 'border' && next.params.fill === 'accent', 'a draw changed, taken');

    /* the hand-written form kept; the builder's form written of it */
    const hookWritten = textOf(docOf(hook));
    check(hookWritten !== hook && hookWritten.startsWith('{\n  "dgx-ui": 1,\n  "name": "theme",\n  "colors": {\n    "paper": "#12161e",\n'),
          'the builder\'s form: ' + hookWritten.slice(0, 80));
    const spans = appearance.write(docOf(hook)).spans;
    check(same(spans.map((span) => span.path.join('.')), ['colors.paper', 'colors.ink', 'colors.fill', 'colors.border', 'colors.accent', 'styles.body',
                                                          'looks.pane', 'looks.slider']), 'a span of each entry');
    const brokenLook = hookWritten.replace('"knob": "ink"', '"knob": "nothing"');
    const refused = judged(brokenLook);
    check(refused.code === UNKNOWN && same(appearance.entryAtByte(appearance.write(docOf(brokenLook)).spans, refused.offset), ['looks', 'slider']),
          'a refusal found in its entry');

    /* an entry of a section that would pass the columns after its name, by a few: broken, every line within them */
    const near = { 'dgx-ui': 1, colors: { ...COLORS }, looks: { wide_look_name: { draw: 'frame', fill: 'fill', params: { border: 'accent', width: 1 } } } };
    const nearLength = () => description.line(description.plain(near.looks.wide_look_name)).length;
    near.looks.wide_look_name.params.border = 'accent'.padEnd(6 + description.ONE_LINE - 10 - nearLength(), 'x'); // 10 short of the columns alone
    check(nearLength() === description.ONE_LINE - 10, 'the entry ten columns short alone: ' + nearLength());
    const nearText = textOf(docOf(JSON.stringify(near)));
    check(nearText.split('\n').every((one) => one.length <= description.ONE_LINE) && nearText.includes('"wide_look_name": {\n'),
          'an entry that passes the columns after its name broken: ' + nearText.split('\n').map((one) => one.length));

    /* a step of several files: recorded with its link, never folded into the typing before it */
    const history = new History();
    history.record({ text: 'a' }, 'typing', 1000);
    history.record({ text: 'b' }, 'typing', 1100, 7);
    check(history.past.length === 2 && history.undoLink === 7 && history.past[1].link === 7, 'a linked step not folded: ' + JSON.stringify(history.past));
    history.record({ text: 'c' }, 'typing', 1200);
    check(history.past.length === 3 && history.undoLink === null, 'typing after it a step of its own');

    /* a colour typed, and as a panel shows it */
    const names = DgxUiPreview.colorNames();
    check(names.length === 149 && same(names[0], ['black', '#000000']), 'DGX\'s names: ' + names.length);
    check(appearance.colorOf('Gainsboro', names) === '#dcdcdc' && appearance.colorOf('DCDCDC', names) === '#dcdcdc' &&
          appearance.colorOf('#F0A03C', names) === '#f0a03c' && appearance.colorOf('dark slate blue', names) === '#483d8b' &&
          appearance.colorOf('nothing', names) === null && appearance.colorOf('#12345', names) === null, 'a colour typed');
    check(appearance.namesOfColor('#808080', names) === 'gray / grey' && appearance.namesOfColor('#dcdcdc', names) === 'gainsboro' &&
          appearance.namesOfColor('#f0a03c', names) === '', 'its names');
    for (const depth of [16, 18, 24]) {
        preview.screen(32, 32, depth, 1);
        let apart = 0;
        for (const hex of ['#f0a03c', '#12161e', '#dcdcdc', '#7f7f7f', '#ffffff', '#010203', '#fe8001']) {
            preview.appearance(JSON.stringify({ ...base(), colors: { ...COLORS, fill: hex },
                                                looks: { pane: { draw: 'frame', fill: 'fill', params: { border: 'fill', width: 1 } } } }));
            preview.page(JSON.stringify({ 'dgx-ui': 1, tree: { kind: 'frame', look: 'pane' } }));
            preview.paint(2000);
            const pixel = preview.frame().slice(4 * (16 * 32 + 16), 4 * (16 * 32 + 16) + 3);
            const shown = '#' + [...pixel].map((part) => part.toString(16).padStart(2, '0')).join('');
            if (!check(shown === appearance.colorAtDepth(hex, depth), `${hex} at ${depth} bits: the panel ${shown}, the builder ${appearance.colorAtDepth(hex, depth)}`))
                ++apart;
        }
        check(!apart, `at ${depth} bits every colour as the panel shows it`);
    }

    if (failures) {
        console.error(`test_appearance.js: ${failures} of ${checks} checks failed`);
        process.exit(1);
    }
    console.log(`test_appearance.js: ${checks} checks: ${format.draws.length} draws held to the loader and to the table of ${rows} rows, ` +
                `${texts.length} appearances written alike, the operations, the colours`);
})().catch((error) => {
    console.error('test_appearance.js:', error);
    process.exit(1);
});
